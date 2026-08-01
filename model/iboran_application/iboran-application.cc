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

#include "iboran-side-info-tracker.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/lte-ue-mac.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/string.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IbOranApplication");
NS_OBJECT_ENSURE_REGISTERED(IbOranTransmissionApplication);
NS_OBJECT_ENSURE_REGISTERED(IbOranReceptionApplication);

std::string
GetFileExtension(DataFormat format)
{
    std::string fileFormat = ".bin";
    switch (format)
    {
    case DataFormat::IMAGE:
        fileFormat = ".png";
        break;
    case DataFormat::VIDEO:
        fileFormat = ".mp4";
        break;
    case DataFormat::EMBEDDING:
        fileFormat = ".f32";
        break;
    case DataFormat::TEXT:
        fileFormat = ".txt";
        break;
    default:
        fileFormat = ".bin";
        std::cerr << "Unknown data format: Using .bin extension" << std::endl;
        break;
    }
    return fileFormat;
}

uint8_t
GetQciForDataFormat(DataFormat format)
{
    uint8_t qci = 9;
    switch (format)
    {
    case DataFormat::IMAGE:
        qci = 3;
        break;
    case DataFormat::VIDEO:
        qci = 2;
        break;
    case DataFormat::EMBEDDING:
        qci = 3;
        break;
    case DataFormat::TEXT:
        qci = 3;
        break;
    default:
        qci = 9;
        break;
    }
    return qci;
}

bool
IsGbrBearer(uint8_t qci)
{
    return (qci >= 1 && qci <= 4);
}

// == TRANSMISSION APPLICATION ==================================================
TypeId
IbOranTransmissionApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::IbOranTransmissionApplication")
            .SetParent<Application>()
            .AddConstructor<IbOranTransmissionApplication>()
            .AddAttribute("PacketSize",
                          "Size of each packet in bytes",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&IbOranTransmissionApplication::m_packetSize),
                          MakeUintegerChecker<uint32_t>(1, 65535))
            .AddAttribute("BitRateBps",
                          "Transmission bit rate in bits per second",
                          UintegerValue(1000000),
                          MakeUintegerAccessor(&IbOranTransmissionApplication::m_bitRateBps),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute(
                "MinPacketInterval",
                "Minimum interval between packets in milliseconds",
                UintegerValue(0),
                MakeUintegerAccessor(&IbOranTransmissionApplication::m_minPacketIntervalMs),
                MakeUintegerChecker<uint32_t>())
            .AddAttribute("UseUdp",
                          "Use UDP instead of TCP (recommended for criticality-aware transfer)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&IbOranTransmissionApplication::m_useUdp),
                          MakeBooleanChecker())
            .AddAttribute("RntiRetryDelayMs",
                          "Delay (ms) before first send attempt, allowing RRC/RNTI setup",
                          UintegerValue(50),
                          MakeUintegerAccessor(&IbOranTransmissionApplication::m_rntiRetryDelayMs),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

IbOranTransmissionApplication::IbOranTransmissionApplication()
    : m_format(DataFormat::IMAGE),
      m_packetSize(1024),
      m_bitRateBps(1000000),
      m_minPacketIntervalMs(0),
      m_rntiRetryDelayMs(50),
      m_dataOffset(0),
      m_txRepeat(1),
      m_repeatCount(1),
      m_txTotalBytes(0),
      m_connected(false),
      m_cachedRnti(0),
      m_useUdp(true),
      m_linkDirection(LinkDirection::UPLINK),
      m_inputImageFormat(".png")
{
    NS_LOG_FUNCTION(this);
}

IbOranTransmissionApplication::~IbOranTransmissionApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_socket)
    {
        m_socket->Close();
    }
}

void
IbOranTransmissionApplication::LoadData(std::string filePath, DataFormat format)
{
    NS_LOG_FUNCTION(this << filePath << (int)format);

    std::string extension = (format == DataFormat::IMAGE && !m_inputImageFormat.empty())
                                ? m_inputImageFormat
                                : GetFileExtension(format);

    filePath += extension;

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "Cannot open file: " << filePath << std::endl;
        return;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    m_data.resize(size);
    if (!file.read((char*)m_data.data(), size))
    {
        std::cerr << "Error reading file: " << filePath << std::endl;
        m_data.clear();
        file.close();
        return;
    }

    if (format == DataFormat::EMBEDDING && (size % 4 != 0))
    {
        std::cerr << "EMBEDDING file size " << size << " not a multiple of 4 — corrupt .f32"
                  << std::endl;
        m_data.clear();
        file.close();
        return;
    }

    file.close();
    m_format = format;
    m_dataOffset = 0;

    m_originalData = m_data;

    if (m_format == DataFormat::EMBEDDING)
    {
        EnforceEmbeddingPacketSize();
    }

    NS_LOG_INFO("Loaded data: " << filePath << " | Format: " << FormatToString(format)
                                << " | Size: " << size << " bytes");
}

void
IbOranTransmissionApplication::LoadDataFromBuffer(const uint8_t* data,
                                                  uint32_t size,
                                                  DataFormat format)
{
    NS_LOG_FUNCTION(this << (void*)data << size << (int)format);

    if (!data || size == 0)
    {
        std::cerr << "Invalid data buffer" << std::endl;
        return;
    }

    m_data.clear();
    m_data.insert(m_data.end(), data, data + size);
    m_format = format;
    m_dataOffset = 0;

    m_originalData = m_data;

    NS_LOG_INFO("Loaded data from buffer: " << size << " bytes | "
                                            << "Format: " << FormatToString(format));
}

void
IbOranTransmissionApplication::SetPeer(Address address)
{
    NS_LOG_FUNCTION(this << address);
    m_peer = address;
}

void
IbOranTransmissionApplication::SetTxRepeat(uint32_t txRepeat)
{
    NS_LOG_FUNCTION(this << txRepeat);
    m_txRepeat = txRepeat;
}

void
IbOranTransmissionApplication::SetPacketSize(uint32_t packetSize)
{
    NS_LOG_FUNCTION(this << packetSize);
    m_packetSize = packetSize;
}

void
IbOranTransmissionApplication::SetBitRate(uint32_t bitRate)
{
    NS_LOG_FUNCTION(this << bitRate);
    m_bitRateBps = bitRate;
    NS_LOG_INFO("Bit rate set to " << bitRate << " bps");
}

void
IbOranTransmissionApplication::SetMinPacketInterval(uint32_t intervalMs)
{
    NS_LOG_FUNCTION(this << intervalMs);
    m_minPacketIntervalMs = intervalMs;
}

void
IbOranTransmissionApplication::SetUseUdp(bool useUdp)
{
    m_useUdp = useUdp;
}

uint32_t
IbOranTransmissionApplication::GetTotalBytesTransmitted() const
{
    return m_txTotalBytes;
}

uint32_t
IbOranTransmissionApplication::GetRemainingDataSize() const
{
    return (m_dataOffset < m_data.size()) ? (m_data.size() - m_dataOffset) : 0;
}

DataFormat
IbOranTransmissionApplication::GetDataFormat() const
{
    return m_format;
}

std::string
IbOranTransmissionApplication::GetDataFormatName() const
{
    return FormatToString(m_format);
}

std::string
IbOranTransmissionApplication::FormatToString(DataFormat format) const
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
    default:
        return "UNKNOWN";
    }
}

void
IbOranTransmissionApplication::SetRntiGetter(std::function<uint16_t()> getter)
{
    m_rntiGetter = getter;
    m_cachedRnti = 0;
}

void
IbOranTransmissionApplication::SetLinkDirection(LinkDirection direction)
{
    m_linkDirection = direction;
}

void
IbOranTransmissionApplication::SetUeObjectId(std::string objectId)
{
    NS_LOG_FUNCTION(this << objectId);
    m_ueObjectId = objectId;
}

void
IbOranTransmissionApplication::SetInputImageFormat(std::string format)
{
    NS_LOG_FUNCTION(this << format);
    m_inputImageFormat = format;
}

// -- PROTECTED METHODS ----------------------------------------------------
void
IbOranTransmissionApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_data.empty())
    {
        std::cerr << "No data loaded. Call LoadData() or LoadDataFromBuffer() first." << std::endl;
        return;
    }

    if (m_format == DataFormat::IMAGE && GetFileExtension(m_format) == ".png")
    {
        AnalyzePngStructure();
    }

    if (m_format == DataFormat::EMBEDDING)
    {
        EnforceEmbeddingPacketSize();
    }

    //   1. TCP is a stream protocol — the receiver cannot rely on Send() boundaries
    //      matching RecvFrom() boundaries, which breaks per-packet criticality tracking.
    //   2. For small images over a simulated link, the reliability benefit of TCP
    //      is already provided by LTE/5G HARQ+RLC ARQ at lower layers.
    TypeId socketTid = m_useUdp ? UdpSocketFactory::GetTypeId() : TcpSocketFactory::GetTypeId();
    m_socket = Socket::CreateSocket(GetNode(), socketTid);

    if (m_useUdp)
    {
        // UDP: no connection handshake; bind and set peer directly.
        m_socket->Connect(m_peer);
        m_connected = true;

        // If we send immediately, GetRnti() may return 0 because the UE has not
        // yet completed RRC connection setup. The delay is configurable via
        Simulator::Schedule(MilliSeconds(m_rntiRetryDelayMs),
                            &IbOranTransmissionApplication::SendNextPacket,
                            this);
    }
    else
    {
        // TCP path (kept for backward compatibility).
        m_socket->SetConnectCallback(
            MakeCallback(&IbOranTransmissionApplication::ConnectionSucceeded, this),
            MakeCallback(&IbOranTransmissionApplication::ConnectionFailed, this));
        m_socket->SetSendCallback(MakeCallback(&IbOranTransmissionApplication::HandleSend, this));
        m_socket->Connect(m_peer);
    }

    PublishRntiObjectId();

    NS_LOG_INFO("Transmission starting: " << m_data.size() << " bytes | "
                                          << "Format: " << GetDataFormatName() << " | "
                                          << "Packet size: " << m_packetSize << " bytes | "
                                          << "Transport: " << (m_useUdp ? "UDP" : "TCP"));
}

void
IbOranTransmissionApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_sendEvent.IsPending())
    {
        m_sendEvent.Cancel();
    }

    if (m_socket)
    {
        m_socket->Close();
        m_connected = false;
    }

    NS_LOG_INFO("Transmission stopped. Total sent: " << m_txTotalBytes << " bytes");
}

// -- PRIVATE METHODS ------------------------------------------------------
void
IbOranTransmissionApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_INFO("TCP connection succeeded");
    m_connected = true;

    Simulator::Schedule(MilliSeconds(m_rntiRetryDelayMs),
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
IbOranTransmissionApplication::SendNextPacket()
{
    NS_LOG_FUNCTION(this);

    if (m_dataOffset >= m_data.size())
    {
        NS_LOG_INFO("Transmission complete: " << m_txTotalBytes << " bytes sent");
        return;
    }

    if (!m_connected)
    {
        NS_LOG_WARN("Not connected, skipping packet");
        return;
    }

    if (m_txTotalBytes == 0)
    {
        m_firstPacketTime = Simulator::Now();
    }

    uint32_t remaining = m_data.size() - m_dataOffset;
    uint32_t toSend = std::min(m_packetSize, remaining);

    Ptr<Packet> packet = Create<Packet>(m_data.data() + m_dataOffset, toSend);

    RefreshRntiCriticality(packet);
    PublishRntiCriticality();

    int actual = m_socket->Send(packet);
    if (actual == -1)
    {
        NS_LOG_WARN("Send failed (socket buffer full?), retrying in 1ms");
        // On send failure (e.g. UDP send buffer momentarily full), retry after a short backoff.
        m_sendEvent = Simulator::Schedule(MilliSeconds(1),
                                          &IbOranTransmissionApplication::SendNextPacket,
                                          this);
        return;
    }

    m_dataOffset += toSend;
    m_txTotalBytes += toSend;

    NS_LOG_DEBUG("Sent packet: " << toSend << " bytes | "
                                 << "Offset: " << m_dataOffset << "/" << m_data.size() << " | "
                                 << "Total sent: " << m_txTotalBytes << " bytes");

    // Schedule next packet if data remains.
    if (m_dataOffset < m_data.size())
    {
        double bytesPerSec = static_cast<double>(m_bitRateBps) / 8.0;
        Time interval = std::max(Seconds(static_cast<double>(toSend) / bytesPerSec),
                                 MilliSeconds(m_minPacketIntervalMs));
        m_sendEvent =
            Simulator::Schedule(interval, &IbOranTransmissionApplication::SendNextPacket, this);
    }
    // Schedule repeat if configured to do so and we've completed one full transmission.
    else if (m_txRepeat == 0 || m_repeatCount < m_txRepeat)
    {
        if (!m_originalData.empty())
        {
            NS_LOG_INFO("Repeat " << m_repeatCount << "/" << m_txRepeat
                                  << " — restarting transmission");
            m_data = m_originalData;
            m_dataOffset = 0;

            // schedule next repeat if configured to do so.
            Time repeatInterval = std::max(MilliSeconds(1.0), MilliSeconds(m_minPacketIntervalMs));
            m_sendEvent = Simulator::Schedule(repeatInterval,
                                              &IbOranTransmissionApplication::SendNextPacket,
                                              this);
            m_repeatCount++;
        }
    }
}

void
IbOranTransmissionApplication::HandleSend(Ptr<Socket> socket, uint32_t availableBytes)
{
    NS_LOG_FUNCTION(this << socket << availableBytes);
    if (!m_sendEvent.IsPending() && m_connected && m_dataOffset < m_data.size())
    {
        NS_LOG_DEBUG("Send buffer drained (" << availableBytes << " bytes free), resuming send");
        SendNextPacket();
    }
}

uint16_t
IbOranTransmissionApplication::GetRnti() const
{
    if (m_cachedRnti == 0 && m_rntiGetter)
    {
        uint16_t rnti = m_rntiGetter();
        if (rnti != 0)
        {
            m_cachedRnti = rnti;
            if (!m_ueObjectId.empty())
            {
                SideInfoTracker::GetInstance().UpdateRntiObjectId(m_cachedRnti,
                                                                  m_linkDirection,
                                                                  m_ueObjectId);
            }
        }
        return rnti;
    }
    return m_cachedRnti;
}

void
IbOranTransmissionApplication::EnforceEmbeddingPacketSize()
{
    if (m_format != DataFormat::EMBEDDING)
    {
        return;
    }

    const uint32_t kMaxEmbeddingPacket = 1500; // since we are using UDP, keep it under typical MTU
    if (m_packetSize > kMaxEmbeddingPacket)
    {
        NS_LOG_WARN("EMBEDDING packet size " << m_packetSize << " exceeds max "
                                             << kMaxEmbeddingPacket << " — clamping");
        m_packetSize = kMaxEmbeddingPacket;
    }

    // align to a 4-byte (float32) boundary so packets never split a float.
    if (m_packetSize % 4 != 0)
    {
        uint32_t adjusted = (m_packetSize / 4) * 4; // round DOWN to nearest multiple of 4
        if (adjusted == 0)
        {
            adjusted = 4; // never allow a zero-size packet
        }
        NS_LOG_WARN("EMBEDDING packet size " << m_packetSize
                                             << " not divisible by 4 — adjusting to " << adjusted);
        m_packetSize = adjusted;
    }
}

void
IbOranTransmissionApplication::AnalyzePngStructure()
{
    if (m_format != DataFormat::IMAGE || GetFileExtension(m_format) != ".png")
    {
        return;
    }

    uint32_t offset = 0;

    // PNG signature (8 bytes)
    m_criticalityMap.push_back({0, 8, 10});
    offset = 8;

    bool firstIDAT = true;
    uint32_t idatIndex = 0;

    while (offset + 8 < m_data.size())
    {
        uint32_t chunkLen = (m_data[offset] << 24) | (m_data[offset + 1] << 16) |
                            (m_data[offset + 2] << 8) | m_data[offset + 3];

        std::string chunkType(reinterpret_cast<char*>(&m_data[offset + 4]), 4);

        uint8_t criticality = 5;
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
            if (firstIDAT)
            {
                criticality = 2;
                firstIDAT = false;
            }
            else
            {
                uint8_t logDecay = static_cast<uint8_t>(std::log2(idatIndex + 1));
                criticality = std::min((uint8_t)9, (uint8_t)(3 + logDecay));
                idatIndex++;
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

        uint32_t chunkTotalLen = 12 + chunkLen;
        m_criticalityMap.push_back({offset, offset + chunkTotalLen, criticality});
        offset += chunkTotalLen;
    }
}

uint8_t
IbOranTransmissionApplication::GetCriticalityForByteRange(uint32_t byteOffset, uint32_t length)
{
    uint8_t criticality = 10;
    for (const auto& range : m_criticalityMap)
    {
        if (byteOffset < range.endByte && (byteOffset + length) > range.startByte)
        {
            criticality = std::max((uint8_t)1, std::min(criticality, range.criticality));
        }
    }
    return criticality;
}

void
IbOranTransmissionApplication::RefreshRntiCriticality(Ptr<Packet> packet)
{
    uint32_t currentOffset = m_dataOffset;
    uint32_t packetSize = packet->GetSize();

    uint8_t criticality = GetCriticalityForByteRange(currentOffset, packetSize);

    auto it = m_rntiCriticality.find({GetRnti(), m_linkDirection});
    if (it != m_rntiCriticality.end())
    {
        it->second = criticality;
    }
    else
    {
        m_rntiCriticality[{GetRnti(), m_linkDirection}] = criticality;
    }
}

void
IbOranTransmissionApplication::PublishRntiCriticality() const
{
    uint16_t rnti = GetRnti();
    if (rnti == 0)
    {
        NS_LOG_WARN("RNTI not yet assigned, skipping criticality publish");
        return;
    }

    auto it = m_rntiCriticality.find({rnti, m_linkDirection});
    if (it == m_rntiCriticality.end())
    {
        NS_LOG_WARN("No criticality entry for RNTI " << rnti);
        return;
    }

    SideInfoTracker::GetInstance().UpdateRntiCriticality(rnti, m_linkDirection, it->second);
}

void
IbOranTransmissionApplication::PublishRntiObjectId() const
{
    uint16_t rnti = GetRnti();
    if (rnti != 0 && !m_ueObjectId.empty())
    {
        SideInfoTracker::GetInstance().UpdateRntiObjectId(rnti, m_linkDirection, m_ueObjectId);
    }
}

// == RECEPTION APPLICATION =====================================================
TypeId
IbOranReceptionApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::IbOranReceptionApplication")
            .SetParent<Application>()
            .AddConstructor<IbOranReceptionApplication>()
            .AddAttribute("UseUdp",
                          "Use UDP socket (must match transmitter setting)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&IbOranReceptionApplication::m_useUdp),
                          MakeBooleanChecker());
    return tid;
}

IbOranReceptionApplication::IbOranReceptionApplication()
    : m_format(DataFormat::IMAGE),
      m_rxTotalBytes(0),
      m_packetCount(0),
      m_useUdp(true)
{
    NS_LOG_FUNCTION(this);
}

IbOranReceptionApplication::~IbOranReceptionApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_socket)
    {
        m_socket->Close();
    }
}

void
IbOranReceptionApplication::SetLocalAddress(Address address)
{
    NS_LOG_FUNCTION(this << address);
    m_local = address;
}

void
IbOranReceptionApplication::SetOutputPath(std::string path, DataFormat format)
{
    NS_LOG_FUNCTION(this << path);
    m_outputPath = path + GetFileExtension(format);
}

void
IbOranReceptionApplication::SetDataFormat(DataFormat format)
{
    NS_LOG_FUNCTION(this << (int)format);
    m_format = format;
}

void
IbOranReceptionApplication::SetUseUdp(bool useUdp)
{
    m_useUdp = useUdp;
}

uint32_t
IbOranReceptionApplication::GetTotalBytesReceived() const
{
    return m_rxTotalBytes;
}

uint32_t
IbOranReceptionApplication::GetReceivedDataSize() const
{
    return m_receivedData.size();
}

uint32_t
IbOranReceptionApplication::GetPacketCount() const
{
    return m_packetCount;
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

// -- PROTECTED METHODS ----------------------------------------------------
void
IbOranReceptionApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);

    TypeId socketTid = m_useUdp ? UdpSocketFactory::GetTypeId() : TcpSocketFactory::GetTypeId();
    m_socket = Socket::CreateSocket(GetNode(), socketTid);

    if (m_socket->Bind(m_local) == -1)
    {
        std::cerr << "Failed to bind socket" << std::endl;
        return;
    }

    if (m_useUdp)
    {
        // UDP receiver — no Listen()/Accept() needed; just set recv callback.
        m_socket->SetRecvCallback(MakeCallback(&IbOranReceptionApplication::ReceivePacket, this));
    }
    else
    {
        // TCP path unchanged.
        m_socket->Listen();
        m_socket->SetAcceptCallback(
            MakeCallback(&IbOranReceptionApplication::ConnectionRequested, this),
            MakeCallback(&IbOranReceptionApplication::NewConnectionCreated, this));
    }

    m_receivedData.clear();
    m_rxTotalBytes = 0;
    m_packetCount = 0;

    NS_LOG_INFO("Reception started on " << m_local
                                        << " | Transport: " << (m_useUdp ? "UDP" : "TCP"));
}

void
IbOranReceptionApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->Close();
    }

    SaveReceivedData();

    NS_LOG_INFO("Reception stopped. Total received: " << m_rxTotalBytes << " bytes");
}

// -- PRIVATE METHODS ------------------------------------------------------
bool
IbOranReceptionApplication::ConnectionRequested(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);
    NS_LOG_INFO("TCP connection requested from " << from);
    return true;
}

void
IbOranReceptionApplication::NewConnectionCreated(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);
    NS_LOG_INFO("New TCP connection from " << from);
    socket->SetRecvCallback(MakeCallback(&IbOranReceptionApplication::ReceivePacket, this));
}

void
IbOranReceptionApplication::ReceivePacket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    if (m_firstPacketTime.IsZero())
    {
        m_firstPacketTime = Simulator::Now();
    }
    m_lastPacketTime = Simulator::Now();

    Ptr<Packet> packet;
    Address from;

    while ((packet = socket->RecvFrom(from)))
    {
        uint32_t packetSize = packet->GetSize();
        std::vector<uint8_t> buffer(packetSize);
        packet->CopyData(buffer.data(), packetSize);

        m_receivedData.insert(m_receivedData.end(), buffer.begin(), buffer.begin() + packetSize);
        m_rxTotalBytes += packetSize;
        m_packetCount++;

        NS_LOG_DEBUG("Received packet: " << packetSize << " bytes | "
                                         << "Total: " << m_rxTotalBytes << " bytes | "
                                         << "Packets: " << m_packetCount);
    }
}

void
IbOranReceptionApplication::SaveReceivedData()
{
    NS_LOG_FUNCTION(this);

    if (m_receivedData.empty())
    {
        NS_LOG_WARN("No data received, nothing to save");
        return;
    }

    if (m_outputPath.empty())
    {
        NS_LOG_WARN("Output path not set");
        return;
    }

    std::ofstream outFile(m_outputPath, std::ios::binary);
    if (!outFile.is_open())
    {
        std::cerr << "Cannot open output file: " << m_outputPath << std::endl;
        return;
    }

    outFile.write((char*)m_receivedData.data(), m_receivedData.size());
    outFile.close();

    NS_LOG_INFO("Data saved: " << m_outputPath << " (" << m_receivedData.size() << " bytes)");
}

} // namespace ns3
