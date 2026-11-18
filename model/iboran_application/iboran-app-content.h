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

#ifndef IBORAN_APP_CONTENT_H
#define IBORAN_APP_CONTENT_H

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{

// == DATA FORMAT
// ===============================================================
enum class DataFormat
{
    IMAGE,     /**< image data (PNG, JPEG, BMP) */
    VIDEO,     /**< video data (H.264 Annex B, MP4) */
    EMBEDDING, /**< embeddings or quantized codes */
    TEXT,      /**< text data (UTF-8, JSON) */
    BINARY,    /**< opaque bytes */
};

std::string GetFileExtension(DataFormat format);
std::string DataFormatToString(DataFormat format);
bool ParseDataFormat(const std::string& str, DataFormat& out);
uint8_t GetQciForDataFormat(DataFormat format);
bool IsGbrBearer(uint8_t qci);

// == CONTENT TYPE
// ==============================================================
enum class IbOranContentType : uint8_t
{
    UNKNOWN = 0,
    RAW = 1,
    PNG = 2,
    JPEG = 3,
    BMP = 4,
    H264_ANNEXB = 5,
    MP4 = 6,
    EMBEDDING_F32 = 7, /**< little-endian float32 vector (.f32) */
    EMBEDDING_F16 = 8, /**< little-endian float16 vector (.f16) */
    EMBEDDING_U8 = 9,  /**< 1-byte codes: PQ indices, int8/uint8 quantization */
    TEXT_UTF8 = 10,
    JSON = 11,
};

std::string ContentTypeToString(IbOranContentType type);
std::string ContentTypeToMediaType(IbOranContentType type);
IbOranContentType ContentTypeFromByte(uint8_t value);
DataFormat GetDataFormatForContentType(IbOranContentType type);
std::string GetFileExtension(IbOranContentType type);

/**
 * Smallest unit of the payload that must never be split across packets
 * (4 bytes for float32, 2 for float16, 1 otherwise).
 */
uint32_t GetElementSize(IbOranContentType type);

/** True for floating-point embedding types, where a lost element can be marked
 * NaN. */
bool IsFloatEmbedding(IbOranContentType type);

/**
 * Detect the content type. Magic bytes win over the extension, the extension
 * wins over the configured format.
 */
IbOranContentType DetectContentType(const std::vector<uint8_t>& data,
                                    const std::string& extension,
                                    DataFormat hint);

/** Byte pattern written in place of one missing element. */
std::vector<uint8_t> GetMissingElementPattern(IbOranContentType type, bool useNan);

// == CRITICALITY
// ===============================================================
// 1 is the most critical, 10 is the default (unknown, least critical).
struct IbOranCriticalityRange
{
    uint32_t start;      /**< first byte (inclusive) */
    uint32_t end;        /**< last byte (exclusive) */
    uint8_t criticality; /**< 1 (most critical) to 10 */
};

class IbOranCriticalityMap
{
  public:
    static constexpr uint8_t kMostCritical = 1;
    static constexpr uint8_t kDefault = 10;

    void Clear();
    void Add(uint32_t start, uint32_t end, uint8_t criticality);
    bool Empty() const;

    /** Most critical value among the ranges overlapping [offset, offset +
     * length). */
    uint8_t Get(uint32_t offset, uint32_t length) const;

    const std::vector<IbOranCriticalityRange>& GetRanges() const;

  private:
    std::vector<IbOranCriticalityRange> m_ranges;
};

struct IbOranCriticalityOptions
{
    /**
     * Treat embeddings as importance-ordered (Matryoshka or PCA-rotated):
     * dimensions [0, 8) get criticality 1, [8, 16) get 2, and so on, doubling
     * up to 9. When false, embeddings keep the default criticality.
     */
    bool embeddingPrefix = false;

    /** Criticality applied to types without a parser (text, raw, MP4). */
    uint8_t uniformCriticality = IbOranCriticalityMap::kDefault;
};

IbOranCriticalityMap BuildCriticalityMap(IbOranContentType type,
                                         const std::vector<uint8_t>& data,
                                         const IbOranCriticalityOptions& options);

// Individual parsers, exposed for testing.
IbOranCriticalityMap BuildPngCriticalityMap(const std::vector<uint8_t>& data);
IbOranCriticalityMap BuildJpegCriticalityMap(const std::vector<uint8_t>& data);
IbOranCriticalityMap BuildH264CriticalityMap(const std::vector<uint8_t>& data);
IbOranCriticalityMap BuildEmbeddingPrefixCriticalityMap(uint32_t sizeBytes, uint32_t elementSize);

} // namespace ns3

#endif // IBORAN_APP_CONTENT_H
