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

#ifndef IBORAN_APPLICATION_H
#define IBORAN_APPLICATION_H

#include "../iboran_interfaces/iboran-shm-structure.h"

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace ns3
{

enum class DataFormat
{
    IMAGE,     /**< image data (PNG, JPG, BMP) */
    VIDEO,     /**< video data (H.264, H.265 frames) */
    EMBEDDING, /**< embeddings (float vectors) */
    TEXT,      /**< text data */
};

std::string GetFileExtension(DataFormat format);
uint8_t GetQciForDataFormat(DataFormat format);
bool IsGbrBearer(uint8_t qci);

// == TRANSMISSION APPLICATION ==================================================
class IbOranTransmissionApplication : public Application
{
  public:
    static TypeId GetTypeId();

    IbOranTransmissionApplication();
    virtual ~IbOranTransmissionApplication();

    // load data from file or memory buffer
    void LoadData(std::string filePath, DataFormat format);
    void LoadDataFromBuffer(const uint8_t* data, uint32_t size, DataFormat format);

    // set destination address and port
    void SetPeer(Address address);

    void SetTxRepeat(uint32_t txRepeat);
    void SetPacketSize(uint32_t packetSize);
    void SetBitRate(uint32_t bitRate);
    void SetMinPacketInterval(uint32_t intervalMs);
    void SetUseUdp(bool useUdp);

    uint32_t GetTotalBytesTransmitted() const;
    uint32_t GetRemainingDataSize() const;

    DataFormat GetDataFormat() const;
    std::string GetDataFormatName() const;

    void SetRntiGetter(std::function<uint16_t()> getter);
    void SetLinkDirection(LinkDirection direction);

    void SetUeObjectId(std::string objectId);
    void SetInputImageFormat(std::string format);

  protected:
    void StartApplication() override;
    void StopApplication() override;

  private:
    void ConnectionSucceeded(Ptr<Socket> socket);
    void ConnectionFailed(Ptr<Socket> socket);
    void SendNextPacket();
    void HandleSend(Ptr<Socket> socket, uint32_t unused);

    uint16_t GetRnti() const;
    void RefreshRntiCriticality(Ptr<Packet> packet);
    void PublishRntiCriticality() const;
    void PublishRntiObjectId() const;

    // ensure that the packet size is compatible with the EMBEDDING format
    void EnforceEmbeddingPacketSize();

    void AnalyzePngStructure();
    uint8_t GetCriticalityForByteRange(uint32_t byteOffset, uint32_t length);

    // convert data format enum to string
    std::string FormatToString(DataFormat format) const;

    // member variables
    DataFormat m_format;            // data format
    uint32_t m_packetSize;          // size of each packet in bytes
    uint32_t m_bitRateBps;          // bit rate in bps
    uint32_t m_minPacketIntervalMs; // minimum interval between packets in ms
    uint32_t m_rntiRetryDelayMs;
    uint32_t m_dataOffset;
    uint32_t m_txRepeat;                 // number of times to repeat the transmission
    uint32_t m_repeatCount;              // current repeat iteration
    uint32_t m_txTotalBytes;             // total bytes transmitted
    std::vector<uint8_t> m_originalData; // preserve original data for replay
    bool m_connected;                    // connection status
    mutable uint16_t m_cachedRnti;       // cached RNTI value
    bool m_useUdp;
    LinkDirection m_linkDirection; // link direction (UPLINK or DOWNLINK)

    Ptr<Socket> m_socket;           // socket for data transmission
    Address m_peer;                 // destination address
    std::vector<uint8_t> m_data;    // data buffer to transmit
    std::string m_ueObjectId;       // UE content ID
    std::string m_inputImageFormat; // input image format based on selected dataset
    EventId m_sendEvent;            // event for scheduling next packet send
    Time m_firstPacketTime;         // time when first packet is sent
    std::function<uint16_t()> m_rntiGetter;
    std::map<std::pair<uint16_t, LinkDirection>, uint8_t>
        m_rntiCriticality; // RNTI to criticality map

    struct ByteRangeCriticality
    {
        uint32_t startByte;
        uint32_t endByte;
        uint8_t criticality;
    };

    std::vector<ByteRangeCriticality> m_criticalityMap; // criticality map for data chunks
};

// == RECEPTION APPLICATION =====================================================
class IbOranReceptionApplication : public Application
{
  public:
    static TypeId GetTypeId();

    IbOranReceptionApplication();
    virtual ~IbOranReceptionApplication();

    // set local address and port to listen on
    void SetLocalAddress(Address address);

    // set output file path for received data
    void SetOutputPath(std::string path, DataFormat format);

    void SetDataFormat(DataFormat format);

    void SetUseUdp(bool useUdp);

    uint32_t GetTotalBytesReceived() const;
    uint32_t GetReceivedDataSize() const;
    uint32_t GetPacketCount() const;

    Time GetTransmissionDuration() const;

  protected:
    void StartApplication() override;
    void StopApplication() override;

  private:
    bool ConnectionRequested(Ptr<Socket> socket, const Address& from);
    void NewConnectionCreated(Ptr<Socket> socket, const Address& from);
    void ReceivePacket(Ptr<Socket> socket);
    void SaveReceivedData();

    // member variables
    DataFormat m_format;
    uint32_t m_rxTotalBytes;
    uint32_t m_packetCount;
    bool m_useUdp;

    Ptr<Socket> m_socket;
    Address m_local;
    std::vector<uint8_t> m_receivedData;
    std::string m_outputPath;
    Time m_firstPacketTime; // time when first packet is received
    Time m_lastPacketTime;  // time when last packet is received
};

} // namespace ns3

#endif // IBORAN_APPLICATION_H
