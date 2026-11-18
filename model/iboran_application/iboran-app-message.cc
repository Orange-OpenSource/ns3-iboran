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

#include "iboran-app-message.h"

#include "iboran-app-envelope.h"

#include <algorithm>
#include <cctype>
#include <numeric>

namespace ns3
{

namespace
{

constexpr uint32_t kMaxMessageWireSize = 256u * 1024u * 1024u; // sanity bound for reassembly

// flags that describe the whole message, equal in all its fragments
constexpr uint8_t kMessageFlags =
    IbOranFragmentHeader::FLAG_ENVELOPE | IbOranFragmentHeader::FLAG_FILE;

const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int
Base64Value(uint8_t c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z')
    {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9')
    {
        return c - '0' + 52;
    }
    if (c == '+')
    {
        return 62;
    }
    if (c == '/')
    {
        return 63;
    }
    if (c == '=')
    {
        return 0;
    }
    return -1;
}

void
Put8(uint8_t*& p, uint8_t v)
{
    *p++ = v;
}

void
Put16(uint8_t*& p, uint16_t v)
{
    *p++ = static_cast<uint8_t>(v >> 8);
    *p++ = static_cast<uint8_t>(v);
}

void
Put32(uint8_t*& p, uint32_t v)
{
    for (int s = 24; s >= 0; s -= 8)
    {
        *p++ = static_cast<uint8_t>(v >> s);
    }
}

void
Put64(uint8_t*& p, uint64_t v)
{
    for (int s = 56; s >= 0; s -= 8)
    {
        *p++ = static_cast<uint8_t>(v >> s);
    }
}

uint8_t
Get8(const uint8_t*& p)
{
    return *p++;
}

uint16_t
Get16(const uint8_t*& p)
{
    uint16_t v = static_cast<uint16_t>((p[0] << 8) | p[1]);
    p += 2;
    return v;
}

uint32_t
Get32(const uint8_t*& p)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
    {
        v = (v << 8) | *p++;
    }
    return v;
}

uint64_t
Get64(const uint8_t*& p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
    {
        v = (v << 8) | *p++;
    }
    return v;
}

} // namespace

// == BASE64
// ====================================================================
uint32_t
Base64EncodedSize(uint32_t payloadSize)
{
    return 4 * ((payloadSize + 2) / 3);
}

std::vector<uint8_t>
Base64Encode(const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> out;
    out.reserve(Base64EncodedSize(static_cast<uint32_t>(payload.size())));
    size_t i = 0;
    while (i + 3 <= payload.size())
    {
        const uint32_t v = (payload[i] << 16) | (payload[i + 1] << 8) | payload[i + 2];
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 6) & 0x3F]);
        out.push_back(kBase64Alphabet[v & 0x3F]);
        i += 3;
    }
    const size_t rest = payload.size() - i;
    if (rest == 1)
    {
        const uint32_t v = payload[i] << 16;
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    }
    else if (rest == 2)
    {
        const uint32_t v = (payload[i] << 16) | (payload[i + 1] << 8);
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

void
Base64DecodeMasked(const std::vector<uint8_t>& encoded,
                   const std::vector<uint8_t>& encodedMask,
                   uint32_t payloadSize,
                   std::vector<uint8_t>& out,
                   std::vector<uint8_t>& outMask)
{
    out.assign(payloadSize, 0);
    outMask.assign(payloadSize, 0);
    const size_t groups = encoded.size() / 4;
    for (size_t g = 0; g < groups; ++g)
    {
        const size_t byteStart = 3 * g;
        if (byteStart >= payloadSize)
        {
            break;
        }
        bool ok = true;
        int v[4];
        for (int k = 0; k < 4; ++k)
        {
            const size_t idx = 4 * g + k;
            if (idx >= encodedMask.size() || !encodedMask[idx])
            {
                ok = false;
                break;
            }
            v[k] = Base64Value(encoded[idx]);
            if (v[k] < 0)
            {
                ok = false;
                break;
            }
        }
        if (!ok)
        {
            continue;
        }
        const uint8_t bytes[3] = {static_cast<uint8_t>((v[0] << 2) | (v[1] >> 4)),
                                  static_cast<uint8_t>(((v[1] & 0x0F) << 4) | (v[2] >> 2)),
                                  static_cast<uint8_t>(((v[2] & 0x03) << 6) | v[3])};
        for (size_t k = 0; k < 3 && byteStart + k < payloadSize; ++k)
        {
            out[byteStart + k] = bytes[k];
            outMask[byteStart + k] = 1;
        }
    }
}

// == FRAGMENT HEADER
// ===========================================================
void
IbOranFragmentHeader::Serialize(uint8_t* out) const
{
    uint8_t* p = out;
    Put8(p, kMagic);
    Put8(p, kVersion);
    Put8(p, flags);
    Put8(p, static_cast<uint8_t>(contentType));
    Put8(p, static_cast<uint8_t>(encoding));
    Put8(p, criticality);
    Put16(p, round);
    Put32(p, messageId);
    Put32(p, taskId);
    Put32(p, payloadSize);
    Put32(p, wireSize);
    Put32(p, offset);
    Put16(p, fragmentIndex);
    Put16(p, fragmentLength);
    Put16(p, prefixLength);
    Put16(p, suffixLength);
    Put64(p, txTimeNs);
}

bool
IbOranFragmentHeader::Deserialize(const uint8_t* in, uint32_t length)
{
    if (length < kSize)
    {
        return false;
    }
    const uint8_t* p = in;
    if (Get8(p) != kMagic || Get8(p) != kVersion)
    {
        return false;
    }
    flags = Get8(p);
    const uint8_t type = Get8(p);
    const uint8_t enc = Get8(p);
    criticality = Get8(p);
    round = Get16(p);
    messageId = Get32(p);
    taskId = Get32(p);
    payloadSize = Get32(p);
    wireSize = Get32(p);
    offset = Get32(p);
    fragmentIndex = Get16(p);
    fragmentLength = Get16(p);
    prefixLength = Get16(p);
    suffixLength = Get16(p);
    txTimeNs = Get64(p);

    if (enc > static_cast<uint8_t>(IbOranPayloadEncoding::BASE64))
    {
        return false;
    }
    encoding = static_cast<IbOranPayloadEncoding>(enc);
    contentType = ContentTypeFromByte(type);

    const uint64_t fragEnd = static_cast<uint64_t>(offset) + fragmentLength;
    const uint64_t envelope = static_cast<uint64_t>(prefixLength) + suffixLength;
    return fragEnd <= wireSize && envelope <= wireSize && wireSize <= kMaxMessageWireSize;
}

bool
IbOranFragmentHeader::IsFirst() const
{
    return flags & FLAG_FIRST;
}

bool
IbOranFragmentHeader::IsLast() const
{
    return flags & FLAG_LAST;
}

// == FILL POLICY
// ===============================================================
bool
ParseFillPolicy(const std::string& str, IbOranFillPolicy& out)
{
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (s == "auto")
    {
        out = IbOranFillPolicy::AUTO;
    }
    else if (s == "zero")
    {
        out = IbOranFillPolicy::ZERO;
    }
    else if (s == "nan")
    {
        out = IbOranFillPolicy::NAN_FILL;
    }
    else if (s == "drop")
    {
        out = IbOranFillPolicy::DROP;
    }
    else
    {
        return false;
    }
    return true;
}

std::string
FillPolicyToString(IbOranFillPolicy policy)
{
    switch (policy)
    {
    case IbOranFillPolicy::ZERO:
        return "Zero";
    case IbOranFillPolicy::NAN_FILL:
        return "Nan";
    case IbOranFillPolicy::DROP:
        return "Drop";
    case IbOranFillPolicy::AUTO:
    default:
        return "Auto";
    }
}

// == RECEIVED MESSAGE
// ==========================================================
double
IbOranReceivedMessage::GetElementRatio() const
{
    if (elementsTotal == 0)
    {
        return 1.0;
    }
    return static_cast<double>(elementsReceived) / elementsTotal;
}

uint64_t
IbOranReceivedMessage::GetLatencyNs() const
{
    return (lastRxNs > txTimeNs) ? (lastRxNs - txTimeNs) : 0;
}

// == ASSEMBLER
// =================================================================
bool
IbOranMessageAssembler::Add(const IbOranFragmentHeader& header,
                            const uint8_t* data,
                            uint32_t length,
                            uint64_t nowNs)
{
    if (length != header.fragmentLength ||
        static_cast<uint64_t>(header.offset) + length > header.wireSize)
    {
        return false;
    }
    if (!m_initialized)
    {
        if (header.wireSize > kMaxMessageWireSize)
        {
            return false;
        }
        m_first = header;
        m_wire.assign(header.wireSize, 0);
        m_mask.assign(header.wireSize, 0);
        m_firstRxNs = nowNs;
        m_initialized = true;
    }
    else if (header.messageId != m_first.messageId || header.wireSize != m_first.wireSize ||
             header.payloadSize != m_first.payloadSize ||
             header.prefixLength != m_first.prefixLength ||
             header.suffixLength != m_first.suffixLength || header.encoding != m_first.encoding ||
             header.contentType != m_first.contentType ||
             (header.flags & kMessageFlags) != (m_first.flags & kMessageFlags))
    {
        return false;
    }

    for (uint32_t i = 0; i < length; ++i)
    {
        const uint32_t pos = header.offset + i;
        if (!m_mask[pos])
        {
            m_mask[pos] = 1;
            ++m_wireReceived;
        }
        m_wire[pos] = data[i];
    }
    ++m_fragments;
    m_lastRxNs = nowNs;
    return true;
}

bool
IbOranMessageAssembler::IsComplete() const
{
    return m_initialized && m_wireReceived == m_first.wireSize;
}

uint32_t
IbOranMessageAssembler::GetMessageId() const
{
    return m_first.messageId;
}

IbOranReceivedMessage
IbOranMessageAssembler::Finalize(IbOranFillPolicy policy) const
{
    IbOranReceivedMessage r;
    if (!m_initialized)
    {
        return r;
    }
    r.messageId = m_first.messageId;
    r.taskId = m_first.taskId;
    r.round = m_first.round;
    r.fromFile = (m_first.flags & IbOranFragmentHeader::FLAG_FILE) != 0;
    r.contentType = m_first.contentType;
    r.encoding = m_first.encoding;
    r.payloadSize = m_first.payloadSize;
    r.wireSize = m_first.wireSize;
    r.wireBytesReceived = m_wireReceived;
    r.fragmentsReceived = m_fragments;
    r.txTimeNs = m_first.txTimeNs;
    r.firstRxNs = m_firstRxNs;
    r.lastRxNs = m_lastRxNs;
    r.complete = IsComplete();

    // 1. encoded payload region
    const uint32_t start = m_first.prefixLength;
    const uint32_t end = m_first.wireSize - m_first.suffixLength;
    std::vector<uint8_t> enc;
    std::vector<uint8_t> encMask;
    if (end > start)
    {
        enc.assign(m_wire.begin() + start, m_wire.begin() + end);
        encMask.assign(m_mask.begin() + start, m_mask.begin() + end);
    }

    // 2. decode
    std::vector<uint8_t> dec;
    std::vector<uint8_t> decMask;
    if (m_first.encoding == IbOranPayloadEncoding::BASE64)
    {
        Base64DecodeMasked(enc, encMask, m_first.payloadSize, dec, decMask);
    }
    else
    {
        dec = enc;
        decMask = encMask;
        dec.resize(m_first.payloadSize, 0);
        decMask.resize(m_first.payloadSize, 0);
    }

    // 3. element-wise fill
    const uint32_t elem = GetElementSize(m_first.contentType);
    const bool useNan = policy == IbOranFillPolicy::NAN_FILL ||
                        (policy == IbOranFillPolicy::AUTO && IsFloatEmbedding(m_first.contentType));
    const std::vector<uint8_t> pattern = GetMissingElementPattern(m_first.contentType, useNan);

    r.elementsTotal = (m_first.payloadSize + elem - 1) / elem;
    r.payload.reserve(m_first.payloadSize);
    for (uint32_t e = 0; e < r.elementsTotal; ++e)
    {
        const uint32_t b = e * elem;
        const uint32_t len = std::min(elem, m_first.payloadSize - b);
        const bool ok = std::all_of(decMask.begin() + b, decMask.begin() + b + len, [](uint8_t m) {
            return m;
        });
        if (ok)
        {
            r.payload.insert(r.payload.end(), dec.begin() + b, dec.begin() + b + len);
            ++r.elementsReceived;
            continue;
        }
        if (policy != IbOranFillPolicy::DROP)
        {
            r.payload.insert(r.payload.end(), pattern.begin(), pattern.begin() + len);
        }
        if (!r.missingRanges.empty() && r.missingRanges.back().second == b)
        {
            r.missingRanges.back().second = b + len;
        }
        else
        {
            r.missingRanges.emplace_back(b, b + len);
        }
    }

    // 4. envelope
    if (m_first.flags & IbOranFragmentHeader::FLAG_ENVELOPE)
    {
        r.envelope = "Unknown";
        const uint32_t suffixStart = m_first.wireSize - m_first.suffixLength;
        const bool whole =
            std::all_of(m_mask.begin(), m_mask.begin() + start, [](uint8_t m) { return m; }) &&
            std::all_of(m_mask.begin() + suffixStart, m_mask.end(), [](uint8_t m) { return m; });
        IbOranEnvelopeModel model = IbOranEnvelopeModel::NONE;
        IbOranEnvelopeContext context;
        if (whole &&
            ParseEnvelope(std::vector<uint8_t>(m_wire.begin(), m_wire.begin() + start),
                          std::vector<uint8_t>(m_wire.begin() + suffixStart, m_wire.end()),
                          m_first.payloadSize,
                          model,
                          context) &&
            context.messageId == m_first.messageId && context.taskId == m_first.taskId)
        {
            r.envelope = EnvelopeModelToString(model);
            r.envelopeValid = true;
            r.role = context.role;
            r.mediaType = context.mediaType;
            r.metadata = context.metadata;
            r.contextId = context.contextId;
        }
    }
    return r;
}

// == FRAGMENTATION
// =============================================================
uint32_t
GetWireAlignmentUnit(IbOranContentType type, IbOranPayloadEncoding encoding)
{
    const uint32_t elem = GetElementSize(type);
    if (encoding == IbOranPayloadEncoding::BASE64)
    {
        return (std::lcm(3u, elem) / 3) * 4;
    }
    return elem;
}

uint32_t
AlignFragmentEnd(uint32_t start,
                 uint32_t proposedEnd,
                 uint32_t payloadStart,
                 uint32_t payloadEnd,
                 uint32_t unit)
{
    if (unit <= 1 || proposedEnd <= payloadStart || proposedEnd >= payloadEnd)
    {
        return proposedEnd;
    }
    const uint32_t aligned = payloadStart + ((proposedEnd - payloadStart) / unit) * unit;
    return (aligned > start) ? aligned : proposedEnd;
}

} // namespace ns3
