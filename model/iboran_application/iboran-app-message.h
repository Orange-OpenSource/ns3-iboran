/*
 * Software Name: ib-ORAN
 * SPDX-FileCopyrightText: Copyright (c) Orange SA
 * SPDX-License-Identifier: MIT
 *
 * This software is distributed under the MIT license,
 * see the "LICENSE" file for more details
 * or https://spdx.org/licenses/MIT.html
 *
 * Authors: see CONTRIBUTORS.md
 */

#ifndef IBORAN_APP_MESSAGE_H
#define IBORAN_APP_MESSAGE_H

#include "iboran-app-content.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

// == PAYLOAD ENCODING
// ==========================================================
enum class IbOranPayloadEncoding : uint8_t
{
    IDENTITY = 0, /**< payload bytes as they are (raw UDP, gRPC) */
    BASE64 = 1,   /**< base64 text, as in A2A JSON bindings */
};

uint32_t Base64EncodedSize(uint32_t payloadSize);
std::vector<uint8_t> Base64Encode(const std::vector<uint8_t>& payload);

/**
 * Decode base64 text where some characters may be missing. A 4-character
 * group with a missing character marks its 3 output bytes as missing.
 */
void Base64DecodeMasked(const std::vector<uint8_t>& encoded,
                        const std::vector<uint8_t>& encodedMask,
                        uint32_t payloadSize,
                        std::vector<uint8_t>& out,
                        std::vector<uint8_t>& outMask);

// == FRAGMENT HEADER
// ===========================================================
/**
 * Fixed-size header carried at the start of every datagram (or of every
 * fragment in a TCP stream). Big-endian, 44 bytes.
 *
 * | Field          | Bytes | Meaning                                          |
 * |----------------|-------|--------------------------------------------------|
 * | magic          | 1     | 0xB0                                             |
 * | version        | 1     | 1                                                |
 * | flags          | 1     | FIRST, LAST, ENVELOPE, FILE                      |
 * | contentType    | 1     | IbOranContentType                                |
 * | encoding       | 1     | IbOranPayloadEncoding                            |
 * | criticality    | 1     | 1 (most critical) to 10                          |
 * | round          | 2     | round set by the sender (0 for the file)         |
 * | messageId      | 4     | unique per sender                                |
 * | taskId         | 4     | groups messages of one exchange (0 = none)       |
 * | payloadSize    | 4     | decoded payload size in bytes                    |
 * | wireSize       | 4     | bytes of the whole message on the wire           |
 * | offset         | 4     | offset of this fragment in the wire message      |
 * | fragmentIndex  | 2     | index of this fragment                           |
 * | fragmentLength | 2     | bytes of message data after the header           |
 * | prefixLength   | 2     | envelope bytes before the encoded payload        |
 * | suffixLength   | 2     | envelope bytes after the encoded payload         |
 * | txTimeNs       | 8     | creation time of the message (simulation time)   |
 */
struct IbOranFragmentHeader
{
    static constexpr uint8_t kMagic = 0xB0;
    static constexpr uint8_t kVersion = 1;
    static constexpr uint32_t kSize = 44;

    static constexpr uint8_t FLAG_FIRST = 0x01;
    static constexpr uint8_t FLAG_LAST = 0x02;
    static constexpr uint8_t FLAG_ENVELOPE = 0x04;
    static constexpr uint8_t FLAG_FILE = 0x08; /**< message carries the input file */

    uint8_t flags = 0;
    IbOranContentType contentType = IbOranContentType::UNKNOWN;
    IbOranPayloadEncoding encoding = IbOranPayloadEncoding::IDENTITY;
    uint8_t criticality = IbOranCriticalityMap::kDefault;
    uint16_t round = 0;
    uint32_t messageId = 0;
    uint32_t taskId = 0;
    uint32_t payloadSize = 0;
    uint32_t wireSize = 0;
    uint32_t offset = 0;
    uint16_t fragmentIndex = 0;
    uint16_t fragmentLength = 0;
    uint16_t prefixLength = 0;
    uint16_t suffixLength = 0;
    uint64_t txTimeNs = 0;

    /** Write kSize bytes to out. */
    void Serialize(uint8_t* out) const;

    /** Read kSize bytes from in; false if magic, version or field bounds are
     * invalid. */
    bool Deserialize(const uint8_t* in, uint32_t length);

    bool IsFirst() const;
    bool IsLast() const;
};

// == MESSAGE (SENDER SIDE)
// =====================================================
/**
 * Unit of application data handed to the transmission application. A file is
 * one message; an agent can push many (requests, refinements, results).
 */
struct IbOranMessage
{
    uint32_t messageId = 0; /**< 0 = assigned by the application */
    uint32_t taskId = 0;    /**< 0 = no task */
    uint16_t round = 0;
    std::string contextId;
    IbOranContentType contentType = IbOranContentType::RAW;
    std::vector<uint8_t> payload;
    std::map<std::string, std::string> metadata;
    IbOranCriticalityMap criticality; /**< over payload bytes; empty = default */
    bool fromFile = false;            /**< the loaded input file; sets FLAG_FILE */
};

// == MESSAGE (RECEIVER SIDE)
// ===================================================
enum class IbOranFillPolicy
{
    AUTO,     /**< NaN for float embeddings, zero otherwise */
    ZERO,     /**< missing elements set to zero bytes */
    NAN_FILL, /**< missing float elements set to NaN (zero for other types) */
    DROP,     /**< missing elements removed (legacy concatenation, element-aligned) */
};

bool ParseFillPolicy(const std::string& str, IbOranFillPolicy& out);
std::string FillPolicyToString(IbOranFillPolicy policy);

struct IbOranReceivedMessage
{
    uint32_t messageId = 0;
    uint32_t taskId = 0; /**< task ID set by the sender (0 = none) */
    uint16_t round = 0;
    bool fromFile = false; /**< FLAG_FILE was set: the sender's input file */

    // -- envelope, read with ParseEnvelope (FLAG_ENVELOPE set) ----------------
    std::string envelope = "None"; /**< model name, "Unknown" if unparsable */
    bool envelopeValid = false;    /**< envelope received whole and parsed */
    std::string role;
    std::string mediaType;
    std::map<std::string, std::string> metadata;

    // -- A2A identifiers held by the receiver (the server agent) --------------
    std::string a2aTaskId;          /**< the sender's, or assigned */
    std::string contextId;          /**< the sender's, or assigned */
    bool taskIdAssigned = false;    /**< a2aTaskId assigned by the receiver */
    bool contextIdAssigned = false; /**< contextId assigned by the receiver */
    IbOranContentType contentType = IbOranContentType::UNKNOWN;
    IbOranPayloadEncoding encoding = IbOranPayloadEncoding::IDENTITY;

    std::vector<uint8_t> payload; /**< decoded payload after the fill policy */

    uint32_t payloadSize = 0;       /**< original payload size */
    uint32_t wireSize = 0;          /**< bytes of the message on the wire */
    uint32_t wireBytesReceived = 0; /**< distinct wire bytes received */
    uint32_t fragmentsReceived = 0; /**< fragments received, duplicates included */
    uint32_t elementsTotal = 0;
    uint32_t elementsReceived = 0; /**< elements with every byte received */

    uint64_t txTimeNs = 0;
    uint64_t firstRxNs = 0;
    uint64_t lastRxNs = 0;

    bool complete = false; /**< every wire byte received */

    /**
     * Payload byte ranges [start, end) of the elements not fully received, in
     * offsets of the original payload. With the DROP fill policy these elements
     * are absent from payload.
     */
    std::vector<std::pair<uint32_t, uint32_t>> missingRanges;

    /** Fraction of payload elements fully received (1 if the payload is empty).
     */
    double GetElementRatio() const;

    /** Time from message creation to its last received fragment, in ns. */
    uint64_t GetLatencyNs() const;
};

// == REASSEMBLY
// ================================================================
class IbOranMessageAssembler
{
  public:
    /**
     * Add one fragment. Returns false and ignores the fragment if it is
     * inconsistent with the first fragment of the same message.
     */
    bool Add(const IbOranFragmentHeader& header,
             const uint8_t* data,
             uint32_t length,
             uint64_t nowNs);

    bool IsComplete() const;
    uint32_t GetMessageId() const;

    /** Decode the payload and apply the fill policy. */
    IbOranReceivedMessage Finalize(IbOranFillPolicy policy) const;

  private:
    bool m_initialized = false;
    IbOranFragmentHeader m_first; // reference header (sizes, type, ids)
    std::vector<uint8_t> m_wire;
    std::vector<uint8_t> m_mask; // 1 = byte received
    uint32_t m_wireReceived = 0;
    uint32_t m_fragments = 0;
    uint64_t m_firstRxNs = 0;
    uint64_t m_lastRxNs = 0;
};

// == FRAGMENTATION
// =============================================================
/**
 * Largest fragment end <= proposedEnd that does not split an element of the
 * payload (identity encoding) or a base64 group covering whole elements.
 * Returns proposedEnd when no aligned end lies after start.
 */
uint32_t AlignFragmentEnd(uint32_t start,
                          uint32_t proposedEnd,
                          uint32_t payloadStart,
                          uint32_t payloadEnd,
                          uint32_t unit);

/** Alignment unit in wire bytes for a content type and encoding. */
uint32_t GetWireAlignmentUnit(IbOranContentType type, IbOranPayloadEncoding encoding);

} // namespace ns3

#endif // IBORAN_APP_MESSAGE_H
