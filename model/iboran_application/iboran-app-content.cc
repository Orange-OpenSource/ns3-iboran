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

#include "iboran-app-content.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ns3
{

namespace
{

std::string
ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool
StartsWith(const std::vector<uint8_t>& data, std::initializer_list<uint8_t> magic, size_t at = 0)
{
    if (data.size() < at + magic.size())
    {
        return false;
    }
    size_t i = at;
    for (uint8_t b : magic)
    {
        if (data[i++] != b)
        {
            return false;
        }
    }
    return true;
}

uint32_t
ReadBe32(const std::vector<uint8_t>& d, size_t at)
{
    return (static_cast<uint32_t>(d[at]) << 24) | (static_cast<uint32_t>(d[at + 1]) << 16) |
           (static_cast<uint32_t>(d[at + 2]) << 8) | static_cast<uint32_t>(d[at + 3]);
}

uint32_t
ReadBe16(const std::vector<uint8_t>& d, size_t at)
{
    return (static_cast<uint32_t>(d[at]) << 8) | static_cast<uint32_t>(d[at + 1]);
}

uint8_t
LogDecay(uint32_t index, uint8_t base)
{
    // base for index 0, then base + floor(log2(index + 1)), capped at 9
    uint32_t extra = 0;
    for (uint32_t v = index + 1; v > 1; v >>= 1)
    {
        ++extra;
    }
    return static_cast<uint8_t>(std::min<uint32_t>(9, base + extra));
}

} // namespace

// == DATA FORMAT
// ===============================================================
std::string
GetFileExtension(DataFormat format)
{
    switch (format)
    {
    case DataFormat::IMAGE:
        return ".png";
    case DataFormat::VIDEO:
        return ".mp4";
    case DataFormat::EMBEDDING:
        return ".f32";
    case DataFormat::TEXT:
        return ".txt";
    case DataFormat::BINARY:
    default:
        return ".bin";
    }
}

std::string
DataFormatToString(DataFormat format)
{
    switch (format)
    {
    case DataFormat::IMAGE:
        return "IMAGE";
    case DataFormat::VIDEO:
        return "VIDEO";
    case DataFormat::EMBEDDING:
        return "EMBEDDING";
    case DataFormat::TEXT:
        return "TEXT";
    case DataFormat::BINARY:
        return "BINARY";
    default:
        return "UNKNOWN";
    }
}

bool
ParseDataFormat(const std::string& str, DataFormat& out)
{
    const std::string s = ToLower(str);
    if (s == "image")
    {
        out = DataFormat::IMAGE;
    }
    else if (s == "video")
    {
        out = DataFormat::VIDEO;
    }
    else if (s == "embedding")
    {
        out = DataFormat::EMBEDDING;
    }
    else if (s == "text")
    {
        out = DataFormat::TEXT;
    }
    else if (s == "binary")
    {
        out = DataFormat::BINARY;
    }
    else
    {
        return false;
    }
    return true;
}

uint8_t
GetQciForDataFormat(DataFormat format)
{
    switch (format)
    {
    case DataFormat::IMAGE:
    case DataFormat::EMBEDDING:
    case DataFormat::TEXT:
        return 3;
    case DataFormat::VIDEO:
        return 2;
    case DataFormat::BINARY:
    default:
        return 9;
    }
}

bool
IsGbrBearer(uint8_t qci)
{
    return (qci >= 1 && qci <= 4);
}

// == CONTENT TYPE
// ==============================================================
std::string
ContentTypeToString(IbOranContentType type)
{
    switch (type)
    {
    case IbOranContentType::RAW:
        return "RAW";
    case IbOranContentType::PNG:
        return "PNG";
    case IbOranContentType::JPEG:
        return "JPEG";
    case IbOranContentType::BMP:
        return "BMP";
    case IbOranContentType::H264_ANNEXB:
        return "H264_ANNEXB";
    case IbOranContentType::MP4:
        return "MP4";
    case IbOranContentType::EMBEDDING_F32:
        return "EMBEDDING_F32";
    case IbOranContentType::EMBEDDING_F16:
        return "EMBEDDING_F16";
    case IbOranContentType::EMBEDDING_U8:
        return "EMBEDDING_U8";
    case IbOranContentType::TEXT_UTF8:
        return "TEXT_UTF8";
    case IbOranContentType::JSON:
        return "JSON";
    case IbOranContentType::UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

std::string
ContentTypeToMediaType(IbOranContentType type)
{
    switch (type)
    {
    case IbOranContentType::PNG:
        return "image/png";
    case IbOranContentType::JPEG:
        return "image/jpeg";
    case IbOranContentType::BMP:
        return "image/bmp";
    case IbOranContentType::H264_ANNEXB:
        return "video/h264";
    case IbOranContentType::MP4:
        return "video/mp4";
    case IbOranContentType::EMBEDDING_F32:
        return "application/vnd.iboran.embedding+f32";
    case IbOranContentType::EMBEDDING_F16:
        return "application/vnd.iboran.embedding+f16";
    case IbOranContentType::EMBEDDING_U8:
        return "application/vnd.iboran.embedding+u8";
    case IbOranContentType::TEXT_UTF8:
        return "text/plain";
    case IbOranContentType::JSON:
        return "application/json";
    case IbOranContentType::RAW:
    case IbOranContentType::UNKNOWN:
    default:
        return "application/octet-stream";
    }
}

IbOranContentType
ContentTypeFromByte(uint8_t value)
{
    if (value > static_cast<uint8_t>(IbOranContentType::JSON))
    {
        return IbOranContentType::UNKNOWN;
    }
    return static_cast<IbOranContentType>(value);
}

DataFormat
GetDataFormatForContentType(IbOranContentType type)
{
    switch (type)
    {
    case IbOranContentType::PNG:
    case IbOranContentType::JPEG:
    case IbOranContentType::BMP:
        return DataFormat::IMAGE;
    case IbOranContentType::H264_ANNEXB:
    case IbOranContentType::MP4:
        return DataFormat::VIDEO;
    case IbOranContentType::EMBEDDING_F32:
    case IbOranContentType::EMBEDDING_F16:
    case IbOranContentType::EMBEDDING_U8:
        return DataFormat::EMBEDDING;
    case IbOranContentType::TEXT_UTF8:
    case IbOranContentType::JSON:
        return DataFormat::TEXT;
    case IbOranContentType::RAW:
    case IbOranContentType::UNKNOWN:
    default:
        return DataFormat::BINARY;
    }
}

std::string
GetFileExtension(IbOranContentType type)
{
    switch (type)
    {
    case IbOranContentType::PNG:
        return ".png";
    case IbOranContentType::JPEG:
        return ".jpg";
    case IbOranContentType::BMP:
        return ".bmp";
    case IbOranContentType::H264_ANNEXB:
        return ".h264";
    case IbOranContentType::MP4:
        return ".mp4";
    case IbOranContentType::EMBEDDING_F32:
        return ".f32";
    case IbOranContentType::EMBEDDING_F16:
        return ".f16";
    case IbOranContentType::EMBEDDING_U8:
        return ".u8";
    case IbOranContentType::TEXT_UTF8:
        return ".txt";
    case IbOranContentType::JSON:
        return ".json";
    case IbOranContentType::RAW:
    case IbOranContentType::UNKNOWN:
    default:
        return ".bin";
    }
}

uint32_t
GetElementSize(IbOranContentType type)
{
    switch (type)
    {
    case IbOranContentType::EMBEDDING_F32:
        return 4;
    case IbOranContentType::EMBEDDING_F16:
        return 2;
    default:
        return 1;
    }
}

bool
IsFloatEmbedding(IbOranContentType type)
{
    return type == IbOranContentType::EMBEDDING_F32 || type == IbOranContentType::EMBEDDING_F16;
}

IbOranContentType
DetectContentType(const std::vector<uint8_t>& data, const std::string& extension, DataFormat hint)
{
    const std::string ext = ToLower(extension);

    // 1. magic bytes
    if (StartsWith(data, {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}))
    {
        return IbOranContentType::PNG;
    }
    if (StartsWith(data, {0xFF, 0xD8, 0xFF}))
    {
        return IbOranContentType::JPEG;
    }
    if (StartsWith(data, {'f', 't', 'y', 'p'}, 4))
    {
        return IbOranContentType::MP4;
    }
    if ((ext == ".bmp" || hint == DataFormat::IMAGE) && StartsWith(data, {'B', 'M'}))
    {
        return IbOranContentType::BMP;
    }
    const bool annexB =
        StartsWith(data, {0x00, 0x00, 0x00, 0x01}) || StartsWith(data, {0x00, 0x00, 0x01});
    if (annexB && (ext == ".h264" || ext == ".264" || hint == DataFormat::VIDEO))
    {
        return IbOranContentType::H264_ANNEXB;
    }

    // 2. extension
    if (ext == ".f32")
    {
        return IbOranContentType::EMBEDDING_F32;
    }
    if (ext == ".f16")
    {
        return IbOranContentType::EMBEDDING_F16;
    }
    if (ext == ".u8" || ext == ".i8" || ext == ".pq" || ext == ".codes")
    {
        return IbOranContentType::EMBEDDING_U8;
    }
    if (ext == ".json")
    {
        return IbOranContentType::JSON;
    }
    if (ext == ".txt" || ext == ".md" || ext == ".csv")
    {
        return IbOranContentType::TEXT_UTF8;
    }

    // 3. configured format
    switch (hint)
    {
    case DataFormat::EMBEDDING:
        return IbOranContentType::EMBEDDING_F32;
    case DataFormat::TEXT:
        return IbOranContentType::TEXT_UTF8;
    default:
        return IbOranContentType::RAW;
    }
}

std::vector<uint8_t>
GetMissingElementPattern(IbOranContentType type, bool useNan)
{
    if (useNan && type == IbOranContentType::EMBEDDING_F32)
    {
        return {0x00, 0x00, 0xC0, 0x7F}; // quiet NaN, little-endian
    }
    if (useNan && type == IbOranContentType::EMBEDDING_F16)
    {
        return {0x00, 0x7E}; // quiet NaN, little-endian
    }
    return std::vector<uint8_t>(GetElementSize(type), 0x00);
}

// == CRITICALITY MAP
// ===========================================================
void
IbOranCriticalityMap::Clear()
{
    m_ranges.clear();
}

void
IbOranCriticalityMap::Add(uint32_t start, uint32_t end, uint8_t criticality)
{
    if (end <= start)
    {
        return;
    }
    criticality = std::clamp<uint8_t>(criticality, kMostCritical, kDefault);
    m_ranges.push_back({start, end, criticality});
}

bool
IbOranCriticalityMap::Empty() const
{
    return m_ranges.empty();
}

uint8_t
IbOranCriticalityMap::Get(uint32_t offset, uint32_t length) const
{
    uint8_t criticality = kDefault;
    const uint64_t end = static_cast<uint64_t>(offset) + length;
    for (const auto& range : m_ranges)
    {
        if (offset < range.end && end > range.start)
        {
            criticality = std::min(criticality, range.criticality);
        }
    }
    return std::max(kMostCritical, criticality);
}

const std::vector<IbOranCriticalityRange>&
IbOranCriticalityMap::GetRanges() const
{
    return m_ranges;
}

IbOranCriticalityMap
BuildPngCriticalityMap(const std::vector<uint8_t>& data)
{
    IbOranCriticalityMap map;
    if (!StartsWith(data, {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}))
    {
        return map;
    }
    const uint64_t size = data.size();
    map.Add(0, 8, 10); // signature

    uint64_t offset = 8;
    bool firstIdat = true;
    uint32_t idatIndex = 0;

    while (offset + 8 <= size)
    {
        const uint64_t chunkLen = ReadBe32(data, offset);
        const std::string chunkType(reinterpret_cast<const char*>(&data[offset + 4]), 4);
        const uint64_t chunkEnd = std::min<uint64_t>(size, offset + 12 + chunkLen);

        uint8_t criticality;
        if (chunkType == "IHDR")
        {
            criticality = 1;
        }
        else if (chunkType == "PLTE")
        {
            criticality = 2;
        }
        else if (chunkType == "IDAT")
        {
            if (firstIdat)
            {
                criticality = 2;
                firstIdat = false;
            }
            else
            {
                criticality = LogDecay(idatIndex, 3);
                ++idatIndex;
            }
        }
        else if (chunkType == "IEND")
        {
            criticality = 7;
        }
        else
        {
            criticality = 9;
        }

        map.Add(static_cast<uint32_t>(offset), static_cast<uint32_t>(chunkEnd), criticality);
        if (chunkEnd >= size || chunkType == "IEND")
        {
            break;
        }
        offset = chunkEnd;
    }
    return map;
}

IbOranCriticalityMap
BuildJpegCriticalityMap(const std::vector<uint8_t>& data)
{
    IbOranCriticalityMap map;
    if (!StartsWith(data, {0xFF, 0xD8}))
    {
        return map;
    }
    const uint64_t size = data.size();
    map.Add(0, 2, 1); // SOI

    uint64_t offset = 2;
    uint32_t scanIndex = 0;

    while (offset + 1 < size)
    {
        if (data[offset] != 0xFF)
        {
            break; // not at a marker: malformed, stop parsing
        }
        // skip fill bytes
        while (offset + 1 < size && data[offset + 1] == 0xFF)
        {
            ++offset;
        }
        if (offset + 1 >= size)
        {
            break;
        }
        const uint8_t marker = data[offset + 1];

        if (marker == 0xD9) // EOI
        {
            map.Add(static_cast<uint32_t>(offset), static_cast<uint32_t>(offset + 2), 7);
            break;
        }
        if ((marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) // standalone markers
        {
            map.Add(static_cast<uint32_t>(offset), static_cast<uint32_t>(offset + 2), 9);
            offset += 2;
            continue;
        }
        if (offset + 4 > size)
        {
            break;
        }
        const uint64_t segEnd = std::min<uint64_t>(size, offset + 2 + ReadBe16(data, offset + 2));

        uint8_t criticality = 9;
        if (marker == 0xDB || marker == 0xC4 || marker == 0xDD || marker == 0xDA ||
            (marker >= 0xC0 && marker <= 0xCF && marker != 0xC8 && marker != 0xCC))
        {
            criticality = 1; // DQT, DHT, DRI, SOS header, SOFn
        }
        map.Add(static_cast<uint32_t>(offset), static_cast<uint32_t>(segEnd), criticality);

        if (marker != 0xDA)
        {
            offset = segEnd;
            continue;
        }

        // entropy-coded data until a marker other than FF00 or RSTn
        uint64_t j = segEnd;
        while (j + 1 < size)
        {
            if (data[j] == 0xFF && data[j + 1] != 0x00 &&
                !(data[j + 1] >= 0xD0 && data[j + 1] <= 0xD7))
            {
                break;
            }
            ++j;
        }
        if (j + 1 >= size)
        {
            j = size;
        }
        const uint8_t scanCriticality = (scanIndex == 0) ? 2 : LogDecay(scanIndex - 1, 3);
        map.Add(static_cast<uint32_t>(segEnd), static_cast<uint32_t>(j), scanCriticality);
        ++scanIndex;
        offset = j;
    }
    return map;
}

IbOranCriticalityMap
BuildH264CriticalityMap(const std::vector<uint8_t>& data)
{
    IbOranCriticalityMap map;
    const uint64_t size = data.size();

    // positions of NAL payloads (byte after each 00 00 01) and of their start
    // codes
    std::vector<std::pair<uint64_t, uint64_t>> nals; // {startCodePos, nalHeaderPos}
    for (uint64_t i = 0; i + 2 < size; ++i)
    {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01)
        {
            const uint64_t startCode = (i > 0 && data[i - 1] == 0x00) ? i - 1 : i;
            nals.emplace_back(startCode, i + 3);
            i += 2;
        }
    }
    for (size_t n = 0; n < nals.size(); ++n)
    {
        const uint64_t begin = nals[n].first;
        const uint64_t end = (n + 1 < nals.size()) ? nals[n + 1].first : size;
        if (nals[n].second >= size)
        {
            break;
        }
        const uint8_t nalType = data[nals[n].second] & 0x1F;
        uint8_t criticality;
        switch (nalType)
        {
        case 7: // SPS
        case 8: // PPS
            criticality = 1;
            break;
        case 5: // IDR slice
            criticality = 2;
            break;
        case 1: // non-IDR slice
            criticality = 5;
            break;
        case 6: // SEI
        case 9: // access unit delimiter
            criticality = 9;
            break;
        default:
            criticality = 8;
            break;
        }
        map.Add(static_cast<uint32_t>(begin), static_cast<uint32_t>(end), criticality);
    }
    return map;
}

IbOranCriticalityMap
BuildEmbeddingPrefixCriticalityMap(uint32_t sizeBytes, uint32_t elementSize)
{
    IbOranCriticalityMap map;
    if (elementSize == 0)
    {
        return map;
    }
    const uint32_t numElements = sizeBytes / elementSize;
    uint32_t begin = 0;
    uint32_t end = 8;
    uint8_t criticality = 1;
    while (begin < numElements)
    {
        const uint32_t last = (criticality >= 9) ? numElements : std::min(end, numElements);
        map.Add(begin * elementSize, last * elementSize, criticality);
        begin = last;
        end *= 2;
        ++criticality;
    }
    return map;
}

IbOranCriticalityMap
BuildCriticalityMap(IbOranContentType type,
                    const std::vector<uint8_t>& data,
                    const IbOranCriticalityOptions& options)
{
    switch (type)
    {
    case IbOranContentType::PNG:
        return BuildPngCriticalityMap(data);
    case IbOranContentType::JPEG:
        return BuildJpegCriticalityMap(data);
    case IbOranContentType::H264_ANNEXB:
        return BuildH264CriticalityMap(data);
    case IbOranContentType::EMBEDDING_F32:
    case IbOranContentType::EMBEDDING_F16:
    case IbOranContentType::EMBEDDING_U8:
        if (options.embeddingPrefix)
        {
            return BuildEmbeddingPrefixCriticalityMap(static_cast<uint32_t>(data.size()),
                                                      GetElementSize(type));
        }
        break;
    default:
        break;
    }

    IbOranCriticalityMap map;
    if (options.uniformCriticality != IbOranCriticalityMap::kDefault && !data.empty())
    {
        map.Add(0, static_cast<uint32_t>(data.size()), options.uniformCriticality);
    }
    return map;
}

} // namespace ns3
