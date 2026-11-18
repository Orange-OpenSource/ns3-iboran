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

#ifndef IBORAN_APP_ENVELOPE_H
#define IBORAN_APP_ENVELOPE_H

#include "iboran-app-content.h"
#include "iboran-app-message.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ns3
{

enum class IbOranEnvelopeModel
{
    NONE,          /**< payload sent as it is */
    A2A_JSONRPC,   /**< A2A JSON-RPC 2.0 binding over HTTP/1.1 */
    A2A_HTTP_JSON, /**< A2A HTTP+JSON (REST) binding over HTTP/1.1 */
    A2A_GRPC,      /**< A2A gRPC binding over HTTP/2 (size model) */
};

bool ParseEnvelopeModel(const std::string& str, IbOranEnvelopeModel& out);
std::string EnvelopeModelToString(IbOranEnvelopeModel model);

struct IbOranEnvelopeContext
{
    uint32_t messageId = 0;
    uint32_t taskId = 0; /**< 0 = no taskId field */
    std::string contextId;
    std::string role = "ROLE_USER";
    std::string mediaType = "application/octet-stream";
    std::map<std::string, std::string> metadata;
    std::string host = "iboran.local";
};

struct IbOranEnvelopeOptions
{
    /** Bytes assumed for the HTTP/2 HEADERS frame (HPACK) of a gRPC call. */
    uint32_t grpcHeaderBytes = 64;
    /** Length of the bearer token placeholder in HTTP/1.1 requests (0 = no
     * header). */
    uint32_t bearerTokenLength = 32;
};

struct IbOranEnvelopeLayout
{
    IbOranPayloadEncoding encoding = IbOranPayloadEncoding::IDENTITY;
    std::vector<uint8_t> prefix;
    std::vector<uint8_t> suffix;
};

/**
 * Build the envelope around a payload of payloadSize bytes. The wire message
 * is prefix + encode(payload) + suffix.
 */
IbOranEnvelopeLayout BuildEnvelope(IbOranEnvelopeModel model,
                                   const IbOranEnvelopeContext& context,
                                   uint32_t payloadSize,
                                   const IbOranEnvelopeOptions& options);

/** Assemble the full wire message. */
std::vector<uint8_t> BuildWireMessage(const IbOranEnvelopeLayout& layout,
                                      const std::vector<uint8_t>& payload);

/**
 * Map a payload criticality map to wire offsets. The envelope prefix and
 * suffix are marked most critical, since the receiver needs both to parse the
 * message.
 */
IbOranCriticalityMap MapCriticalityToWire(const IbOranCriticalityMap& payloadMap,
                                          const IbOranEnvelopeLayout& layout,
                                          uint32_t payloadSize);

/**
 * Read back an envelope built by BuildEnvelope. The model is detected from the
 * bytes: an HTTP/1.1 POST to /a2a/jsonrpc or /message:send for the JSON
 * bindings, an HTTP/2 DATA frame with a gRPC message for A2A-gRPC.
 */
bool ParseEnvelope(const std::vector<uint8_t>& prefix,
                   const std::vector<uint8_t>& suffix,
                   uint32_t payloadSize,
                   IbOranEnvelopeModel& model,
                   IbOranEnvelopeContext& context);

std::string JsonEscape(const std::string& s);

} // namespace ns3

#endif // IBORAN_APP_ENVELOPE_H
