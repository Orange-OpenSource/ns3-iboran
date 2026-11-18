# Application layer

This document describes the code of the application layer: its files and interfaces, the envelope and packet formats, and a worked example down to the bytes. It is meant for changing the code or decoding packets by hand. For the behavior, the outputs and the configuration of a run, see [application layer](../../doc/application.md). Paths are relative to `contrib/iboran`.

## Files

| File | Role | Depends on ns-3 |
|---|---|---|
| `iboran-app-content.*` | Data formats, content types, type detection, element sizes, criticality maps | No |
| `iboran-app-message.*` | Fragment header, messages, reassembly, fill policies, base64, fragment alignment | No |
| `iboran-app-envelope.*` | Envelope models, envelope parsing and the mapping of criticality onto the wire | No |
| `iboran-app-header.*` | ns-3 `Header` wrapper of the fragment header | Yes |
| `iboran-application.*` | Transmission and reception applications | Yes |
| `iboran-side-info-tracker.*` | Latest criticality and content ID per RNTI and link, read by the MAC schedulers | Yes |

[`iboran-ran-common.cc`](../iboran_ran_environment/iboran-ran-common.cc) creates one transmission and one reception application per UE and link direction, loads the input file, sets the receiver `IdPrefix` to `dl-ue<i>` or `ul-ue<i>` and applies the `app*` keys of the configuration.

A payload goes through four objects:

| Object | Content | Count |
|---|---|---|
| Message | Payload, IDs, round, content type, metadata and criticality map, in memory | One per input file or `SendMessage()` call |
| Envelope | Protocol text or bytes around the payload (HTTP and JSON, or HTTP/2 and protobuf) | One per message |
| Wire message | Envelope prefix, encoded payload, envelope suffix | One per message |
| Fragment header | 44 bytes in front of each slice of the wire message | One per packet |

With `appEnvelope: None`, the wire message is the payload itself.

## Attributes and traces

| ns-3 attribute | Application | Default | Set from | Effect |
|---|---|---|---|---|
| `PacketSize` | Transmission | 1024 | `app*PacketSize` | Packet size in bytes, fragment header included |
| `BitRateBps` | Transmission | 1000000 | `app*BitRate` | Pacing rate in bps, fragment header included |
| `MinPacketInterval` | Transmission | 0 s | 8 × `app*PacketSize` ÷ `app*BitRate` | Lower bound on the time between packets |
| `UseUdp` | Both | `true` | `app*Transport` | UDP instead of TCP; must match on both sides |
| `EnableFraming` | Both | `true` | `appFraming`, or any envelope at the receiver | Fragment header on every packet; must match on both sides |
| `Envelope` | Transmission | `None` | `appEnvelope` | Envelope model; any model other than `None` turns framing on |
| `MaxDatagramSize` | Transmission | 1472 | | Upper bound on `PacketSize` over UDP |
| `RntiRetryDelayMs` | Transmission | 50 | | Delay before the first packet |
| `ContextId` | Transmission | Empty | | Context ID of messages that set none |
| `GrpcHeaderBytes` | Transmission | 64 | | Size of the HTTP/2 HEADERS frame in `A2A-gRPC` |
| `FillPolicy` | Reception | `Auto` | `appFillPolicy` | Fill of lost elements |
| `MessageTimeout` | Reception | 0 s | | Delivery of an incomplete message after its first fragment; 0 waits until the application stops |
| `SaveAllMessages` | Reception | `false` | `appSaveAllMessages` | Save messages other than the input file |
| `IdPrefix` | Reception | Empty | `dl-ue<i>` or `ul-ue<i>` | Prefix of the task and context IDs the receiver assigns |

An empty "Set from" cell means the environment leaves the default.

| Trace | Application | Fired when |
|---|---|---|
| `Tx` | Transmission | A packet is handed to the socket |
| `TxMessage` | Transmission | The last packet of a message is handed to the socket |
| `Rx` | Reception | A datagram or TCP segment is received |
| `RxMessage` | Reception | A message is delivered, with the sender address |

## Loading payloads

| Function | Source | Detection | Use |
|---|---|---|---|
| `LoadData(stem, format)` | Input stem | As `LoadFile` | Every run; adds the extension set with `SetInputFileExtension()`, otherwise the dataset's `input_image_format` for `IMAGE`, and `.mp4`, `.f32`, `.txt` or `.bin` for the other formats |
| `LoadFile(path, format)` | File on disk | Magic bytes, extension, format | Called by `LoadData`; rejects a size that is not a whole number of elements |
| `LoadDataFromBuffer(data, size, format)` | Bytes in memory | Magic bytes, format | Code only; no size check |
| `SendMessage(message)` | An `IbOranMessage` | None; the caller sets the content type and criticality | Code only; any number of messages |

`DetectContentType` checks the magic bytes first, then the extension, then the data format. Anything it does not recognize is `RAW` under the `BINARY` format, which has no `--data_format` value and covers payloads passed from code. The receiver takes the content type from the fragment header and never detects it.

| Content type | Signature | Media type in the envelope |
|---|---|---|
| `PNG` | `89 50 4E 47 0D 0A 1A 0A` | `image/png` |
| `JPEG` | `FF D8 FF` | `image/jpeg` |
| `BMP` | `BM` | `image/bmp` |
| `H264_ANNEXB` | Start code `00 00 01` or `00 00 00 01` before each NAL unit | `video/h264` |
| `MP4` | `ftyp` at byte 4 | `video/mp4` |
| `EMBEDDING_F32` | None | `application/vnd.iboran.embedding+f32` |
| `EMBEDDING_F16` | None | `application/vnd.iboran.embedding+f16` |
| `EMBEDDING_U8` | None | `application/vnd.iboran.embedding+u8` |
| `TEXT_UTF8` | None | `text/plain` |
| `JSON` | None | `application/json` |
| `RAW` | None | `application/octet-stream` |

## Criticality maps

A criticality map is a list of byte ranges of the payload with a level. `IbOranCriticalityMap::Get(offset, length)` returns the lowest level among the ranges that overlap a byte range, or 10 if none does. Decaying levels of later PNG IDATs and JPEG scans follow min(9, 3 + ⌊log₂(i + 1)⌋), where *i* counts them from 0.

`MapCriticalityToWire` maps the payload levels onto the wire message: the envelope prefix and suffix are level 1, and payload ranges scale by 4/3 under base64. Before each packet is sent, the transmitter writes the level of its slice into the fragment header and publishes it to `SideInfoTracker` for the UE's RNTI and link.

## Identifiers

| Identifier | Type | Fragment header | Envelope | Set by |
|---|---|---|---|---|
| `messageId` | `uint32_t` | Yes | `"messageId":"m-<n>"`, protobuf field 1 | Per-sender counter from 1, unless the caller sets it |
| `taskId` | `uint32_t`, 0 = none | Yes | `"taskId":"t-<n>"`, protobuf field 3 | The caller of `SendMessage()`, 0 for the file; otherwise the receiver |
| `contextId` | String, empty = none | No | `"contextId":"<id>"`, protobuf field 2 | The caller, or the `ContextId` attribute, empty for the file; otherwise the receiver |
| `round` | `uint16_t` | Yes | In metadata | The caller; 0 for the file |

The file message takes the next counter value at start and is queued ahead of any message queued earlier with `SendMessage()`. A caller-chosen `messageId` moves the counter past it, and an ID below the counter triggers a warning, since it may repeat an earlier one. The receiver identifies a message by its sender address and `messageId`, so senders that share a receiver may use the same IDs.

The receiver assigns IDs in `AssignA2aIds`. It reserves a task number when the first fragment of a message arrives, so the numbers follow the order in which messages start to arrive, not the order in which they complete. Uplink of UE 1, with `A2A-JSONRPC`: a follow-up queued with `SendMessage()` before start, with `taskId` 9 and `contextId` `ctx-a`, and the file. The follow-up takes message ID 1 when queued and the file takes ID 2 at start, but the file is sent first:

| Message | `taskId` | Assigned | `contextId` | Assigned |
|---|---|---|---|---|
| 2, the file | `ul-ue1-t-1` | Yes | `ul-ue1-ctx-1` | Yes |
| 1, the follow-up | `t-9` | No | `ctx-a` | No |

## Envelopes

| Model | Operation | Payload encoding | Length information |
|---|---|---|---|
| `A2A-JSONRPC` | `POST /a2a/jsonrpc`, `"method":"SendMessage"` | Base64 in a `raw` part | `Content-Length` |
| `A2A-HTTPJSON` | `POST /message:send` | Base64 in a `raw` part | `Content-Length` |
| `A2A-gRPC` | RPC in the HEADERS frame | Raw protobuf `bytes` | DATA frame length, gRPC prefix and a length per protobuf level |

The JSON envelopes are real HTTP/1.1 text with a correct `Content-Length` and a 32-character placeholder bearer token. HTTP+JSON is the JSON-RPC body without the `jsonrpc`, `id`, `method` and `params` wrapper. In the gRPC model, the DATA frame header, the gRPC prefix and the protobuf lengths are exact, the HEADERS frame is `GrpcHeaderBytes` zero bytes, and the protobuf field numbers are illustrative. A payload above 16 KiB adds one 9-byte DATA frame header per extra 16 KiB.

Base64 applies to the payload only, and only in the JSON envelopes. Two floats 1.0 and 2.0 (`00 00 80 3F 00 00 00 40`) become `AACAPwAAAEA=`. The `A2A-JSONRPC` prefix of a file message with ID 1 is 302 bytes: 178 of HTTP request line and headers, 59 of JSON-RPC wrapper and 65 of A2A message and part fields.

`ParseEnvelope(prefix, suffix, payloadSize, model, context)` detects the model from the bytes: an HTTP/1.1 `POST` to `/a2a/jsonrpc` or `/message:send`, or an HTTP/2 DATA frame carrying a gRPC message. It checks the declared lengths against the payload size and reads the message ID, task ID, context ID, role, media type and metadata, with a minimal JSON reader and a protobuf reader. The envelope is valid when every byte of the prefix and the suffix arrived, it parses, and its message and task IDs match the fragment header. Otherwise the model is `Unknown`.

## Fragment header

Big-endian, 44 bytes, in front of every slice.

| Field | Bytes | Meaning | Varies per packet |
|---|---|---|---|
| `magic`, `version` | 1 + 1 | `0xB0`, 1 | No |
| `flags` | 1 | `FIRST` (0x01), `LAST` (0x02), `ENVELOPE` (0x04), `FILE` (0x08, the input file) | `FIRST`, `LAST` |
| `contentType` | 1 | Content type | No |
| `encoding` | 1 | 0 identity, 1 base64 | No |
| `criticality` | 1 | Lowest level in the slice | Yes |
| `round` | 2 | Round, 0 for the file | No |
| `messageId`, `taskId` | 4 + 4 | Identifiers | No |
| `payloadSize`, `wireSize` | 4 + 4 | Decoded payload, whole wire message | No |
| `offset` | 4 | Position of the slice in the wire message | Yes |
| `fragmentIndex`, `fragmentLength` | 2 + 2 | Index and size of the slice | Yes |
| `prefixLength`, `suffixLength` | 2 + 2 | Envelope bytes before and after the payload | No |
| `txTimeNs` | 8 | Time the message was prepared, in ns of simulation time | No |

34 of the 44 bytes are the same in every packet of a message, as `flags`, `criticality`, `offset`, `fragmentIndex` and `fragmentLength` change.

## Fragmentation

The transmitter keeps a cursor in the wire message. For each packet, it proposes an end `min(offset + PacketSize − 44, wireSize)`, moves it back to a safe boundary with `AlignFragmentEnd`, sends the slice with its header, and advances the cursor once the socket accepts the packet. The next packet follows after max(`MinPacketInterval`, packet bits ÷ `BitRateBps`).

Inside the payload, cuts fall on multiples of the alignment unit, counted from the start of the payload:

| Element size | Identity | Base64 |
|---|---|---|
| 1 byte (images, video, text, 1-byte codes) | No alignment | 4 characters (3 bytes) |
| 2 bytes (float16) | 2 bytes | 8 characters (3 values) |
| 4 bytes (float32) | 4 bytes | 16 characters (3 values) |

Alignment can leave up to one unit minus one byte unused per packet. The prefix shares the first packet with the start of the payload and the suffix shares the last one, unless the prefix is longer than a slice or the suffix does not fit.

For a 1,024-dimension float32 embedding with `A2A-JSONRPC`, the payload runs from wire byte 302 to 5,766:

| Packet | Proposed end | Aligned end | Slice |
|---|---|---|---|
| 0 | 468 | 302 + ⌊166/16⌋ × 16 = 462 | Prefix and 160 characters (30 floats) |
| 1 | 930 | 302 + ⌊628/16⌋ × 16 = 926 | 464 characters (87 floats) |
| 2–11 | | | 464 characters each |
| 12 | 6,034, capped at 5,876 | 5,876 | Last characters and the 110-byte suffix |

## Reassembly

The receiver parses each UDP datagram, or rebuilds fragment boundaries from the TCP byte stream, and places each slice at its offset in the message with the same sender and `messageId`. A message is delivered when every byte has arrived, when its `MessageTimeout` expires, or when the application stops. `Finalize` then parses the envelope, assigns IDs, removes the envelope by its lengths, decodes base64 and applies the fill policy to every element with a missing byte. A fragment with an invalid header counts in `invalidFragments`, and one for a message already delivered in `lateFragments`.

## Worked example

A message queued with `SendMessage()`, with `PacketSize` set to 160 (116 bytes of slice) and `GrpcHeaderBytes` to 16 for readability:

| Field | Value |
|---|---|
| `messageId`, `taskId`, `contextId` | 3, 7, `ctx-42` |
| `contentType` | `EMBEDDING_F32` |
| Payload | 16 floats, 64 bytes: `00 00 80 3c 00 00 00 3d …` |
| Metadata | `link`: `UL`, `objectId`: `12`, `round`: `0` |

| Model | Prefix | Payload on the wire | Suffix | Wire message | Packets | Bytes sent |
|---|---|---|---|---|---|---|
| `None` | 0 | 64 | 0 | 64 | 1 | 108 |
| `A2A-JSONRPC` | 337 | 88 | 110 | 535 | 5 | 755 |
| `A2A-HTTPJSON` | 280 | 88 | 109 | 477 | 5 | 697 |
| `A2A-gRPC` | 58 | 64 | 78 | 200 | 2 | 288 |

`A2A-JSONRPC` wire message, with the prefix, the payload and the suffix on separate lines (line breaks inside the prefix are `\r\n`):

```text
POST /a2a/jsonrpc HTTP/1.1
Host: iboran.local
Content-Type: application/json
A2A-Version: 1.0
Authorization: Bearer xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
Content-Length: 358

{"jsonrpc":"2.0","id":"3","method":"SendMessage","params":{"message":{"messageId":"m-3","role":"ROLE_USER","taskId":"t-7","contextId":"ctx-42","parts":[{"raw":"
AACAPAAAAD0AAEA9AACAPQAAoD0AAMA9AADgPQAAAD4AABA+AAAgPgAAMD4AAEA+AABQPgAAYD4AAHA+AACAPg==
","mediaType":"application/vnd.iboran.embedding+f32","metadata":{"link":"UL","objectId":"12","round":"0"}}]}}}
```

`A2A-gRPC` wire message:

```text
00 × 16                            HEADERS frame (placeholder)
00 00 af | 00 | 01 | 00 00 00 01   DATA frame: length 175, END_STREAM, stream 1
00 | 00 00 00 aa                   gRPC prefix: protobuf length 170
0a a7 01                           SendMessageRequest.message, length 167
0a 03 6d 2d 33                     message_id m-3
12 06 63 74 78 2d 34 32            context_id ctx-42
1a 03 74 2d 37                     task_id t-7
20 01                              role USER
2a 90 01                           parts, length 144
0a 40                              Part.raw, length 64
00 00 80 3c 00 00 00 3d …          payload, 64 bytes
12 24 …                            media_type, 36 bytes
1a 0a … 1a 0e … 1a 0a …            metadata link, objectId, round
```

Packets, with the bytes of prefix, payload and suffix in each slice:

| Model | Packet | Size | Slice | Prefix, payload, suffix | Flags | Level |
|---|---|---|---|---|---|---|
| `None` | 0 | 108 | 0–63 | 0, 64, 0 | `FIRST`, `LAST` | 10 |
| `A2A-JSONRPC` | 0 | 160 | 0–115 | 116, 0, 0 | `FIRST`, `ENVELOPE` | 1 |
| `A2A-JSONRPC` | 1 | 160 | 116–231 | 116, 0, 0 | `ENVELOPE` | 1 |
| `A2A-JSONRPC` | 2 | 149 | 232–336 | 105, 0, 0 | `ENVELOPE` | 1 |
| `A2A-JSONRPC` | 3 | 160 | 337–452 | 0, 88, 28 | `ENVELOPE` | 1 |
| `A2A-JSONRPC` | 4 | 126 | 453–534 | 0, 0, 82 | `LAST`, `ENVELOPE` | 1 |
| `A2A-HTTPJSON` | 0 | 160 | 0–115 | 116, 0, 0 | `FIRST`, `ENVELOPE` | 1 |
| `A2A-HTTPJSON` | 1 | 160 | 116–231 | 116, 0, 0 | `ENVELOPE` | 1 |
| `A2A-HTTPJSON` | 2 | 156 | 232–343 | 48, 64, 0 | `ENVELOPE` | 1 |
| `A2A-HTTPJSON` | 3 | 160 | 344–459 | 0, 24, 92 | `ENVELOPE` | 1 |
| `A2A-HTTPJSON` | 4 | 61 | 460–476 | 0, 0, 17 | `LAST`, `ENVELOPE` | 1 |
| `A2A-gRPC` | 0 | 158 | 0–113 | 58, 56, 0 | `FIRST`, `ENVELOPE` | 1 |
| `A2A-gRPC` | 1 | 130 | 114–199 | 0, 8, 78 | `LAST`, `ENVELOPE` | 1 |

`A2A-JSONRPC` packet 2 ends early because its proposed end, byte 348, falls inside the first base64 group and moves back to the start of the payload.

Header of `A2A-JSONRPC` packet 0:

```text
b0 01 05 07 01 01 00 00   magic, version, flags FIRST|ENVELOPE, EMBEDDING_F32, base64, level 1, round 0
00 00 00 03 00 00 00 07   messageId 3, taskId 7
00 00 00 40 00 00 02 17   payloadSize 64, wireSize 535
00 00 00 00 00 00 00 74   offset 0, fragmentIndex 0, fragmentLength 116
01 51 00 6e               prefixLength 337, suffixLength 110
00 00 00 00 77 35 94 00   txTimeNs 2,000,000,000 (2 s)
```

## Limits

- A receiver shared by several senders saves only the input file of the first one under the output name.
- Task and context IDs set by the caller are kept as sent, without the link and UE prefix, so they are unique only if the caller makes them so.
- The JSON reader in `ParseEnvelope` handles the envelopes this layer writes, not arbitrary JSON.
