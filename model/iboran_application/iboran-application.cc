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

#include "iboran-application.h"

#include "iboran-app-header.h"
#include "iboran-side-info-tracker.h"

#include "ns3/boolean.h"
#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/string.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IbOranApplication");
NS_OBJECT_ENSURE_REGISTERED(IbOranTransmissionApplication);
NS_OBJECT_ENSURE_REGISTERED(IbOranReceptionApplication);

namespace
{

// "ip:port" for IPv4 socket addresses, the ns-3 printout otherwise
std::string
AddressToString(const Address& address)
{
    std::ostringstream os;
    if (InetSocketAddress::IsMatchingType(address))
    {
        const InetSocketAddress inet = InetSocketAddress::ConvertFrom(address);
        os << inet.GetIpv4() << ":" << inet.GetPort();
    }
    else
    {
        os << address;
    }
    return os.str();
}

} // namespace

// == TRANSMISSION APPLICATION
// ==================================================
TypeId
IbOranTransmissionApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::IbOranTransmissionApplication")
            .SetParent<Application>()
            .SetGroupName("IbOran")
            .AddConstructor<IbOranTransmissionApplication>()
            .AddAttribute("PacketSize",
                          "Datagram size in bytes, application header included",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&IbOranTransmissionApplication::m_packetSize),
                          MakeUintegerChecker<uint32_t>(1, 65535))
            .AddAttribute("BitRateBps",
                          "Sending rate in bits per second, application header included",
                          UintegerValue(1000000),
                          MakeUintegerAccessor(&IbOranTransmissionApplication::m_bitRateBps),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("MinPacketInterval",
                          "Minimum interval between two datagrams",
                          TimeValue(Seconds(0)),
                          MakeTimeAccessor(&IbOranTransmissionApplication::m_minPacketInterval),
                          MakeTimeChecker())
            .AddAttribute("UseUdp",
                          "Use UDP instead of TCP (recommended for criticality-aware "
                          "transfer)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&IbOranTransmissionApplication::m_useUdp),
                          MakeBooleanChecker())
            .AddAttribute("RntiRetryDelayMs",
                          "Delay (ms) before the first send attempt, allowing "
                          "RRC/RNTI setup",
                          UintegerValue(50),
                          MakeUintegerAccessor(&IbOranTransmissionApplication::m_rntiRetryDelayMs),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("EnableFraming",
                          "Prepend an IbOranAppHeader to every datagram. When false, raw "
                          "bytes "
                          "are sent as in the original MVP (no loss localization).",
                          BooleanValue(true),
                          MakeBooleanAccessor(&IbOranTransmissionApplication::m_framing),
                          MakeBooleanChecker())
            .AddAttribute("Envelope",
                          "Envelope around each message: None, A2A-JSONRPC, "
                          "A2A-HTTPJSON, A2A-gRPC",
                          StringValue("None"),
                          MakeStringAccessor(&IbOranTransmissionApplication::m_envelopeName),
                          MakeStringChecker())
            .AddAttribute("MaxDatagramSize",
                          "Upper bound on PacketSize over UDP, to avoid IP fragmentation "
                          "(1500-byte MTU minus IPv4 and UDP headers)",
                          UintegerValue(1472),
                          MakeUintegerAccessor(&IbOranTransmissionApplication::m_maxDatagramSize),
                          MakeUintegerChecker<uint32_t>(64, 65507))
            .AddAttribute("EmbeddingPrefixCriticality",
                          "Treat embeddings as importance-ordered (Matryoshka or "
                          "PCA): earlier "
                          "dimensions are more critical",
                          BooleanValue(false),
                          MakeBooleanAccessor(&IbOranTransmissionApplication::m_embeddingPrefix),
                          MakeBooleanChecker())
            .AddAttribute("ContextId",
                          "Default A2A contextId for messages that do not set one",
                          StringValue(""),
                          MakeStringAccessor(&IbOranTransmissionApplication::m_contextId),
                          MakeStringChecker())
            .AddAttribute("GrpcHeaderBytes",
                          "Bytes assumed for the HTTP/2 HEADERS frame of the A2A "
                          "gRPC model",
                          UintegerValue(64),
                          MakeUintegerAccessor(&IbOranTransmissionApplication::m_grpcHeaderBytes),
                          MakeUintegerChecker<uint32_t>(0, 4096))
            .AddTraceSource("Tx",
                            "A datagram is handed to the socket",
                            MakeTraceSourceAccessor(&IbOranTransmissionApplication::m_txTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource(
                "TxMessage",
                "The last fragment of a message is handed to the socket",
                MakeTraceSourceAccessor(&IbOranTransmissionApplication::m_txMessageTrace),
                "ns3::IbOranTransmissionApplication::TxMessageTracedCallback");
    return tid;
}

IbOranTransmissionApplication::IbOranTransmissionApplication()
    : m_format(DataFormat::IMAGE),
      m_contentType(IbOranContentType::UNKNOWN),
      m_packetSize(1024),
      m_bitRateBps(1000000),
      m_minPacketInterval(Seconds(0)),
      m_rntiRetryDelayMs(50),
      m_useUdp(true),
      m_framing(true),
      m_envelopeName("None"),
      m_envelope(IbOranEnvelopeModel::NONE),
      m_maxDatagramSize(1472),
      m_embeddingPrefix(false),
      m_grpcHeaderBytes(64),
      m_linkDirection(LinkDirection::UPLINK),
      m_inputImageFormat(".png"),
      m_fileLoaded(false),
      m_fileCriticalityReady(false),
      m_nextMessageId(1),
      m_running(false),
      m_connected(false),
      m_lastRnti(0),
      m_txTotalBytes(0),
      m_txPayloadBytes(0),
      m_txPackets(0),
      m_txMessages(0)
{
    NS_LOG_FUNCTION(this);
}

IbOranTransmissionApplication::~IbOranTransmissionApplication()
{
    NS_LOG_FUNCTION(this);
}

// -- DATA LOADING
// ---------------------------------------------------------------
void
IbOranTransmissionApplication::LoadData(std::string filePathStem, DataFormat format)
{
    NS_LOG_FUNCTION(this << filePathStem << static_cast<int>(format));

    std::string extension;
    if (!m_inputFileExtension.empty())
    {
        extension = m_inputFileExtension;
    }
    else if (format == DataFormat::IMAGE && !m_inputImageFormat.empty())
    {
        extension = m_inputImageFormat;
    }
    else
    {
        extension = GetFileExtension(format);
    }
    LoadFile(filePathStem + extension, format);
}

bool
IbOranTransmissionApplication::LoadFile(const std::string& filePath, DataFormat formatHint)
{
    NS_LOG_FUNCTION(this << filePath << static_cast<int>(formatHint));

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Cannot open file: " << filePath << std::endl;
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    if (file.bad())
    {
        std::cerr << "Error reading file: " << filePath << std::endl;
        return false;
    }

    const std::string extension = std::filesystem::path(filePath).extension().string();
    const IbOranContentType type = DetectContentType(data, extension, formatHint);

    const uint32_t elem = GetElementSize(type);
    if (data.size() % elem != 0)
    {
        std::cerr << ContentTypeToString(type) << " file size " << data.size()
                  << " is not a multiple of " << elem << " bytes: " << filePath << std::endl;
        return false;
    }
    if (GetDataFormatForContentType(type) != formatHint && type != IbOranContentType::RAW)
    {
        NS_LOG_WARN("File " << filePath << " detected as " << ContentTypeToString(type)
                            << " while the configured format is "
                            << DataFormatToString(formatHint));
    }

    m_fileData = std::move(data);
    m_fileLoaded = true;
    m_filePath = filePath;
    m_format = formatHint;
    m_contentType = type;
    m_fileCriticalityReady = false;

    NS_LOG_INFO("Loaded " << filePath << " | Format: " << DataFormatToString(formatHint)
                          << " | Content: " << ContentTypeToString(type)
                          << " | Size: " << m_fileData.size() << " bytes");
    return true;
}

void
IbOranTransmissionApplication::LoadDataFromBuffer(const uint8_t* data,
                                                  uint32_t size,
                                                  DataFormat format)
{
    NS_LOG_FUNCTION(this << static_cast<const void*>(data) << size << static_cast<int>(format));

    if (!data || size == 0)
    {
        std::cerr << "Invalid data buffer" << std::endl;
        return;
    }
    m_fileData.assign(data, data + size);
    m_fileLoaded = true;
    m_filePath.clear();
    m_format = format;
    m_contentType = DetectContentType(m_fileData, "", format);
    m_fileCriticalityReady = false;
}

uint32_t
IbOranTransmissionApplication::SendMessage(IbOranMessage message)
{
    NS_LOG_FUNCTION(this << message.messageId << message.payload.size());

    message.messageId = ReserveMessageId(message.messageId);
    const uint32_t id = message.messageId;
    m_queue.push_back(std::move(message));
    ScheduleSendIfIdle();
    return id;
}

// -- CONFIGURATION
// --------------------------------------------------------------
void
IbOranTransmissionApplication::SetPeer(Address address)
{
    m_peer = address;
}

void
IbOranTransmissionApplication::SetPacketSize(uint32_t packetSize)
{
    m_packetSize = packetSize;
}

void
IbOranTransmissionApplication::SetBitRate(uint32_t bitRate)
{
    m_bitRateBps = std::max<uint32_t>(1, bitRate);
}

void
IbOranTransmissionApplication::SetMinPacketInterval(uint32_t intervalMs)
{
    m_minPacketInterval = MilliSeconds(intervalMs);
}

void
IbOranTransmissionApplication::SetMinPacketIntervalTime(Time interval)
{
    m_minPacketInterval = interval;
}

void
IbOranTransmissionApplication::SetUseUdp(bool useUdp)
{
    m_useUdp = useUdp;
}

void
IbOranTransmissionApplication::SetFraming(bool enable)
{
    m_framing = enable;
}

void
IbOranTransmissionApplication::SetEnvelopeModel(IbOranEnvelopeModel model)
{
    m_envelopeName = EnvelopeModelToString(model);
    m_envelope = model;
}

void
IbOranTransmissionApplication::SetEmbeddingPrefixCriticality(bool enable)
{
    m_embeddingPrefix = enable;
    m_fileCriticalityReady = false;
}

void
IbOranTransmissionApplication::SetRntiGetter(std::function<uint16_t()> getter)
{
    m_rntiGetter = getter;
    m_lastRnti = 0;
}

void
IbOranTransmissionApplication::SetLinkDirection(LinkDirection direction)
{
    m_linkDirection = direction;
}

void
IbOranTransmissionApplication::SetUeObjectId(std::string objectId)
{
    m_ueObjectId = objectId;
}

void
IbOranTransmissionApplication::SetInputImageFormat(std::string format)
{
    m_inputImageFormat = format;
}

void
IbOranTransmissionApplication::SetInputFileExtension(std::string extension)
{
    m_inputFileExtension = extension;
}

// -- STATISTICS
// -----------------------------------------------------------------
uint32_t
IbOranTransmissionApplication::GetTotalBytesTransmitted() const
{
    return m_txTotalBytes;
}

uint32_t
IbOranTransmissionApplication::GetTotalPayloadBytesTransmitted() const
{
    return m_txPayloadBytes;
}

uint32_t
IbOranTransmissionApplication::GetTotalPacketsTransmitted() const
{
    return m_txPackets;
}

uint32_t
IbOranTransmissionApplication::GetTotalMessagesTransmitted() const
{
    return m_txMessages;
}

uint32_t
IbOranTransmissionApplication::GetLoadedDataSize() const
{
    return static_cast<uint32_t>(m_fileData.size());
}

uint32_t
IbOranTransmissionApplication::GetRemainingDataSize() const
{
    uint64_t remaining = 0;
    if (m_current)
    {
        remaining += m_current->wire.size() - m_current->offset;
    }
    for (const auto& q : m_queue)
    {
        remaining += q.payload.size();
    }
    if (!m_running && m_fileLoaded && m_queue.empty() && !m_current)
    {
        remaining += m_fileData.size();
    }
    return static_cast<uint32_t>(std::min<uint64_t>(remaining, UINT32_MAX));
}

DataFormat
IbOranTransmissionApplication::GetDataFormat() const
{
    return m_format;
}

std::string
IbOranTransmissionApplication::GetDataFormatName() const
{
    return DataFormatToString(m_format);
}

const std::vector<IbOranTransmissionApplication::SentMessage>&
IbOranTransmissionApplication::GetSentMessages() const
{
    return m_sent;
}

void
IbOranTransmissionApplication::WriteMessageLog(std::ostream& os, const std::string& indent) const
{
    const std::string in1 = indent + "  ";
    const std::string in2 = indent + "    ";
    os << "{\n"
       << in1 << "\"envelope\": \"" << EnvelopeModelToString(m_envelope) << "\",\n"
       << in1 << "\"packetsSent\": " << m_txPackets << ",\n"
       << in1 << "\"bytesSent\": " << m_txTotalBytes << ",\n"
       << in1 << "\"messages\": [";
    bool first = true;
    for (const SentMessage& m : m_sent)
    {
        os << (first ? "\n" : ",\n") << in2;
        first = false;
        os << "{\"messageId\": " << m.messageId << ", \"taskId\": ";
        if (m.taskId != 0)
        {
            os << "\"t-" << m.taskId << "\"";
        }
        else
        {
            os << "null";
        }
        os << ", \"contextId\": ";
        if (!m.contextId.empty())
        {
            os << "\"" << JsonEscape(m.contextId) << "\"";
        }
        else
        {
            os << "null";
        }
        os << ", \"round\": " << m.round << ", \"file\": " << (m.fromFile ? "true" : "false")
           << ", \"contentType\": \"" << ContentTypeToString(m.contentType)
           << "\", \"payloadSize\": " << m.payloadSize << ", \"wireSize\": " << m.wireSize
           << ", \"fragmentsSent\": " << m.fragmentsSent << ", \"firstTxNs\": " << m.firstTxNs
           << ", \"lastTxNs\": " << m.lastTxNs
           << ", \"sentAll\": " << (m.sentAll ? "true" : "false") << "}";
    }
    os << (first ? "]" : "\n" + in1 + "]") << "\n" << indent << "}";
}

IbOranContentType
IbOranTransmissionApplication::GetContentType() const
{
    return m_contentType;
}

// -- APPLICATION LIFECYCLE
// -----------------------------------------------------------------
void
IbOranTransmissionApplication::ResolveRuntimeConfig()
{
    if (!ParseEnvelopeModel(m_envelopeName, m_envelope))
    {
        NS_LOG_ERROR("Unknown envelope '" << m_envelopeName << "', using None");
        m_envelope = IbOranEnvelopeModel::NONE;
    }
    if (m_envelope != IbOranEnvelopeModel::NONE && !m_framing)
    {
        NS_LOG_WARN("Envelope " << m_envelopeName << " requires framing; enabling framing");
        m_framing = true;
    }
    if (m_envelope != IbOranEnvelopeModel::NONE && m_useUdp)
    {
        NS_LOG_WARN("A2A runs over HTTP/TCP; envelope " << m_envelopeName
                                                        << " is used here over UDP");
    }

    if (m_useUdp && m_packetSize > m_maxDatagramSize)
    {
        NS_LOG_WARN("PacketSize " << m_packetSize << " exceeds MaxDatagramSize "
                                  << m_maxDatagramSize << ", clamping");
        m_packetSize = m_maxDatagramSize;
    }

    const uint32_t headerSize = m_framing ? IbOranFragmentHeader::kSize : 0;
    const uint32_t minPacket = headerSize + 16; // room for one aligned base64 group
    if (m_packetSize < minPacket)
    {
        NS_LOG_WARN("PacketSize " << m_packetSize << " too small, using " << minPacket);
        m_packetSize = minPacket;
    }
    if (!m_framing)
    {
        // legacy mode: keep raw packets aligned to elements (e.g. float32)
        const uint32_t elem = GetElementSize(m_contentType);
        m_packetSize = std::max(elem, (m_packetSize / elem) * elem);
    }
}

void
IbOranTransmissionApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);

    ResolveRuntimeConfig();

    if (!m_fileLoaded && m_queue.empty())
    {
        NS_LOG_WARN("No data loaded; waiting for SendMessage()");
    }
    if (m_fileLoaded)
    {
        // the file is the flow's main payload: send it before messages queued
        // earlier
        EnqueueFileMessage();
        std::rotate(m_queue.begin(), std::prev(m_queue.end()), m_queue.end());
    }

    // UDP keeps datagram boundaries, which per-fragment criticality relies on.
    // Over TCP, fragment headers let the receiver rebuild message boundaries.
    TypeId socketTid = m_useUdp ? UdpSocketFactory::GetTypeId() : TcpSocketFactory::GetTypeId();
    m_socket = Socket::CreateSocket(GetNode(), socketTid);
    m_running = true;

    if (m_useUdp)
    {
        m_socket->Connect(m_peer);
        m_connected = true;
        // Sending immediately may find RNTI 0 because RRC setup has not completed.
        m_sendEvent = Simulator::Schedule(MilliSeconds(m_rntiRetryDelayMs),
                                          &IbOranTransmissionApplication::SendNextPacket,
                                          this);
    }
    else
    {
        m_socket->SetConnectCallback(
            MakeCallback(&IbOranTransmissionApplication::ConnectionSucceeded, this),
            MakeCallback(&IbOranTransmissionApplication::ConnectionFailed, this));
        m_socket->SetSendCallback(MakeCallback(&IbOranTransmissionApplication::HandleSend, this));
        m_socket->Connect(m_peer);
    }

    PublishRntiObjectId();

    NS_LOG_INFO("Transmission starting | Content: "
                << ContentTypeToString(m_contentType) << " | Packet size: " << m_packetSize
                << " bytes | Transport: " << (m_useUdp ? "UDP" : "TCP")
                << " | Framing: " << (m_framing ? "on" : "off")
                << " | Envelope: " << EnvelopeModelToString(m_envelope));
}

void
IbOranTransmissionApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);

    m_running = false;
    if (m_sendEvent.IsPending())
    {
        m_sendEvent.Cancel();
    }
    if (m_socket)
    {
        m_socket->Close();
        m_connected = false;
    }
    NS_LOG_INFO("Transmission stopped. Sent " << m_txPackets << " packets, " << m_txTotalBytes
                                              << " bytes, " << m_txMessages << " messages");
}

void
IbOranTransmissionApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    m_connected = true;
    m_sendEvent = Simulator::Schedule(MilliSeconds(m_rntiRetryDelayMs),
                                      &IbOranTransmissionApplication::SendNextPacket,
                                      this);
}

void
IbOranTransmissionApplication::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    std::cerr << "Connection failed" << std::endl;
    m_connected = false;
}

void
IbOranTransmissionApplication::HandleSend(Ptr<Socket> socket, uint32_t availableBytes)
{
    NS_LOG_FUNCTION(this << socket << availableBytes);
    if (!m_sendEvent.IsPending() && m_connected && HasPendingData())
    {
        SendNextPacket();
    }
}

void
IbOranTransmissionApplication::ScheduleSendIfIdle()
{
    if (m_running && m_connected && !m_sendEvent.IsPending() && !m_current)
    {
        m_sendEvent = Simulator::ScheduleNow(&IbOranTransmissionApplication::SendNextPacket, this);
    }
}

bool
IbOranTransmissionApplication::HasPendingData() const
{
    return m_current.has_value() || !m_queue.empty();
}

void
IbOranTransmissionApplication::EnqueueFileMessage()
{
    if (!m_fileCriticalityReady)
    {
        IbOranCriticalityOptions options;
        options.embeddingPrefix = m_embeddingPrefix;
        m_fileCriticality = BuildCriticalityMap(m_contentType, m_fileData, options);
        m_fileCriticalityReady = true;
    }

    IbOranMessage m;
    m.messageId = ReserveMessageId(0);
    m.fromFile = true;
    m.contentType = m_contentType;
    m.payload = m_fileData;
    m.criticality = m_fileCriticality;
    if (!m_ueObjectId.empty())
    {
        m.metadata["objectId"] = m_ueObjectId;
    }
    m.metadata["round"] = std::to_string(m.round);
    m.metadata["link"] = (m_linkDirection == LinkDirection::UPLINK) ? "UL" : "DL";
    m_queue.push_back(std::move(m));
}

uint32_t
IbOranTransmissionApplication::ReserveMessageId(uint32_t requested)
{
    if (requested == 0)
    {
        return m_nextMessageId++;
    }
    // A chosen ID moves the counter past it, so automatic IDs never reuse it.
    if (requested < m_nextMessageId)
    {
        NS_LOG_WARN("Message ID " << requested << " is below the next automatic ID "
                                  << m_nextMessageId << " and may reuse an earlier one");
    }
    if (requested < UINT32_MAX)
    {
        m_nextMessageId = std::max(m_nextMessageId, requested + 1);
    }
    return requested;
}

bool
IbOranTransmissionApplication::PrepareNextMessage()
{
    if (m_current)
    {
        return true;
    }
    while (!m_queue.empty())
    {
        IbOranMessage msg = std::move(m_queue.front());
        m_queue.pop_front();
        if (msg.messageId == 0)
        {
            msg.messageId = m_nextMessageId++;
        }
        if (msg.contentType == IbOranContentType::UNKNOWN)
        {
            msg.contentType = IbOranContentType::RAW;
        }

        OutgoingMessage out;
        const uint32_t payloadSize = static_cast<uint32_t>(msg.payload.size());

        if (m_framing)
        {
            IbOranEnvelopeContext ctx;
            ctx.messageId = msg.messageId;
            ctx.taskId = msg.taskId;
            ctx.contextId = msg.contextId.empty() ? m_contextId : msg.contextId;
            ctx.mediaType = ContentTypeToMediaType(msg.contentType);
            ctx.metadata = msg.metadata;
            IbOranEnvelopeOptions options;
            options.grpcHeaderBytes = m_grpcHeaderBytes;

            const IbOranEnvelopeLayout layout =
                BuildEnvelope(m_envelope, ctx, payloadSize, options);
            if (layout.prefix.size() > UINT16_MAX || layout.suffix.size() > UINT16_MAX)
            {
                NS_LOG_ERROR("Envelope too large for message " << msg.messageId << ", dropped");
                continue;
            }
            out.wire = BuildWireMessage(layout, msg.payload);
            out.criticality = MapCriticalityToWire(msg.criticality, layout, payloadSize);
            out.payloadStart = static_cast<uint32_t>(layout.prefix.size());
            out.payloadEnd = static_cast<uint32_t>(out.wire.size() - layout.suffix.size());
            out.alignmentUnit = GetWireAlignmentUnit(msg.contentType, layout.encoding);

            const uint32_t maxData = m_packetSize - IbOranFragmentHeader::kSize;
            const uint64_t fragments = (out.wire.size() + maxData - 1) / maxData;
            if (fragments > UINT16_MAX)
            {
                NS_LOG_ERROR("Message " << msg.messageId << " needs " << fragments
                                        << " fragments (max 65535), dropped");
                continue;
            }

            out.base.contentType = msg.contentType;
            out.base.encoding = layout.encoding;
            out.base.round = msg.round;
            out.base.messageId = msg.messageId;
            out.base.taskId = msg.taskId;
            out.base.payloadSize = payloadSize;
            out.base.wireSize = static_cast<uint32_t>(out.wire.size());
            out.base.prefixLength = static_cast<uint16_t>(layout.prefix.size());
            out.base.suffixLength = static_cast<uint16_t>(layout.suffix.size());
            out.base.txTimeNs = static_cast<uint64_t>(Simulator::Now().GetNanoSeconds());
            out.base.flags = static_cast<uint8_t>(
                (m_envelope != IbOranEnvelopeModel::NONE ? IbOranFragmentHeader::FLAG_ENVELOPE
                                                         : 0) |
                (msg.fromFile ? IbOranFragmentHeader::FLAG_FILE : 0));
        }
        else
        {
            out.wire = std::move(msg.payload);
            out.criticality = msg.criticality;
            out.payloadStart = 0;
            out.payloadEnd = static_cast<uint32_t>(out.wire.size());
            out.alignmentUnit = GetElementSize(msg.contentType);
            out.base.messageId = msg.messageId;
        }

        if (out.wire.empty())
        {
            NS_LOG_WARN("Message " << msg.messageId << " is empty, skipped");
            continue;
        }

        SentMessage record;
        record.messageId = msg.messageId;
        record.taskId = msg.taskId;
        record.contextId = msg.contextId.empty() ? m_contextId : msg.contextId;
        record.round = msg.round;
        record.fromFile = msg.fromFile;
        record.contentType = msg.contentType;
        record.payloadSize = payloadSize;
        record.wireSize = static_cast<uint32_t>(out.wire.size());
        out.record = m_sent.size();
        m_sent.push_back(std::move(record));

        m_current = std::move(out);
        return true;
    }
    return false;
}

void
IbOranTransmissionApplication::SendNextPacket()
{
    NS_LOG_FUNCTION(this);

    if (!m_connected)
    {
        NS_LOG_WARN("Not connected, skipping packet");
        return;
    }
    if (!PrepareNextMessage())
    {
        NS_LOG_INFO("Queue empty: " << m_txMessages << " messages sent");
        return;
    }

    OutgoingMessage& cur = *m_current;
    const uint32_t headerSize = m_framing ? IbOranFragmentHeader::kSize : 0;
    const uint32_t maxData = m_packetSize - headerSize;
    const uint32_t wireSize = static_cast<uint32_t>(cur.wire.size());

    uint32_t end = std::min(cur.offset + maxData, wireSize);
    end = AlignFragmentEnd(cur.offset, end, cur.payloadStart, cur.payloadEnd, cur.alignmentUnit);
    const uint32_t length = end - cur.offset;
    const uint8_t criticality = cur.criticality.Get(cur.offset, length);

    Ptr<Packet> packet = Create<Packet>(cur.wire.data() + cur.offset, length);
    if (m_framing)
    {
        IbOranFragmentHeader h = cur.base;
        h.flags |= (cur.offset == 0 ? IbOranFragmentHeader::FLAG_FIRST : 0) |
                   (end == wireSize ? IbOranFragmentHeader::FLAG_LAST : 0);
        h.offset = cur.offset;
        h.fragmentIndex = cur.nextFragment;
        h.fragmentLength = static_cast<uint16_t>(length);
        h.criticality = criticality;
        packet->AddHeader(IbOranAppHeader(h));
    }

    PublishRntiCriticality(criticality);

    if (m_socket->Send(packet) == -1)
    {
        // e.g. UDP send buffer momentarily full, or TCP buffer full
        NS_LOG_WARN("Send failed, retrying in 1 ms");
        m_sendEvent = Simulator::Schedule(MilliSeconds(1),
                                          &IbOranTransmissionApplication::SendNextPacket,
                                          this);
        return;
    }

    m_txTrace(packet);
    m_txPackets++;
    m_txTotalBytes += packet->GetSize();
    m_txPayloadBytes += length;
    cur.offset = end;
    cur.nextFragment++;

    SentMessage& record = m_sent[cur.record];
    const uint64_t nowNs = static_cast<uint64_t>(Simulator::Now().GetNanoSeconds());
    if (record.fragmentsSent == 0)
    {
        record.firstTxNs = nowNs;
    }
    record.lastTxNs = nowNs;
    record.fragmentsSent++;
    record.sentAll = (end >= wireSize);

    NS_LOG_DEBUG("Sent fragment " << cur.nextFragment - 1 << " of message " << cur.base.messageId
                                  << " | " << length << " bytes | " << cur.offset << "/"
                                  << wireSize);

    Time interval = std::max(Seconds(packet->GetSize() * 8.0 / m_bitRateBps), m_minPacketInterval);

    if (cur.offset >= wireSize)
    {
        m_txMessageTrace(cur.base.messageId, wireSize);
        m_txMessages++;
        m_current.reset();
    }

    if (HasPendingData())
    {
        m_sendEvent =
            Simulator::Schedule(interval, &IbOranTransmissionApplication::SendNextPacket, this);
    }
}

uint16_t
IbOranTransmissionApplication::GetRnti()
{
    const uint16_t rnti = m_rntiGetter ? m_rntiGetter() : 0;
    if (rnti != 0 && rnti != m_lastRnti)
    {
        // first assignment, or a new RNTI after handover
        m_lastRnti = rnti;
        PublishRntiObjectId();
    }
    return rnti;
}

void
IbOranTransmissionApplication::PublishRntiCriticality(uint8_t criticality)
{
    const uint16_t rnti = GetRnti();
    if (rnti == 0)
    {
        NS_LOG_WARN("RNTI not yet assigned, skipping criticality publish");
        return;
    }
    SideInfoTracker::GetInstance().UpdateRntiCriticality(rnti, m_linkDirection, criticality);
}

void
IbOranTransmissionApplication::PublishRntiObjectId() const
{
    if (m_lastRnti != 0 && !m_ueObjectId.empty())
    {
        SideInfoTracker::GetInstance().UpdateRntiObjectId(m_lastRnti,
                                                          m_linkDirection,
                                                          m_ueObjectId);
    }
}

// == RECEPTION APPLICATION
// =====================================================
TypeId
IbOranReceptionApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::IbOranReceptionApplication")
            .SetParent<Application>()
            .SetGroupName("IbOran")
            .AddConstructor<IbOranReceptionApplication>()
            .AddAttribute("UseUdp",
                          "Use UDP socket (must match transmitter setting)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&IbOranReceptionApplication::m_useUdp),
                          MakeBooleanChecker())
            .AddAttribute("EnableFraming",
                          "Expect an IbOranAppHeader on every fragment (must match "
                          "transmitter)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&IbOranReceptionApplication::m_framing),
                          MakeBooleanChecker())
            .AddAttribute("FillPolicy",
                          "Lost elements: Auto (NaN for float embeddings, zero otherwise), "
                          "Zero, Nan, Drop",
                          StringValue("Auto"),
                          MakeStringAccessor(&IbOranReceptionApplication::m_fillPolicyName),
                          MakeStringChecker())
            .AddAttribute("MessageTimeout",
                          "Deliver an incomplete message this long after its first "
                          "fragment "
                          "(0 = only when the application stops)",
                          TimeValue(Seconds(0)),
                          MakeTimeAccessor(&IbOranReceptionApplication::m_messageTimeout),
                          MakeTimeChecker())
            .AddAttribute("SaveAllMessages",
                          "Also save every message except the first as <stem>_m<id><ext>",
                          BooleanValue(false),
                          MakeBooleanAccessor(&IbOranReceptionApplication::m_saveAllMessages),
                          MakeBooleanChecker())
            .AddAttribute("IdPrefix",
                          "Prefix of the A2A task and context IDs this receiver assigns "
                          "(\"ul-ue3\" gives ul-ue3-t-1 and ul-ue3-ctx-1)",
                          StringValue(""),
                          MakeStringAccessor(&IbOranReceptionApplication::m_idPrefix),
                          MakeStringChecker())
            .AddTraceSource("Rx",
                            "A datagram or stream segment is received",
                            MakeTraceSourceAccessor(&IbOranReceptionApplication::m_rxTrace),
                            "ns3::Packet::AddressTracedCallback")
            .AddTraceSource("RxMessage",
                            "A message is delivered after reassembly",
                            MakeTraceSourceAccessor(&IbOranReceptionApplication::m_rxMessageTrace),
                            "ns3::IbOranReceptionApplication::RxMessageTracedCallback");
    return tid;
}

IbOranReceptionApplication::IbOranReceptionApplication()
    : m_format(DataFormat::IMAGE),
      m_useUdp(true),
      m_framing(true),
      m_fillPolicyName("Auto"),
      m_fillPolicy(IbOranFillPolicy::AUTO),
      m_messageTimeout(Seconds(0)),
      m_saveAllMessages(false),
      m_rxTotalBytes(0),
      m_packetCount(0),
      m_fragmentCount(0),
      m_invalidFragments(0),
      m_lateFragments(0),
      m_nextTaskNumber(1)
{
    NS_LOG_FUNCTION(this);
}

IbOranReceptionApplication::~IbOranReceptionApplication()
{
    NS_LOG_FUNCTION(this);
}

void
IbOranReceptionApplication::SetLocalAddress(Address address)
{
    m_local = address;
}

void
IbOranReceptionApplication::SetOutputPath(std::string path, DataFormat format)
{
    m_outputStem = path;
    m_format = format;
    UpdateOutputPath();
}

void
IbOranReceptionApplication::SetOutputImageFormat(std::string format)
{
    m_outputImageFormat = format;
    UpdateOutputPath();
}

void
IbOranReceptionApplication::UpdateOutputPath()
{
    if (m_outputStem.empty())
    {
        return;
    }
    // The KPI step looks for the dataset's image extension (input_image_format)
    // and for the default extension of the other formats.
    const std::string extension = (m_format == DataFormat::IMAGE && !m_outputImageFormat.empty())
                                      ? m_outputImageFormat
                                      : GetFileExtension(m_format);
    m_outputPath = m_outputStem + extension;
}

void
IbOranReceptionApplication::SetDataFormat(DataFormat format)
{
    m_format = format;
    UpdateOutputPath();
}

void
IbOranReceptionApplication::SetUseUdp(bool useUdp)
{
    m_useUdp = useUdp;
}

void
IbOranReceptionApplication::SetFraming(bool enable)
{
    m_framing = enable;
}

void
IbOranReceptionApplication::SetFillPolicy(IbOranFillPolicy policy)
{
    m_fillPolicyName = FillPolicyToString(policy);
    m_fillPolicy = policy;
}

void
IbOranReceptionApplication::SetSaveAllMessages(bool enable)
{
    m_saveAllMessages = enable;
}

void
IbOranReceptionApplication::SetMessageTimeout(Time timeout)
{
    m_messageTimeout = timeout;
}

void
IbOranReceptionApplication::SetIdPrefix(std::string prefix)
{
    m_idPrefix = std::move(prefix);
}

uint32_t
IbOranReceptionApplication::GetTotalBytesReceived() const
{
    return m_rxTotalBytes;
}

const IbOranReceivedMessage*
IbOranReceptionApplication::GetPrimaryMessage() const
{
    // The input file of the first sender. Without it there is no primary
    // output: the KPI step compares this output with the sender's input file.
    const IbOranReceivedMessage* primary = nullptr;
    for (const auto& [key, m] : m_messages)
    {
        const auto sender = m_senderIndex.find(key.first);
        if (m.fromFile && sender != m_senderIndex.end() && sender->second == 0 &&
            (!primary || m.messageId < primary->messageId))
        {
            primary = &m;
        }
    }
    return primary;
}

uint32_t
IbOranReceptionApplication::GetReceivedDataSize() const
{
    if (!m_framing)
    {
        return static_cast<uint32_t>(m_receivedData.size());
    }
    const IbOranReceivedMessage* primary = GetPrimaryMessage();
    return primary ? static_cast<uint32_t>(primary->payload.size()) : 0;
}

uint32_t
IbOranReceptionApplication::GetPacketCount() const
{
    return m_packetCount;
}

uint32_t
IbOranReceptionApplication::GetFragmentCount() const
{
    return m_fragmentCount;
}

uint32_t
IbOranReceptionApplication::GetMessageCount() const
{
    return static_cast<uint32_t>(m_messages.size());
}

uint32_t
IbOranReceptionApplication::GetCompleteMessageCount() const
{
    return static_cast<uint32_t>(std::count_if(m_messages.begin(),
                                               m_messages.end(),
                                               [](const auto& m) { return m.second.complete; }));
}

uint32_t
IbOranReceptionApplication::GetInvalidFragmentCount() const
{
    return m_invalidFragments;
}

const std::map<IbOranReceptionApplication::MessageKey, IbOranReceivedMessage>&
IbOranReceptionApplication::GetMessages() const
{
    return m_messages;
}

Time
IbOranReceptionApplication::GetTransmissionDuration() const
{
    if (m_firstPacketTime.IsZero() || m_lastPacketTime.IsZero())
    {
        return Seconds(0);
    }
    return m_lastPacketTime - m_firstPacketTime;
}

void
IbOranReceptionApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!ParseFillPolicy(m_fillPolicyName, m_fillPolicy))
    {
        NS_LOG_ERROR("Unknown fill policy '" << m_fillPolicyName << "', using Auto");
        m_fillPolicy = IbOranFillPolicy::AUTO;
    }

    TypeId socketTid = m_useUdp ? UdpSocketFactory::GetTypeId() : TcpSocketFactory::GetTypeId();
    m_socket = Socket::CreateSocket(GetNode(), socketTid);
    if (m_socket->Bind(m_local) == -1)
    {
        std::cerr << "Failed to bind socket" << std::endl;
        return;
    }

    if (m_useUdp)
    {
        m_socket->SetRecvCallback(MakeCallback(&IbOranReceptionApplication::ReceivePacket, this));
    }
    else
    {
        m_socket->Listen();
        m_socket->SetAcceptCallback(
            MakeCallback(&IbOranReceptionApplication::ConnectionRequested, this),
            MakeCallback(&IbOranReceptionApplication::NewConnectionCreated, this));
    }

    m_receivedData.clear();
    m_rxTotalBytes = 0;
    m_packetCount = 0;
    m_fragmentCount = 0;

    NS_LOG_INFO("Reception started on " << m_local << " | Transport: " << (m_useUdp ? "UDP" : "TCP")
                                        << " | Framing: " << (m_framing ? "on" : "off"));
}

void
IbOranReceptionApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->Close();
    }
    FinalizeAll();
    SaveReceivedData();

    NS_LOG_INFO("Reception stopped. Received " << m_rxTotalBytes << " bytes, " << m_messages.size()
                                               << " messages (" << GetCompleteMessageCount()
                                               << " complete), " << m_invalidFragments
                                               << " invalid fragments");
}

bool
IbOranReceptionApplication::ConnectionRequested(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);
    return true;
}

void
IbOranReceptionApplication::NewConnectionCreated(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);
    socket->SetRecvCallback(MakeCallback(&IbOranReceptionApplication::ReceivePacket, this));
}

void
IbOranReceptionApplication::ReceivePacket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        const uint32_t size = packet->GetSize();
        if (size == 0)
        {
            continue;
        }
        if (m_firstPacketTime.IsZero())
        {
            m_firstPacketTime = Simulator::Now();
        }
        m_lastPacketTime = Simulator::Now();
        m_rxTotalBytes += size;
        m_packetCount++;
        m_rxTrace(packet, from);

        std::vector<uint8_t> buffer(size);
        packet->CopyData(buffer.data(), size);

        if (!m_framing)
        {
            m_receivedData.insert(m_receivedData.end(), buffer.begin(), buffer.end());
        }
        else if (m_useUdp)
        {
            HandleDatagram(buffer.data(), size, from);
        }
        else
        {
            HandleStream(socket, buffer.data(), size, from);
        }
    }
}

void
IbOranReceptionApplication::HandleDatagram(const uint8_t* data,
                                           uint32_t length,
                                           const Address& from)
{
    IbOranFragmentHeader header;
    if (!header.Deserialize(data, length) ||
        length - IbOranFragmentHeader::kSize != header.fragmentLength)
    {
        m_invalidFragments++;
        NS_LOG_WARN("Invalid fragment of " << length << " bytes dropped");
        return;
    }
    ProcessFragment(header, data + IbOranFragmentHeader::kSize, from);
}

void
IbOranReceptionApplication::HandleStream(Ptr<Socket> socket,
                                         const uint8_t* data,
                                         uint32_t length,
                                         const Address& from)
{
    std::vector<uint8_t>& stream = m_streams[socket];
    stream.insert(stream.end(), data, data + length);

    size_t consumed = 0;
    while (stream.size() - consumed >= IbOranFragmentHeader::kSize)
    {
        IbOranFragmentHeader header;
        if (!header.Deserialize(stream.data() + consumed, IbOranFragmentHeader::kSize))
        {
            NS_LOG_ERROR("TCP stream lost fragment alignment; discarding buffered bytes");
            m_invalidFragments++;
            consumed = stream.size();
            break;
        }
        const size_t total = IbOranFragmentHeader::kSize + header.fragmentLength;
        if (stream.size() - consumed < total)
        {
            break; // wait for the rest of the fragment
        }
        ProcessFragment(header, stream.data() + consumed + IbOranFragmentHeader::kSize, from);
        consumed += total;
    }
    stream.erase(stream.begin(), stream.begin() + static_cast<std::ptrdiff_t>(consumed));
}

void
IbOranReceptionApplication::ProcessFragment(const IbOranFragmentHeader& header,
                                            const uint8_t* data,
                                            const Address& from)
{
    m_fragmentCount++;
    GetSenderIndex(from);
    const MessageKey key(from, header.messageId);
    if (m_finalized.count(key))
    {
        m_lateFragments++;
        NS_LOG_DEBUG("Fragment of already delivered message " << header.messageId << " ignored");
        return;
    }

    auto [it, inserted] = m_assemblers.try_emplace(key);
    if (inserted && header.taskId == 0)
    {
        // A new task is created when its request starts to arrive.
        m_taskNumbers[key] = m_nextTaskNumber++;
    }
    if (inserted && m_messageTimeout.IsStrictlyPositive())
    {
        m_timeouts[key] = Simulator::Schedule(m_messageTimeout,
                                              &IbOranReceptionApplication::FinalizeMessage,
                                              this,
                                              key);
    }

    const uint64_t nowNs = static_cast<uint64_t>(Simulator::Now().GetNanoSeconds());
    if (!it->second.Add(header, data, header.fragmentLength, nowNs))
    {
        m_invalidFragments++;
        NS_LOG_WARN("Fragment inconsistent with message " << header.messageId << " dropped");
        return;
    }
    if (it->second.IsComplete())
    {
        FinalizeMessage(key);
    }
}

uint32_t
IbOranReceptionApplication::GetSenderIndex(const Address& from)
{
    const auto [it, inserted] =
        m_senderIndex.try_emplace(from, static_cast<uint32_t>(m_senderIndex.size()));
    return it->second;
}

void
IbOranReceptionApplication::FinalizeMessage(MessageKey key)
{
    auto it = m_assemblers.find(key);
    if (it == m_assemblers.end())
    {
        return;
    }
    IbOranReceivedMessage message = it->second.Finalize(m_fillPolicy);
    m_assemblers.erase(it);
    m_finalized.insert(key);
    AssignA2aIds(key, message);

    auto t = m_timeouts.find(key);
    if (t != m_timeouts.end())
    {
        t->second.Cancel();
        m_timeouts.erase(t);
    }

    NS_LOG_INFO("Message " << key.second << " delivered | task " << message.a2aTaskId
                           << " | context " << message.contextId << " | "
                           << ContentTypeToString(message.contentType) << " | elements "
                           << message.elementsReceived << "/" << message.elementsTotal
                           << (message.complete ? " | complete" : ""));

    auto [stored, ok] = m_messages.emplace(key, std::move(message));
    m_rxMessageTrace(stored->second, key.first);
}

std::string
IbOranReceptionApplication::MakeA2aId(const std::string& kind, uint32_t number) const
{
    const std::string id = kind + "-" + std::to_string(number);
    return m_idPrefix.empty() ? id : m_idPrefix + "-" + id;
}

void
IbOranReceptionApplication::AssignA2aIds(const MessageKey& key, IbOranReceivedMessage& message)
{
    // Task ID: the sender's when it names one, otherwise the task created when
    // the message started to arrive.
    if (message.taskId != 0)
    {
        message.a2aTaskId = "t-" + std::to_string(message.taskId);
    }
    else
    {
        auto t = m_taskNumbers.find(key);
        uint32_t number = 0;
        if (t != m_taskNumbers.end())
        {
            number = t->second;
            m_taskNumbers.erase(t);
        }
        else
        {
            number = m_nextTaskNumber++;
        }
        message.a2aTaskId = MakeA2aId("t", number);
        message.taskIdAssigned = true;
    }
    // Context ID: the sender's when the envelope names one, otherwise one context
    // per sender.
    if (message.contextId.empty())
    {
        message.contextId = MakeA2aId("ctx", GetSenderIndex(key.first) + 1);
        message.contextIdAssigned = true;
    }
}

void
IbOranReceptionApplication::FinalizeAll()
{
    std::vector<MessageKey> pending;
    pending.reserve(m_assemblers.size());
    for (const auto& [key, assembler] : m_assemblers)
    {
        pending.push_back(key);
    }
    for (const MessageKey& key : pending)
    {
        FinalizeMessage(key);
    }
}

std::string
IbOranReceptionApplication::GetOutputFileName(const MessageKey& key,
                                              const IbOranReceivedMessage& message) const
{
    if (!m_framing || m_outputPath.empty())
    {
        return "";
    }
    if (&message == GetPrimaryMessage())
    {
        return std::filesystem::path(m_outputPath).filename().string();
    }
    if (m_saveAllMessages)
    {
        return std::filesystem::path(BuildMessagePath(key)).filename().string();
    }
    return "";
}

std::string
IbOranReceptionApplication::BuildMessagePath(const MessageKey& key) const
{
    // <stem>_m<id> for the first sender, <stem>_s<k>_m<id> for sender k > 0
    const auto sender = m_senderIndex.find(key.first);
    const uint32_t k = (sender != m_senderIndex.end()) ? sender->second : 0;
    const std::filesystem::path p(m_outputPath);
    const std::string tag =
        (k > 0 ? "_s" + std::to_string(k) : "") + "_m" + std::to_string(key.second);
    const std::filesystem::path name = p.stem().string() + tag + p.extension().string();
    return (p.parent_path() / name).string();
}

void
IbOranReceptionApplication::SaveReceivedData()
{
    NS_LOG_FUNCTION(this);

    if (m_outputPath.empty())
    {
        NS_LOG_WARN("Output path not set");
        return;
    }

    auto write = [](const std::string& path, const std::vector<uint8_t>& data) {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open())
        {
            std::cerr << "Cannot open output file: " << path << std::endl;
            return;
        }
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    };

    if (!m_framing)
    {
        if (m_receivedData.empty())
        {
            NS_LOG_WARN("No data received, nothing to save");
            return;
        }
        write(m_outputPath, m_receivedData);
        return;
    }

    if (m_messages.empty())
    {
        NS_LOG_WARN("No message received, nothing to save");
        return;
    }
    // The input file of the first sender keeps the path that the KPI step reads.
    const IbOranReceivedMessage* primary = GetPrimaryMessage();
    if (primary)
    {
        write(m_outputPath, primary->payload);
    }
    else
    {
        NS_LOG_WARN("The input file message was not received; " << m_outputPath
                                                                << " is not written");
    }
    if (m_saveAllMessages)
    {
        for (const auto& [key, m] : m_messages)
        {
            if (&m != primary)
            {
                write(BuildMessagePath(key), m.payload);
            }
        }
    }
}

void
IbOranReceptionApplication::WriteMessageLog(std::ostream& os, const std::string& indent) const
{
    const std::string in1 = indent + "  ";
    const std::string in2 = indent + "    ";
    os << "{\n"
       << in1 << "\"framing\": " << (m_framing ? "true" : "false") << ",\n"
       << in1 << "\"packetsReceived\": " << m_packetCount << ",\n"
       << in1 << "\"fragmentsReceived\": " << m_fragmentCount << ",\n"
       << in1 << "\"bytesReceived\": " << m_rxTotalBytes << ",\n"
       << in1 << "\"invalidFragments\": " << m_invalidFragments << ",\n"
       << in1 << "\"lateFragments\": " << m_lateFragments << ",\n"
       << in1 << "\"messages\": [";
    bool first = true;
    for (const auto& [key, m] : m_messages)
    {
        const auto sender = m_senderIndex.find(key.first);
        os << (first ? "\n" : ",\n") << in2;
        first = false;
        os << "{\"sender\": " << (sender != m_senderIndex.end() ? sender->second : 0)
           << ", \"from\": \"" << AddressToString(key.first) << "\", \"messageId\": " << key.second
           << ", \"round\": " << m.round << ", \"file\": " << (m.fromFile ? "true" : "false")
           << ", \"contentType\": \"" << ContentTypeToString(m.contentType)
           << "\", \"payloadSize\": " << m.payloadSize << ", \"wireSize\": " << m.wireSize
           << ", \"wireBytesReceived\": " << m.wireBytesReceived
           << ", \"fragmentsReceived\": " << m.fragmentsReceived
           << ", \"elementsTotal\": " << m.elementsTotal
           << ", \"elementsReceived\": " << m.elementsReceived
           << ", \"complete\": " << (m.complete ? "true" : "false") << ", \"missingRanges\": [";
        for (size_t i = 0; i < m.missingRanges.size(); ++i)
        {
            os << (i ? ", [" : "[") << m.missingRanges[i].first << ", " << m.missingRanges[i].second
               << "]";
        }
        os << "], \"envelope\": \"" << m.envelope
           << "\", \"envelopeValid\": " << (m.envelopeValid ? "true" : "false")
           << ", \"taskId\": \"" << JsonEscape(m.a2aTaskId)
           << "\", \"taskIdAssigned\": " << (m.taskIdAssigned ? "true" : "false")
           << ", \"contextId\": \"" << JsonEscape(m.contextId)
           << "\", \"contextIdAssigned\": " << (m.contextIdAssigned ? "true" : "false")
           << ", \"role\": \"" << JsonEscape(m.role) << "\", \"mediaType\": \""
           << JsonEscape(m.mediaType) << "\", \"metadata\": {";
        bool firstEntry = true;
        for (const auto& [k, v] : m.metadata)
        {
            os << (firstEntry ? "" : ", ") << "\"" << JsonEscape(k) << "\": \"" << JsonEscape(v)
               << "\"";
            firstEntry = false;
        }
        const std::string outputFile = GetOutputFileName(key, m);
        os << "}, \"outputFile\": ";
        if (outputFile.empty())
        {
            os << "null";
        }
        else
        {
            os << "\"" << JsonEscape(outputFile) << "\"";
        }
        os << ", \"txTimeNs\": " << m.txTimeNs << ", \"firstRxNs\": " << m.firstRxNs
           << ", \"lastRxNs\": " << m.lastRxNs << ", \"latencyMs\": " << std::fixed
           << std::setprecision(3) << static_cast<double>(m.GetLatencyNs()) / 1e6 << "}";
    }
    os << (first ? "]" : "\n" + in1 + "]") << "\n" << indent << "}";
}

} // namespace ns3
