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
#include "iboran-app-content.h"
#include "iboran-app-envelope.h"
#include "iboran-app-message.h"

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/traced-callback.h"

#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

// == TRANSMISSION APPLICATION
// ==================================================
class IbOranTransmissionApplication : public Application
{
  public:
    static TypeId GetTypeId();

    /** One message as sent, for the message log. */
    struct SentMessage
    {
        uint32_t messageId = 0;
        uint32_t taskId = 0;   /**< set by the caller (0 = none) */
        std::string contextId; /**< as written in the envelope (empty = none) */
        uint16_t round = 0;
        bool fromFile = false;
        IbOranContentType contentType = IbOranContentType::UNKNOWN;
        uint32_t payloadSize = 0;
        uint32_t wireSize = 0;
        uint32_t fragmentsSent = 0;
        uint64_t firstTxNs = 0; /**< first fragment handed to the socket */
        uint64_t lastTxNs = 0;  /**< latest fragment handed to the socket */
        bool sentAll = false;   /**< every fragment handed to the socket */
    };

    IbOranTransmissionApplication();
    ~IbOranTransmissionApplication() override;

    // -- data loading ---------------------------------------------------------
    void LoadData(std::string filePathStem, DataFormat format);
    bool LoadFile(const std::string& filePath, DataFormat formatHint);
    void LoadDataFromBuffer(const uint8_t* data, uint32_t size, DataFormat format);

    /**
     * Queue a message for transmission. Returns its message ID (assigned if
     * message.messageId is 0). Can be called before or after start.
     */
    uint32_t SendMessage(IbOranMessage message);

    // -- configuration ----------------------------------------------------------
    void SetPeer(Address address);
    void SetPacketSize(uint32_t packetSize);
    void SetBitRate(uint32_t bitRate);
    void SetMinPacketInterval(uint32_t intervalMs); // legacy, milliseconds
    void SetMinPacketIntervalTime(Time interval);
    void SetUseUdp(bool useUdp);
    void SetFraming(bool enable);
    void SetEnvelopeModel(IbOranEnvelopeModel model);
    void SetEmbeddingPrefixCriticality(bool enable);

    void SetRntiGetter(std::function<uint16_t()> getter);
    void SetLinkDirection(LinkDirection direction);
    void SetUeObjectId(std::string objectId);
    void SetInputImageFormat(std::string format);
    void SetInputFileExtension(std::string extension);

    // -- statistics -----------------------------------------------------------
    uint32_t GetTotalBytesTransmitted() const;        /**< bytes handed to the socket */
    uint32_t GetTotalPayloadBytesTransmitted() const; /**< message bytes, headers excluded */
    uint32_t GetTotalPacketsTransmitted() const;
    uint32_t GetTotalMessagesTransmitted() const;
    uint32_t GetLoadedDataSize() const;    /**< size of the loaded file */
    uint32_t GetRemainingDataSize() const; /**< bytes still to send */

    DataFormat GetDataFormat() const;
    std::string GetDataFormatName() const;
    IbOranContentType GetContentType() const;

    /** Messages whose transmission has started, in order. */
    const std::vector<SentMessage>& GetSentMessages() const;

    /**
     * Write the transmission statistics and one record per sent message as a
     * JSON object. The environment collects these into
     * notebook/logs/appMessages.json.
     */
    void WriteMessageLog(std::ostream& os, const std::string& indent) const;

    typedef void (*TxMessageTracedCallback)(uint32_t messageId, uint32_t wireSize);

  protected:
    void StartApplication() override;
    void StopApplication() override;

  private:
    struct OutgoingMessage
    {
        IbOranFragmentHeader base;        // fields shared by all fragments
        std::vector<uint8_t> wire;        // envelope prefix + encoded payload + suffix
        IbOranCriticalityMap criticality; // over wire offsets
        uint32_t payloadStart = 0;
        uint32_t payloadEnd = 0;
        uint32_t alignmentUnit = 1;
        uint32_t offset = 0;
        uint16_t nextFragment = 0;
        size_t record = 0; // index in m_sent
    };

    void ConnectionSucceeded(Ptr<Socket> socket);
    void ConnectionFailed(Ptr<Socket> socket);
    void HandleSend(Ptr<Socket> socket, uint32_t availableBytes);
    void SendNextPacket();
    void ScheduleSendIfIdle();

    bool PrepareNextMessage();
    uint32_t ReserveMessageId(uint32_t requested);
    void EnqueueFileMessage();
    void ResolveRuntimeConfig();
    bool HasPendingData() const;

    uint16_t GetRnti();
    void PublishRntiCriticality(uint8_t criticality);
    void PublishRntiObjectId() const;

    // configuration
    DataFormat m_format;
    IbOranContentType m_contentType;
    uint32_t m_packetSize;
    uint32_t m_bitRateBps;
    Time m_minPacketInterval;
    uint32_t m_rntiRetryDelayMs;
    bool m_useUdp;
    bool m_framing;
    std::string m_envelopeName;
    IbOranEnvelopeModel m_envelope;
    uint32_t m_maxDatagramSize;
    bool m_embeddingPrefix;
    std::string m_contextId;
    uint32_t m_grpcHeaderBytes;
    LinkDirection m_linkDirection;
    std::string m_ueObjectId;
    std::string m_inputImageFormat;
    std::string m_inputFileExtension;

    // loaded file
    std::vector<uint8_t> m_fileData;
    bool m_fileLoaded;
    std::string m_filePath;
    IbOranCriticalityMap m_fileCriticality;
    bool m_fileCriticalityReady;

    // runtime
    std::deque<IbOranMessage> m_queue;
    std::optional<OutgoingMessage> m_current;
    uint32_t m_nextMessageId;
    bool m_running;
    bool m_connected;
    Ptr<Socket> m_socket;
    Address m_peer;
    EventId m_sendEvent;
    std::function<uint16_t()> m_rntiGetter;
    uint16_t m_lastRnti;

    // statistics
    uint32_t m_txTotalBytes;
    uint32_t m_txPayloadBytes;
    uint32_t m_txPackets;
    uint32_t m_txMessages;
    std::vector<SentMessage> m_sent;

    TracedCallback<Ptr<const Packet>> m_txTrace;
    TracedCallback<uint32_t, uint32_t> m_txMessageTrace;
};

// == RECEPTION APPLICATION
// =====================================================
/**
 * Receives datagrams (UDP) or a byte stream (TCP), reassembles messages from
 * their fragment headers, reads the A2A envelope, keeps or assigns task and
 * context IDs, fills lost elements according to the fill policy, and saves the
 * payload. It sends no response. A message is delivered (RxMessage trace) when
 * it is complete, when its timeout expires, or when the application stops.
 */
class IbOranReceptionApplication : public Application
{
  public:
    static TypeId GetTypeId();

    IbOranReceptionApplication();
    ~IbOranReceptionApplication() override;

    /** A received message is identified by its sender and its message ID. */
    using MessageKey = std::pair<Address, uint32_t>;

    void SetLocalAddress(Address address);
    void SetOutputPath(std::string path, DataFormat format);
    /** Extension of IMAGE outputs, the dataset's input_image_format. */
    void SetOutputImageFormat(std::string format);
    void SetDataFormat(DataFormat format);
    void SetUseUdp(bool useUdp);
    void SetFraming(bool enable);
    void SetFillPolicy(IbOranFillPolicy policy);
    void SetSaveAllMessages(bool enable);
    void SetMessageTimeout(Time timeout);
    /**
     * Prefix of the task and context IDs this receiver assigns, for example
     * "ul-ue3" gives "ul-ue3-t-1" and "ul-ue3-ctx-1". Empty gives "t-1".
     */
    void SetIdPrefix(std::string prefix);

    uint32_t GetTotalBytesReceived() const;
    uint32_t GetReceivedDataSize() const; /**< payload bytes of the saved message */
    /** Socket reads: datagrams over UDP, stream reads over TCP. */
    uint32_t GetPacketCount() const;
    /** Fragments with a valid header (framing on); 0 with framing off. */
    uint32_t GetFragmentCount() const;
    uint32_t GetMessageCount() const;
    uint32_t GetCompleteMessageCount() const;
    uint32_t GetInvalidFragmentCount() const;
    const std::map<MessageKey, IbOranReceivedMessage>& GetMessages() const;

    /**
     * Write the reception statistics and one record per message as a JSON
     * object (fragment counts, sizes, completeness, latency). The environment
     * collects these into notebook/logs/appMessages.json.
     */
    void WriteMessageLog(std::ostream& os, const std::string& indent) const;

    Time GetTransmissionDuration() const;

    typedef void (*RxMessageTracedCallback)(const IbOranReceivedMessage& message,
                                            const Address& from);

  protected:
    void StartApplication() override;
    void StopApplication() override;

  private:
    bool ConnectionRequested(Ptr<Socket> socket, const Address& from);
    void NewConnectionCreated(Ptr<Socket> socket, const Address& from);
    void ReceivePacket(Ptr<Socket> socket);

    void HandleDatagram(const uint8_t* data, uint32_t length, const Address& from);
    void HandleStream(Ptr<Socket> socket,
                      const uint8_t* data,
                      uint32_t length,
                      const Address& from);
    void ProcessFragment(const IbOranFragmentHeader& header,
                         const uint8_t* data,
                         const Address& from);
    void FinalizeMessage(MessageKey key);
    void FinalizeAll();

    const IbOranReceivedMessage* GetPrimaryMessage() const;
    /** File name a message is saved under, empty if it is not saved. */
    std::string GetOutputFileName(const MessageKey& key,
                                  const IbOranReceivedMessage& message) const;
    void AssignA2aIds(const MessageKey& key, IbOranReceivedMessage& message);
    std::string MakeA2aId(const std::string& kind, uint32_t number) const;
    void SaveReceivedData();
    void UpdateOutputPath();
    uint32_t GetSenderIndex(const Address& from);
    std::string BuildMessagePath(const MessageKey& key) const;

    // configuration
    DataFormat m_format;
    bool m_useUdp;
    bool m_framing;
    std::string m_fillPolicyName;
    IbOranFillPolicy m_fillPolicy;
    Time m_messageTimeout;
    bool m_saveAllMessages;
    std::string m_idPrefix;

    Ptr<Socket> m_socket;
    Address m_local;
    std::string m_outputStem;
    std::string m_outputImageFormat;
    std::string m_outputPath;

    // statistics
    uint32_t m_rxTotalBytes;
    uint32_t m_packetCount;
    uint32_t m_fragmentCount;
    uint32_t m_invalidFragments;
    uint32_t m_lateFragments;
    Time m_firstPacketTime;
    Time m_lastPacketTime;

    // legacy (framing disabled)
    std::vector<uint8_t> m_receivedData;

    // reassembly
    std::map<Ptr<Socket>, std::vector<uint8_t>> m_streams;
    std::map<MessageKey, IbOranMessageAssembler> m_assemblers;
    std::map<MessageKey, EventId> m_timeouts;
    std::set<MessageKey> m_finalized;
    std::map<MessageKey, IbOranReceivedMessage> m_messages;
    std::map<Address, uint32_t> m_senderIndex;    // order in which senders appear
    std::map<MessageKey, uint32_t> m_taskNumbers; // reserved at first fragment
    uint32_t m_nextTaskNumber;

    TracedCallback<Ptr<const Packet>, const Address&> m_rxTrace;
    TracedCallback<const IbOranReceivedMessage&, const Address&> m_rxMessageTrace;
};

} // namespace ns3

#endif // IBORAN_APPLICATION_H
