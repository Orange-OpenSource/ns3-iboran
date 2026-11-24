# Application layer

This document describes the application layer that carries the UE payloads: how a payload is typed and ranked by content criticality, wrapped in an A2A envelope, cut into packets and rebuilt at the receiver, and what a run records about it. Paths are relative to `contrib/iboran`.

## Overview

Each UE has one flow per enabled link direction, with a transmission application at the sender and a reception application at the receiver. On the downlink, the remote host sends to the UE; on the uplink, the UE sends to the remote host. At start, each transmission application sends its input file once, as one message.

A message goes through four steps:

1. The transmitter detects the content type of the payload and ranks its bytes by criticality.
2. With `appEnvelope` other than `None`, it wraps the payload in an A2A request, as an A2A client agent would send it.
3. It cuts the result into packets, each with a 44-byte header that lets the receiver place the packet on its own.
4. The receiver places the packets, reads the envelope, assigns task and context IDs, fills the lost parts of the payload and saves the result for the KPI step.

## Content types

The data format is set once per run with `--data_format`. It decides which file of a sample is loaded, the QCI (LTE) or 5QI (NR) of the dedicated bearer, the extension of the saved output and the KPI module that scores it. The application then detects the content type of the loaded file, which decides the element size, the content criticality levels and how lost data is filled.

| Data format | Content type | Detected from | Element size (bytes) |
|---|---|---|---|
| `IMAGE` | `PNG` | PNG signature | 1 |
| `IMAGE` | `JPEG` | JPEG signature | 1 |
| `IMAGE` | `BMP` | `BM` signature, with `.bmp` or `IMAGE` | 1 |
| `VIDEO` | `H264_ANNEXB` | Start code, with `.h264`, `.264` or `VIDEO` | 1 |
| `VIDEO` | `MP4` | `ftyp` box | 1 |
| `EMBEDDING` | `EMBEDDING_F32` | `.f32`, or `EMBEDDING` | 4 |
| `EMBEDDING` | `EMBEDDING_F16` | `.f16` | 2 |
| `EMBEDDING` | `EMBEDDING_U8` | `.u8`, `.i8`, `.pq`, `.codes` | 1 |
| `TEXT` | `TEXT_UTF8` | `.txt`, `.md`, `.csv`, or `TEXT` | 1 |
| `TEXT` | `JSON` | `.json` | 1 |

A run loads the input file with the dataset's `input_image_format` for `IMAGE`, and `.f32`, `.mp4` or `.txt` for the other formats. A dataset folder can hold several files of one sample (for example `.jpg` and `.f32`), and the data format selects one.

> [!NOTE]
> A file whose size is not a whole number of elements is rejected, and a file whose detected type belongs to another format triggers a warning. Embeddings and text have no signature, so a float16 file named `.f32` is read as float32.

## Criticality

Criticality ranks payload bytes from 1, the most critical, to 10, the default. The MAC schedulers can use it to serve the most critical traffic first (see [algorithms](algorithms.md#dapp-ue-selection)).

| Content type | Part | Level |
|---|---|---|
| `PNG` | Signature | 10 |
| `PNG` | IHDR | 1 |
| `PNG` | PLTE, first IDAT | 2 |
| `PNG` | Later IDATs | Decaying from 3 to 9 |
| `PNG` | IEND | 7 |
| `PNG` | Other chunks | 9 |
| `JPEG` | SOI, DQT, DHT, DRI, SOS header, SOFn | 1 |
| `JPEG` | First scan | 2 |
| `JPEG` | Later scans (progressive) | Decaying from 3 to 9 |
| `JPEG` | EOI | 7 |
| `JPEG` | APPn, COM, other segments | 9 |
| `H264_ANNEXB` | SPS, PPS | 1 |
| `H264_ANNEXB` | IDR slice | 2 |
| `H264_ANNEXB` | Non-IDR slice | 5 |
| `H264_ANNEXB` | SEI, access unit delimiter | 9 |
| `H264_ANNEXB` | Other NAL units | 8 |
| Embeddings | All dimensions, by default | 10 |
| Other types | All bytes | 10 |

Decaying levels give 3 to the first later IDAT or scan, 4 to the next two, 5 to the next four, and so on up to 9. A baseline JPEG has one scan, so its whole image data is level 2.

## Envelope models

`appEnvelope` wraps each message as one of the three transport bindings of A2A v1.0 would, or not at all.

| Model | Transport | Payload on the wire |
|---|---|---|
| `None` | - | As is |
| `A2A-JSONRPC` | HTTP/1.1, JSON-RPC `SendMessage` | Base64 |
| `A2A-HTTPJSON` | HTTP/1.1, `POST /message:send` | Base64 |
| `A2A-gRPC` | HTTP/2, gRPC `SendMessage` | As is |

The JSON envelopes carry the payload in base64, which writes every 3 bytes as 4 characters: a 3,072-byte embedding becomes 4,096 characters. The receiver decodes a group of 4 characters only if all 4 arrived. The gRPC envelope carries the payload as raw bytes.

> [!NOTE]
> The envelopes model request traffic only. The receiver sends no response, and streaming, TLS and agent card retrieval are not modeled. A2A runs over HTTP and TCP; with an envelope over UDP, the application warns.

### Envelope sizes

Float32 embeddings sent as the input file, in 512-byte packets:

| Model | 768 dims: bytes on the wire | 768 dims: packets | 768 dims: bytes sent | 1,024 dims: bytes on the wire | 1,024 dims: packets | 1,024 dims: bytes sent |
|---|---|---|---|---|---|---|
| `None` | 3,072 | 7 | 3,380 | 4,096 | 9 | 4,492 |
| `A2A-JSONRPC` | 4,508 | 10 | 4,948 | 5,876 | 13 | 6,448 |
| `A2A-HTTPJSON` | 4,450 | 10 | 4,890 | 5,818 | 13 | 6,390 |
| `A2A-gRPC` | 3,244 | 7 | 3,552 | 4,268 | 10 | 4,708 |

Bytes sent include the packet headers. Most of the JSON overhead is base64. For small messages, the fixed envelope text dominates: 32 bytes (8 float32 dimensions) become about 455 bytes with `A2A-JSONRPC` and 201 with `A2A-gRPC`.

## Packets

Each packet starts with a 44-byte header that repeats the message ID, the sizes and the position of the packet's slice, so the receiver can place any packet on its own, even when the first one is lost. The packet size and the bit rate of a link include this header: a 512-byte packet carries 468 bytes of the message. Over UDP, packets are capped at 1,472 bytes to avoid IP fragmentation.

Cuts inside the payload fall on whole elements, or on groups of 3 elements under base64, so a lost packet removes whole embedding dimensions only. No packet is reserved for the envelope: its start shares the first packet with the first payload elements, and its end shares the last packet with the last ones. Losing either packet loses those elements as well as the envelope.

> [!WARNING]
> With `appFraming: false` and `appEnvelope: None`, no header is sent and the receiver concatenates what arrives. Lost bytes then shift the rest of the payload and cannot be located. Any envelope other than `None` uses the header.

## Transmission

The receivers run from 1 s to `simTime` − 1 s and the transmitters from 1.1 s to `simTime` − 1.1 s. Each transmitter sends its input file once, as one message, starting 50 ms after it starts over UDP, or 50 ms after the TCP connection is set up. Packets are paced at the link bit rate. The application publishes the object ID of its payload as the content ID of the UE, and publishes it again when the RNTI changes, for example after a handover. It stops sending when its file is sent.

## Reception

The receiver places each packet in its message. A message is delivered when every byte has arrived, or when the receiver stops. On delivery, the receiver reads the envelope, assigns task and context IDs (see [task and context IDs](#task-and-context-ids)), removes the envelope, decodes the payload and fills each element that lost any byte according to `appFillPolicy`:

| Policy | Lost element becomes |
|---|---|
| `Auto` | Same as `Nan` |
| `Zero` | Zero bytes |
| `Nan` | NaN for float embeddings, zero bytes otherwise |
| `Drop` | Removed |

Except with `Drop`, a received embedding therefore keeps its full length, with NaN at the lost dimensions, so `FidelityScore` compares aligned dimensions (see [KPIs](kpis.md#intent-based-kpis)).

| Output | Written when | Content |
|---|---|---|
| `<output_pattern>` with the output extension | The input file of the first sender was received, in whole or in part | That file |
| `<output_pattern>_m<messageId>` with the output extension | `appSaveAllMessages: true` | Each other message of the first sender |
| `<output_pattern>_s<k>_m<messageId>` with the output extension | `appSaveAllMessages: true` | Each message of sender *k*, numbered from 1 in order of arrival |
| `notebook/logs/appMessages.json` | `appSaveMessageLog: true` | Per link and UE: what was sent and received (see [message log](#message-log)) |

The output extension is the dataset's `input_image_format` for `IMAGE` (for example `.jpg`) and the default extension of the format otherwise (`.f32`, `.mp4`, `.txt`), as the KPI step expects. When the input file is lost entirely, no output is written and the KPI step skips that UE. Outputs are written when the receivers stop (see [simulation data](simulation-data.md#outputs)).

> [!WARNING]
> A run with `appSaveMessageLog: false` deletes the `appMessages.json` of an earlier run. Copy the file to keep it.

## Task and context IDs

The receiver acts as the A2A server agent. For each message, it keeps the task and context IDs the sender set, or assigns its own, and records them in the message log. It sends no response, so the sender never learns the IDs it did not set.

| Identifier | Meaning | Set by |
|---|---|---|
| Message ID | One message of a sender | The sender, counting from 1 |
| Task ID | Messages of one unit of work | The sender, or the receiver if the sender set none |
| Context ID | A conversation, possibly several tasks | The sender, or the receiver if the sender set none |
| Round | Round of a message, 0 for the input file | The sender |

The input file carries no task or context ID, so the receiver assigns both:

| ID | Sender set it | Otherwise |
|---|---|---|
| Task | `t-<n>` as sent | `<prefix>-t-<n>`: a new task per message, *n* counting from 1 in the order messages start to arrive at this receiver |
| Context | As sent | `<prefix>-ctx-<k>`: one context per sender, *k* the sender's index plus 1 |

`<prefix>` is `dl-ue<i>` or `ul-ue<i>` for UE *i* from 1, so assigned IDs are unique within a run. The IDs come from counters, so a run repeated with the same `--rng_run` gets the same IDs. The downlink input file of UE 2, for example, gets task `dl-ue2-t-1` and context `dl-ue2-ctx-1`.

The envelope is valid when all of its bytes arrived and it matches the packet headers. Otherwise the message log shows the envelope as `Unknown`, and a context ID set by the sender is lost, while a task ID set by the sender is kept, since the packet headers also carry it. The payload is saved either way.

## Message log

With `appSaveMessageLog: true`, `notebook/logs/appMessages.json` holds, per link (`dl`, `ul`) and UE index from 1, the object ID, the output file name, and two records.

| Record | Field | Meaning |
|---|---|---|
| `transmission` | `envelope`, `packetsSent`, `bytesSent` | Envelope model, packets and bytes handed to the socket |
| `transmission` | `messages` | One entry per message whose transmission started |
| `reception` | `framing`, `packetsReceived`, `fragmentsReceived`, `bytesReceived`, `invalidFragments`, `lateFragments` | Reception counts. `packetsReceived` counts socket reads: datagrams over UDP, but over TCP one read can hold several packets. `fragmentsReceived` counts packets with a valid header. |
| `reception` | `messages` | One entry per delivered message |

| Sent message field | Meaning |
|---|---|
| `messageId` | Message ID |
| `taskId`, `contextId` | IDs set by the sender, `null` if none |
| `round`, `file`, `contentType` | Round, whether it is the input file, content type |
| `payloadSize`, `wireSize` | Payload bytes, and bytes on the wire with the envelope |
| `fragmentsSent`, `sentAll` | Packets handed to the socket, and whether that was all of them |
| `firstTxNs`, `lastTxNs` | Simulation time of the first and the latest packet, in ns |

| Received message field | Meaning |
|---|---|
| `sender`, `from`, `messageId` | Sender index and address, message ID |
| `round`, `file`, `contentType` | Round, whether it is the input file, content type |
| `payloadSize`, `wireSize`, `wireBytesReceived`, `fragmentsReceived` | Sizes and counts |
| `elementsTotal`, `elementsReceived`, `complete` | Elements and completeness |
| `missingRanges` | Payload byte ranges `[start, end)` of the elements not received, in offsets of the original payload |
| `envelope`, `envelopeValid` | Model read from the envelope (`None`, `Unknown` if unreadable), and whether it was valid (`false` with `appEnvelope: None`) |
| `taskId`, `taskIdAssigned`, `contextId`, `contextIdAssigned` | IDs held by the receiver, and whether it assigned them |
| `role`, `mediaType`, `metadata` | Read from a valid envelope, empty otherwise |
| `outputFile` | File the message was saved to, `null` if not saved |
| `txTimeNs`, `firstRxNs`, `lastRxNs`, `latencyMs` | Preparation time, first and last packet received, and the time from preparation to the last packet |

A received message and its sent record share `messageId` within one link and UE. A sent message with no received entry was lost entirely. `lastRxNs` minus `firstTxNs` is the time from the first packet sent to the last packet received.

> [!NOTE]
> With `appFillPolicy: Drop`, the elements in `missingRanges` are absent from the saved file, so later offsets shift.

## Configuration

Keys under `common` in [`iboranConfig.yaml`](../model/iboran_campaign/iboranConfig.yaml). They have no command-line parameter. The defaults apply when a key is absent.

| Key | Default | Effect |
|---|---|---|
| `appDlEnable`, `appUlEnable` | `true` | Downlink and uplink flows |
| `appDlPacketSize`, `appUlPacketSize` | 512 | Packet size in bytes, header included |
| `appDlBitRate`, `appUlBitRate` | 2000000, 1000000 | Bit rate in bps, header included |
| `appDlTransport`, `appUlTransport` | `TCP` | `UDP` or `TCP` |
| `appFraming` | `true` | 44-byte header on every packet |
| `appEnvelope` | `None` | `None`, `A2A-JSONRPC`, `A2A-HTTPJSON`, `A2A-gRPC` |
| `appFillPolicy` | `Auto` | `Auto`, `Zero`, `Nan`, `Drop` |
| `appSaveAllMessages` | `false` | Save messages other than the input file |
| `appSaveMessageLog` | `false` | Write `notebook/logs/appMessages.json` |

> [!WARNING]
> After changing an `app*` key, delete the [cached results](simulation-data.md#cached-results).

## Limits

- The receiver sends no response, so a later message cannot name a task the receiver created.
- The receiver records no task outcome; the orchestrator judges it from the saved files and the message log.
- Text is cut on bytes, so a multi-byte UTF-8 character can be split. The JSON envelopes send text and JSON in base64, where A2A would send them as `text` or `data` parts.
- Content criticality is published when a packet is handed to the socket, not when the MAC serves it.
- `MP4`, `BMP`, text and raw payloads have no criticality map, so all their bytes are level 10.
