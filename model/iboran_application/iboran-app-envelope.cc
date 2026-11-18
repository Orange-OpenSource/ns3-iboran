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

#include "iboran-app-envelope.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>

namespace ns3
{

namespace
{

constexpr uint32_t kHttp2MaxFrame = 16384; // default SETTINGS_MAX_FRAME_SIZE

void
Append(std::vector<uint8_t>& out, const std::string& s)
{
    out.insert(out.end(), s.begin(), s.end());
}

// -- protobuf wire helpers (field numbers are illustrative) -------------------
void
PutVarint(std::vector<uint8_t>& out, uint64_t v)
{
    while (v >= 0x80)
    {
        out.push_back(static_cast<uint8_t>(v | 0x80));
        v >>= 7;
    }
    out.push_back(static_cast<uint8_t>(v));
}

uint32_t
VarintSize(uint64_t v)
{
    uint32_t n = 1;
    while (v >= 0x80)
    {
        v >>= 7;
        ++n;
    }
    return n;
}

void
PutLenField(std::vector<uint8_t>& out, uint32_t field, const std::string& value)
{
    PutVarint(out, (static_cast<uint64_t>(field) << 3) | 2);
    PutVarint(out, value.size());
    Append(out, value);
}

void
PutLenHeader(std::vector<uint8_t>& out, uint32_t field, uint64_t length)
{
    PutVarint(out, (static_cast<uint64_t>(field) << 3) | 2);
    PutVarint(out, length);
}

std::string
MetadataJson(const std::map<std::string, std::string>& metadata)
{
    if (metadata.empty())
    {
        return "";
    }
    std::string s = ",\"metadata\":{";
    bool first = true;
    for (const auto& [k, v] : metadata)
    {
        if (!first)
        {
            s += ",";
        }
        first = false;
        s += "\"" + JsonEscape(k) + "\":\"" + JsonEscape(v) + "\"";
    }
    s += "}";
    return s;
}

IbOranEnvelopeLayout
BuildJsonEnvelope(bool jsonRpc,
                  const IbOranEnvelopeContext& ctx,
                  uint32_t payloadSize,
                  const IbOranEnvelopeOptions& options)
{
    IbOranEnvelopeLayout layout;
    layout.encoding = IbOranPayloadEncoding::BASE64;

    std::string message = "\"message\":{\"messageId\":\"m-" + std::to_string(ctx.messageId) +
                          "\",\"role\":\"" + JsonEscape(ctx.role) + "\"";
    if (ctx.taskId != 0)
    {
        message += ",\"taskId\":\"t-" + std::to_string(ctx.taskId) + "\"";
    }
    if (!ctx.contextId.empty())
    {
        message += ",\"contextId\":\"" + JsonEscape(ctx.contextId) + "\"";
    }
    message += ",\"parts\":[{\"raw\":\"";

    std::string head;
    std::string tail = "\",\"mediaType\":\"" + JsonEscape(ctx.mediaType) + "\"" +
                       MetadataJson(ctx.metadata) + "}]}";
    if (jsonRpc)
    {
        head = "{\"jsonrpc\":\"2.0\",\"id\":\"" + std::to_string(ctx.messageId) +
               "\",\"method\":\"SendMessage\",\"params\":{" + message;
        tail += "}}";
    }
    else
    {
        head = "{" + message;
        tail += "}";
    }

    const uint64_t bodyLength = head.size() + Base64EncodedSize(payloadSize) + tail.size();
    std::string http = std::string("POST ") + (jsonRpc ? "/a2a/jsonrpc" : "/message:send") +
                       " HTTP/1.1\r\n" + "Host: " + ctx.host + "\r\n" +
                       "Content-Type: application/json\r\n" + "A2A-Version: 1.0\r\n";
    if (options.bearerTokenLength > 0)
    {
        http += "Authorization: Bearer " + std::string(options.bearerTokenLength, 'x') + "\r\n";
    }
    http += "Content-Length: " + std::to_string(bodyLength) + "\r\n\r\n";

    Append(layout.prefix, http);
    Append(layout.prefix, head);
    Append(layout.suffix, tail);
    return layout;
}

IbOranEnvelopeLayout
BuildGrpcEnvelope(const IbOranEnvelopeContext& ctx,
                  uint32_t payloadSize,
                  const IbOranEnvelopeOptions& options)
{
    IbOranEnvelopeLayout layout;
    layout.encoding = IbOranPayloadEncoding::IDENTITY;

    // Part { raw = 1; media_type = 2; metadata entries = 3 }
    std::vector<uint8_t> partTail;
    PutLenField(partTail, 2, ctx.mediaType);
    for (const auto& [k, v] : ctx.metadata)
    {
        std::vector<uint8_t> entry;
        PutLenField(entry, 1, k);
        PutLenField(entry, 2, v);
        PutLenHeader(partTail, 3, entry.size());
        partTail.insert(partTail.end(), entry.begin(), entry.end());
    }
    const uint64_t partLength = 1 + VarintSize(payloadSize) + payloadSize + partTail.size();

    // Message { message_id = 1; context_id = 2; task_id = 3; role = 4; parts = 5
    // }
    std::vector<uint8_t> messageHead;
    PutLenField(messageHead, 1, "m-" + std::to_string(ctx.messageId));
    if (!ctx.contextId.empty())
    {
        PutLenField(messageHead, 2, ctx.contextId);
    }
    if (ctx.taskId != 0)
    {
        PutLenField(messageHead, 3, "t-" + std::to_string(ctx.taskId));
    }
    PutVarint(messageHead, (4u << 3) | 0); // role, varint
    PutVarint(messageHead, 1);             // ROLE_USER
    PutLenHeader(messageHead, 5, partLength);
    PutVarint(messageHead, (1u << 3) | 2); // Part.raw tag
    PutVarint(messageHead, payloadSize);   // Part.raw length

    const uint64_t messageLength = messageHead.size() + payloadSize + partTail.size();

    // SendMessageRequest { message = 1 }
    std::vector<uint8_t> requestHead;
    PutLenHeader(requestHead, 1, messageLength);
    const uint64_t protobufLength = requestHead.size() + messageLength;

    // HTTP/2: HEADERS frame (placeholder), DATA frame header(s), gRPC prefix
    const uint64_t dataLength = 5 + protobufLength;
    const uint64_t extraFrames = (dataLength > 0) ? (dataLength - 1) / kHttp2MaxFrame : 0;

    layout.prefix.assign(options.grpcHeaderBytes, 0x00);
    const uint32_t firstFrame =
        static_cast<uint32_t>(std::min<uint64_t>(dataLength, kHttp2MaxFrame));
    layout.prefix.push_back(static_cast<uint8_t>(firstFrame >> 16));
    layout.prefix.push_back(static_cast<uint8_t>(firstFrame >> 8));
    layout.prefix.push_back(static_cast<uint8_t>(firstFrame));
    layout.prefix.push_back(0x00);                                       // type DATA
    layout.prefix.push_back(extraFrames == 0 ? 0x01 : 0x00);             // END_STREAM
    layout.prefix.insert(layout.prefix.end(), {0x00, 0x00, 0x00, 0x01}); // stream 1
    // further DATA frame headers, accounted for in size only
    layout.prefix.insert(layout.prefix.end(), 9 * extraFrames, 0x00);

    layout.prefix.push_back(0x00); // gRPC: not compressed
    for (int s = 24; s >= 0; s -= 8)
    {
        layout.prefix.push_back(static_cast<uint8_t>(protobufLength >> s));
    }
    layout.prefix.insert(layout.prefix.end(), requestHead.begin(), requestHead.end());
    layout.prefix.insert(layout.prefix.end(), messageHead.begin(), messageHead.end());
    layout.suffix = partTail;
    return layout;
}

// -- envelope parsing ---------------------------------------------------------
// Minimal JSON reader: enough for the envelopes built above, and strict
// enough to reject truncated or corrupted text.
struct JsonValue
{
    enum class Type
    {
        NUL,
        BOOL,
        NUMBER,
        STRING,
        ARRAY,
        OBJECT
    };
    Type type = Type::NUL;
    std::string text; // string value, or number as written
    bool boolean = false;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;

    const JsonValue* Get(const std::string& key) const
    {
        if (type != Type::OBJECT)
        {
            return nullptr;
        }
        for (const auto& [k, v] : object)
        {
            if (k == key)
            {
                return &v;
            }
        }
        return nullptr;
    }
};

class JsonReader
{
  public:
    explicit JsonReader(const std::string& text)
        : m_s(text)
    {
    }

    /** Parse one value that spans the whole text. */
    bool Parse(JsonValue& out)
    {
        m_pos = 0;
        if (!Value(out, 0))
        {
            return false;
        }
        SkipSpace();
        return m_pos == m_s.size();
    }

  private:
    static constexpr int kMaxDepth = 32;

    void SkipSpace()
    {
        while (m_pos < m_s.size() && (m_s[m_pos] == ' ' || m_s[m_pos] == '\t' ||
                                      m_s[m_pos] == '\n' || m_s[m_pos] == '\r'))
        {
            ++m_pos;
        }
    }

    bool Literal(const char* word)
    {
        const std::string w(word);
        if (m_s.compare(m_pos, w.size(), w) != 0)
        {
            return false;
        }
        m_pos += w.size();
        return true;
    }

    static void AppendUtf8(std::string& out, uint32_t cp)
    {
        if (cp < 0x80)
        {
            out += static_cast<char>(cp);
        }
        else if (cp < 0x800)
        {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool Hex4(uint32_t& out)
    {
        if (m_pos + 4 > m_s.size())
        {
            return false;
        }
        out = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char c = m_s[m_pos++];
            out <<= 4;
            if (c >= '0' && c <= '9')
            {
                out |= static_cast<uint32_t>(c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                out |= static_cast<uint32_t>(c - 'a' + 10);
            }
            else if (c >= 'A' && c <= 'F')
            {
                out |= static_cast<uint32_t>(c - 'A' + 10);
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    bool String(std::string& out)
    {
        if (m_pos >= m_s.size() || m_s[m_pos] != '"')
        {
            return false;
        }
        ++m_pos;
        out.clear();
        while (m_pos < m_s.size())
        {
            const unsigned char c = static_cast<unsigned char>(m_s[m_pos++]);
            if (c == '"')
            {
                return true;
            }
            if (c < 0x20)
            {
                return false;
            }
            if (c != '\\')
            {
                out += static_cast<char>(c);
                continue;
            }
            if (m_pos >= m_s.size())
            {
                return false;
            }
            const char e = m_s[m_pos++];
            switch (e)
            {
            case '"':
            case '\\':
            case '/':
                out += e;
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u': {
                uint32_t cp = 0;
                if (!Hex4(cp))
                {
                    return false;
                }
                if (cp >= 0xD800 && cp <= 0xDBFF)
                {
                    uint32_t low = 0;
                    if (!Literal("\\u") || !Hex4(low) || low < 0xDC00 || low > 0xDFFF)
                    {
                        return false;
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                else if (cp >= 0xDC00 && cp <= 0xDFFF)
                {
                    return false;
                }
                AppendUtf8(out, cp);
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool Number(std::string& out)
    {
        const size_t start = m_pos;
        if (m_pos < m_s.size() && m_s[m_pos] == '-')
        {
            ++m_pos;
        }
        size_t digits = 0;
        while (m_pos < m_s.size() &&
               (std::isdigit(static_cast<unsigned char>(m_s[m_pos])) || m_s[m_pos] == '.' ||
                m_s[m_pos] == 'e' || m_s[m_pos] == 'E' || m_s[m_pos] == '+' || m_s[m_pos] == '-'))
        {
            digits += std::isdigit(static_cast<unsigned char>(m_s[m_pos])) ? 1 : 0;
            ++m_pos;
        }
        out = m_s.substr(start, m_pos - start);
        return digits > 0;
    }

    bool Value(JsonValue& out, int depth)
    {
        if (depth > kMaxDepth)
        {
            return false;
        }
        SkipSpace();
        if (m_pos >= m_s.size())
        {
            return false;
        }
        const char c = m_s[m_pos];
        if (c == '{')
        {
            out.type = JsonValue::Type::OBJECT;
            ++m_pos;
            SkipSpace();
            if (m_pos < m_s.size() && m_s[m_pos] == '}')
            {
                ++m_pos;
                return true;
            }
            while (true)
            {
                SkipSpace();
                std::string key;
                if (!String(key))
                {
                    return false;
                }
                SkipSpace();
                if (m_pos >= m_s.size() || m_s[m_pos] != ':')
                {
                    return false;
                }
                ++m_pos;
                JsonValue v;
                if (!Value(v, depth + 1))
                {
                    return false;
                }
                out.object.emplace_back(std::move(key), std::move(v));
                SkipSpace();
                if (m_pos < m_s.size() && m_s[m_pos] == ',')
                {
                    ++m_pos;
                    continue;
                }
                if (m_pos < m_s.size() && m_s[m_pos] == '}')
                {
                    ++m_pos;
                    return true;
                }
                return false;
            }
        }
        if (c == '[')
        {
            out.type = JsonValue::Type::ARRAY;
            ++m_pos;
            SkipSpace();
            if (m_pos < m_s.size() && m_s[m_pos] == ']')
            {
                ++m_pos;
                return true;
            }
            while (true)
            {
                JsonValue v;
                if (!Value(v, depth + 1))
                {
                    return false;
                }
                out.array.push_back(std::move(v));
                SkipSpace();
                if (m_pos < m_s.size() && m_s[m_pos] == ',')
                {
                    ++m_pos;
                    continue;
                }
                if (m_pos < m_s.size() && m_s[m_pos] == ']')
                {
                    ++m_pos;
                    return true;
                }
                return false;
            }
        }
        if (c == '"')
        {
            out.type = JsonValue::Type::STRING;
            return String(out.text);
        }
        if (c == 't' || c == 'f')
        {
            out.type = JsonValue::Type::BOOL;
            out.boolean = (c == 't');
            return Literal(out.boolean ? "true" : "false");
        }
        if (c == 'n')
        {
            out.type = JsonValue::Type::NUL;
            return Literal("null");
        }
        out.type = JsonValue::Type::NUMBER;
        return Number(out.text);
    }

    const std::string& m_s;
    size_t m_pos = 0;
};

bool
GetString(const JsonValue* v, std::string& out)
{
    if (!v || v->type != JsonValue::Type::STRING)
    {
        return false;
    }
    out = v->text;
    return true;
}

// "<tag><digits>" -> number, as written by the builders ("m-7", "t-3")
bool
ParseTaggedId(const std::string& s, const std::string& tag, uint32_t& out)
{
    if (s.size() <= tag.size() || s.compare(0, tag.size(), tag) != 0 || s.size() - tag.size() > 10)
    {
        return false;
    }
    uint64_t n = 0;
    for (size_t i = tag.size(); i < s.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
        {
            return false;
        }
        n = n * 10 + static_cast<uint64_t>(s[i] - '0');
    }
    if (n > UINT32_MAX)
    {
        return false;
    }
    out = static_cast<uint32_t>(n);
    return true;
}

std::string
ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool
ParseJsonEnvelope(const std::string& prefix,
                  const std::string& suffix,
                  uint32_t payloadSize,
                  IbOranEnvelopeModel& model,
                  IbOranEnvelopeContext& ctx)
{
    // 1. HTTP/1.1 request line and headers
    const size_t headersEnd = prefix.find("\r\n\r\n");
    if (headersEnd == std::string::npos)
    {
        return false;
    }
    const size_t lineEnd = prefix.find("\r\n");
    const std::string requestLine = prefix.substr(0, lineEnd);
    if (requestLine == "POST /a2a/jsonrpc HTTP/1.1")
    {
        model = IbOranEnvelopeModel::A2A_JSONRPC;
    }
    else if (requestLine == "POST /message:send HTTP/1.1")
    {
        model = IbOranEnvelopeModel::A2A_HTTP_JSON;
    }
    else
    {
        return false;
    }

    std::map<std::string, std::string> headers;
    size_t pos = lineEnd + 2;
    while (pos < headersEnd)
    {
        const size_t end = prefix.find("\r\n", pos);
        const std::string line = prefix.substr(pos, end - pos);
        const size_t colon = line.find(':');
        if (colon == std::string::npos)
        {
            return false;
        }
        std::string value = line.substr(colon + 1);
        value.erase(0, value.find_first_not_of(' '));
        headers[ToLower(line.substr(0, colon))] = value;
        pos = end + 2;
    }
    const std::string head = prefix.substr(headersEnd + 4);
    const auto contentType = headers.find("content-type");
    const auto contentLength = headers.find("content-length");
    if (contentType == headers.end() || contentType->second != "application/json" ||
        contentLength == headers.end() ||
        contentLength->second !=
            std::to_string(head.size() + Base64EncodedSize(payloadSize) + suffix.size()))
    {
        return false;
    }
    const auto host = headers.find("host");
    ctx.host = (host != headers.end()) ? host->second : "";

    // 2. JSON body. The base64 payload sits between head and suffix, inside the
    // "raw" string, so head + suffix is the same document with an empty "raw".
    JsonValue root;
    if (!JsonReader(head + suffix).Parse(root))
    {
        return false;
    }
    const JsonValue* message = nullptr;
    if (model == IbOranEnvelopeModel::A2A_JSONRPC)
    {
        std::string version;
        std::string method;
        const JsonValue* params = root.Get("params");
        if (!GetString(root.Get("jsonrpc"), version) || version != "2.0" ||
            !GetString(root.Get("method"), method) || method != "SendMessage" || !params)
        {
            return false;
        }
        message = params->Get("message");
    }
    else
    {
        message = root.Get("message");
    }
    if (!message)
    {
        return false;
    }

    std::string text;
    if (!GetString(message->Get("messageId"), text) || !ParseTaggedId(text, "m-", ctx.messageId) ||
        !GetString(message->Get("role"), ctx.role))
    {
        return false;
    }
    if (model == IbOranEnvelopeModel::A2A_JSONRPC)
    {
        std::string id;
        if (!GetString(root.Get("id"), id) || id != std::to_string(ctx.messageId))
        {
            return false;
        }
    }
    ctx.taskId = 0;
    if (message->Get("taskId") &&
        (!GetString(message->Get("taskId"), text) || !ParseTaggedId(text, "t-", ctx.taskId)))
    {
        return false;
    }
    ctx.contextId.clear();
    if (message->Get("contextId") && !GetString(message->Get("contextId"), ctx.contextId))
    {
        return false;
    }

    const JsonValue* parts = message->Get("parts");
    if (!parts || parts->type != JsonValue::Type::ARRAY || parts->array.size() != 1)
    {
        return false;
    }
    const JsonValue& part = parts->array[0];
    std::string raw;
    if (!GetString(part.Get("raw"), raw) || !raw.empty() ||
        !GetString(part.Get("mediaType"), ctx.mediaType))
    {
        return false;
    }
    ctx.metadata.clear();
    if (const JsonValue* metadata = part.Get("metadata"))
    {
        if (metadata->type != JsonValue::Type::OBJECT)
        {
            return false;
        }
        for (const auto& [k, v] : metadata->object)
        {
            if (v.type != JsonValue::Type::STRING)
            {
                return false;
            }
            ctx.metadata[k] = v.text;
        }
    }
    return true;
}

// Reads protobuf wire data written by PutVarint, PutLenField, PutLenHeader.
class ProtoReader
{
  public:
    ProtoReader(const uint8_t* data, size_t size)
        : m_p(data),
          m_n(size)
    {
    }

    size_t Position() const
    {
        return m_pos;
    }

    bool AtEnd() const
    {
        return m_pos == m_n;
    }

    bool Varint(uint64_t& out)
    {
        out = 0;
        for (int shift = 0; shift < 64; shift += 7)
        {
            if (m_pos >= m_n)
            {
                return false;
            }
            const uint8_t b = m_p[m_pos++];
            out |= static_cast<uint64_t>(b & 0x7F) << shift;
            if (!(b & 0x80))
            {
                return true;
            }
        }
        return false;
    }

    /** Tag of the next field: field number and wire type. */
    bool Tag(uint32_t& field, uint32_t& wireType)
    {
        uint64_t tag = 0;
        if (!Varint(tag) || (tag >> 3) == 0 || (tag >> 3) > UINT32_MAX)
        {
            return false;
        }
        field = static_cast<uint32_t>(tag >> 3);
        wireType = static_cast<uint32_t>(tag & 7);
        return true;
    }

    /** Length-delimited value after its tag. */
    bool Bytes(std::string& out)
    {
        uint64_t length = 0;
        if (!Varint(length) || length > m_n - m_pos)
        {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(m_p + m_pos), length);
        m_pos += length;
        return true;
    }

  private:
    const uint8_t* m_p;
    size_t m_n;
    size_t m_pos = 0;
};

// Message { message_id = 1; context_id = 2; task_id = 3; role = 4; parts = 5 }
// up to the Part.raw length, which must end the prefix.
bool
ParseGrpcMessageHead(const uint8_t* data,
                     size_t size,
                     uint32_t payloadSize,
                     uint64_t& partLength,
                     IbOranEnvelopeContext& ctx)
{
    ProtoReader r(data, size);
    bool haveId = false;
    bool haveRole = false;
    ctx.taskId = 0;
    ctx.contextId.clear();
    while (true)
    {
        uint32_t field = 0;
        uint32_t wireType = 0;
        if (!r.Tag(field, wireType))
        {
            return false;
        }
        std::string value;
        if (field == 1 && wireType == 2)
        {
            if (!r.Bytes(value) || !ParseTaggedId(value, "m-", ctx.messageId))
            {
                return false;
            }
            haveId = true;
        }
        else if (field == 2 && wireType == 2)
        {
            if (!r.Bytes(ctx.contextId))
            {
                return false;
            }
        }
        else if (field == 3 && wireType == 2)
        {
            if (!r.Bytes(value) || !ParseTaggedId(value, "t-", ctx.taskId))
            {
                return false;
            }
        }
        else if (field == 4 && wireType == 0)
        {
            uint64_t role = 0;
            if (!r.Varint(role) || (role != 1 && role != 2))
            {
                return false;
            }
            ctx.role = (role == 1) ? "ROLE_USER" : "ROLE_AGENT";
            haveRole = true;
        }
        else if (field == 5 && wireType == 2)
        {
            // parts: only the length here, then Part.raw tag and length
            uint64_t rawLength = 0;
            uint32_t rawField = 0;
            uint32_t rawType = 0;
            if (!r.Varint(partLength) || !r.Tag(rawField, rawType) || rawField != 1 ||
                rawType != 2 || !r.Varint(rawLength) || rawLength != payloadSize)
            {
                return false;
            }
            return haveId && haveRole && r.AtEnd();
        }
        else
        {
            return false;
        }
    }
}

// Part fields after raw: media_type = 2, metadata entries = 3 { key = 1;
// value = 2 }
bool
ParseGrpcPartTail(const std::vector<uint8_t>& suffix, IbOranEnvelopeContext& ctx)
{
    ProtoReader r(suffix.data(), suffix.size());
    bool haveMediaType = false;
    ctx.metadata.clear();
    while (!r.AtEnd())
    {
        uint32_t field = 0;
        uint32_t wireType = 0;
        if (!r.Tag(field, wireType) || wireType != 2)
        {
            return false;
        }
        std::string value;
        if (!r.Bytes(value))
        {
            return false;
        }
        if (field == 2)
        {
            ctx.mediaType = value;
            haveMediaType = true;
        }
        else if (field == 3)
        {
            ProtoReader e(reinterpret_cast<const uint8_t*>(value.data()), value.size());
            std::string key;
            std::string entryValue;
            uint32_t f = 0;
            uint32_t t = 0;
            if (!e.Tag(f, t) || f != 1 || t != 2 || !e.Bytes(key) || !e.Tag(f, t) || f != 2 ||
                t != 2 || !e.Bytes(entryValue) || !e.AtEnd())
            {
                return false;
            }
            ctx.metadata[key] = entryValue;
        }
        else
        {
            return false;
        }
    }
    return haveMediaType;
}

bool
ParseGrpcEnvelope(const std::vector<uint8_t>& prefix,
                  const std::vector<uint8_t>& suffix,
                  uint32_t payloadSize,
                  IbOranEnvelopeContext& ctx)
{
    // The HEADERS frame is a placeholder of unknown length, so look for the
    // first DATA frame header whose lengths match the rest of the prefix.
    const size_t n = prefix.size();
    for (size_t h = 0; h + 9 + 5 <= n; ++h)
    {
        const uint8_t* d = prefix.data() + h;
        const uint32_t frameLength =
            (static_cast<uint32_t>(d[0]) << 16) | (static_cast<uint32_t>(d[1]) << 8) | d[2];
        if (frameLength == 0 || d[3] != 0x00 || d[4] > 0x01 || d[5] != 0 || d[6] != 0 ||
            d[7] != 0 || d[8] != 1)
        {
            continue;
        }
        // further DATA frame headers (zeros) when the message spans frames
        const size_t maxExtra = (n - h - 9 - 5) / 9;
        for (size_t extra = 0; extra <= maxExtra; ++extra)
        {
            if ((extra == 0) != (d[4] == 0x01))
            {
                continue;
            }
            if (extra > 0 &&
                !std::all_of(d + 9 * extra, d + 9 * extra + 9, [](uint8_t b) { return b == 0; }))
            {
                break; // the extra frame headers are zeros
            }
            const size_t g = h + 9 + 9 * extra;
            if (prefix[g] != 0x00)
            {
                continue;
            }
            const uint64_t protobufLength = (static_cast<uint64_t>(prefix[g + 1]) << 24) |
                                            (static_cast<uint64_t>(prefix[g + 2]) << 16) |
                                            (static_cast<uint64_t>(prefix[g + 3]) << 8) |
                                            prefix[g + 4];
            const uint64_t dataLength = 5 + protobufLength;
            if ((dataLength - 1) / kHttp2MaxFrame != extra ||
                frameLength != std::min<uint64_t>(dataLength, kHttp2MaxFrame))
            {
                continue;
            }
            // SendMessageRequest { message = 1 }
            ProtoReader r(prefix.data() + g + 5, n - g - 5);
            uint32_t field = 0;
            uint32_t wireType = 0;
            uint64_t messageLength = 0;
            if (!r.Tag(field, wireType) || field != 1 || wireType != 2 ||
                !r.Varint(messageLength) || r.Position() + messageLength != protobufLength)
            {
                continue;
            }
            const size_t m = g + 5 + r.Position();
            uint64_t partLength = 0;
            IbOranEnvelopeContext parsed;
            if (!ParseGrpcMessageHead(prefix.data() + m, n - m, payloadSize, partLength, parsed) ||
                messageLength != (n - m) + payloadSize + suffix.size() ||
                partLength != 1 + VarintSize(payloadSize) + payloadSize + suffix.size() ||
                !ParseGrpcPartTail(suffix, parsed))
            {
                continue;
            }
            parsed.host.clear(); // carried in the HPACK placeholder
            ctx = parsed;
            return true;
        }
    }
    return false;
}

} // namespace

bool
ParseEnvelope(const std::vector<uint8_t>& prefix,
              const std::vector<uint8_t>& suffix,
              uint32_t payloadSize,
              IbOranEnvelopeModel& model,
              IbOranEnvelopeContext& context)
{
    static const std::string kPost = "POST ";
    if (prefix.size() >= kPost.size() && std::equal(kPost.begin(), kPost.end(), prefix.begin()))
    {
        return ParseJsonEnvelope(std::string(prefix.begin(), prefix.end()),
                                 std::string(suffix.begin(), suffix.end()),
                                 payloadSize,
                                 model,
                                 context);
    }
    if (ParseGrpcEnvelope(prefix, suffix, payloadSize, context))
    {
        model = IbOranEnvelopeModel::A2A_GRPC;
        return true;
    }
    return false;
}

bool
ParseEnvelopeModel(const std::string& str, IbOranEnvelopeModel& out)
{
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (s == "none" || s.empty())
    {
        out = IbOranEnvelopeModel::NONE;
    }
    else if (s == "a2a-jsonrpc")
    {
        out = IbOranEnvelopeModel::A2A_JSONRPC;
    }
    else if (s == "a2a-httpjson")
    {
        out = IbOranEnvelopeModel::A2A_HTTP_JSON;
    }
    else if (s == "a2a-grpc")
    {
        out = IbOranEnvelopeModel::A2A_GRPC;
    }
    else
    {
        return false;
    }
    return true;
}

std::string
EnvelopeModelToString(IbOranEnvelopeModel model)
{
    switch (model)
    {
    case IbOranEnvelopeModel::A2A_JSONRPC:
        return "A2A-JSONRPC";
    case IbOranEnvelopeModel::A2A_HTTP_JSON:
        return "A2A-HTTPJSON";
    case IbOranEnvelopeModel::A2A_GRPC:
        return "A2A-gRPC";
    case IbOranEnvelopeModel::NONE:
    default:
        return "None";
    }
}

IbOranEnvelopeLayout
BuildEnvelope(IbOranEnvelopeModel model,
              const IbOranEnvelopeContext& context,
              uint32_t payloadSize,
              const IbOranEnvelopeOptions& options)
{
    switch (model)
    {
    case IbOranEnvelopeModel::A2A_JSONRPC:
        return BuildJsonEnvelope(true, context, payloadSize, options);
    case IbOranEnvelopeModel::A2A_HTTP_JSON:
        return BuildJsonEnvelope(false, context, payloadSize, options);
    case IbOranEnvelopeModel::A2A_GRPC:
        return BuildGrpcEnvelope(context, payloadSize, options);
    case IbOranEnvelopeModel::NONE:
    default:
        return IbOranEnvelopeLayout{};
    }
}

std::vector<uint8_t>
BuildWireMessage(const IbOranEnvelopeLayout& layout, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> wire = layout.prefix;
    if (layout.encoding == IbOranPayloadEncoding::BASE64)
    {
        const std::vector<uint8_t> encoded = Base64Encode(payload);
        wire.insert(wire.end(), encoded.begin(), encoded.end());
    }
    else
    {
        wire.insert(wire.end(), payload.begin(), payload.end());
    }
    wire.insert(wire.end(), layout.suffix.begin(), layout.suffix.end());
    return wire;
}

IbOranCriticalityMap
MapCriticalityToWire(const IbOranCriticalityMap& payloadMap,
                     const IbOranEnvelopeLayout& layout,
                     uint32_t payloadSize)
{
    IbOranCriticalityMap wireMap;
    const uint32_t p = static_cast<uint32_t>(layout.prefix.size());
    const bool base64 = layout.encoding == IbOranPayloadEncoding::BASE64;
    const uint32_t encodedSize = base64 ? Base64EncodedSize(payloadSize) : payloadSize;

    if (p > 0)
    {
        wireMap.Add(0, p, IbOranCriticalityMap::kMostCritical);
    }
    if (!layout.suffix.empty())
    {
        wireMap.Add(p + encodedSize,
                    p + encodedSize + static_cast<uint32_t>(layout.suffix.size()),
                    IbOranCriticalityMap::kMostCritical);
    }
    for (const auto& r : payloadMap.GetRanges())
    {
        uint64_t s = r.start;
        uint64_t e = r.end;
        if (base64)
        {
            s = 4 * (s / 3);
            e = 4 * ((e + 2) / 3);
        }
        s = std::min<uint64_t>(s, encodedSize);
        e = std::min<uint64_t>(e, encodedSize);
        wireMap.Add(static_cast<uint32_t>(p + s), static_cast<uint32_t>(p + e), r.criticality);
    }
    return wireMap;
}

std::string
JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            }
            else
            {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

} // namespace ns3
