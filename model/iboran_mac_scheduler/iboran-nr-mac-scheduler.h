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

#pragma once

#include "../../../nr/model/nr-mac-scheduler-lcg.h"
#include "../../../nr/model/nr-mac-scheduler-ofdma.h"
#include "../../../nr/model/nr-mac-scheduler-ue-info.h"
#include "../iboran_interfaces/iboran-shm-structure.h"
#include "iboran-throughput-tracker.h"
#include "iboran-waiting-time-tracker.h"

#include "ns3/ns3-ai-msg-interface.h"

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ns3
{

class IbOranNrMacScheduler : public NrMacSchedulerOfdma
{
  public:
    static TypeId GetTypeId();
    IbOranNrMacScheduler();
    ~IbOranNrMacScheduler() override;

  protected:
    std::shared_ptr<NrMacSchedulerUeInfo> CreateUeRepresentation(
        const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const override;

    std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq&,
                       const NrMacSchedulerNs3::UePtrAndBufferReq&)>
    GetUeCompareDlFn() const override;

    std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq&,
                       const NrMacSchedulerNs3::UePtrAndBufferReq&)>
    GetUeCompareUlFn() const override;

    void AssignedDlResources(const UePtrAndBufferReq& ue,
                             const FTResources& assigned,
                             const FTResources& totalAssigned) const override;

    void AssignedUlResources(const UePtrAndBufferReq& ue,
                             const FTResources& assigned,
                             const FTResources& totalAssigned) const override;

    void NotAssignedDlResources(const UePtrAndBufferReq& ue,
                                const FTResources& notAssigned,
                                const FTResources& totalAssigned) const override;

    void NotAssignedUlResources(const UePtrAndBufferReq& ue,
                                const FTResources& notAssigned,
                                const FTResources& totalAssigned) const override;

    void BeforeDlSched(const UePtrAndBufferReq& ue,
                       const FTResources& assignableInIteration) const override;

    void BeforeUlSched(const UePtrAndBufferReq& ue,
                       const FTResources& assignableInIteration) const override;

    BeamSymbolMap AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const override;
    BeamSymbolMap AssignULRBG(uint32_t symAvail, const ActiveUeMap& activeUl) const override;

    std::shared_ptr<DciInfoElementTdma> CreateDlDci(
        PointInFTPlane* spoint,
        const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
        uint32_t maxSym) const override;

    std::shared_ptr<DciInfoElementTdma> CreateUlDci(
        PointInFTPlane* spoint,
        const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
        uint32_t maxSym) const override;

    void ChangeDlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const override;
    void ChangeUlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const override;

  private:
    // Shared memory interface
    Ns3AiMsgInterfaceImpl<Cpp2Py, Py2Cpp>* m_shmInterface{nullptr};

    // E2 and E3 session state, set by DoSetupExchange()
    mutable bool m_setupDone{false};
    mutable uint16_t m_ricRequestorId{0};
    mutable uint16_t m_ricInstanceId{0};
    mutable uint16_t m_dAppId{0};
    mutable uint32_t m_kpmReportPeriodFrames{0}; ///< 0: every slot
    mutable uint64_t m_lastKpmFrame[2]{0, 0};    ///< per LinkDirection
    mutable ControlOutcome m_outcome[2]{};       ///< per LinkDirection

    // mutable: AssignDLRBG/AssignULRBG are const per base-class contract
    mutable uint32_t m_dlE3IndicationSn{0};
    mutable uint32_t m_dlE2KpmIndicationSn{0};
    mutable uint32_t m_ulE3IndicationSn{0};
    mutable uint32_t m_ulE2KpmIndicationSn{0};

    mutable std::map<uint16_t, uint32_t> m_ueDlTotalBytesServed;
    mutable std::map<uint16_t, uint32_t> m_ueUlTotalBytesServed;
    mutable std::map<uint64_t, uint16_t> m_ueDlMaxRbgAllocation; ///< keyed by UE ID
    mutable std::map<uint64_t, uint16_t> m_ueUlMaxRbgAllocation; ///< keyed by UE ID
    mutable std::map<uint16_t, uint32_t> m_ueDlBytesServedAtLastKpm;
    mutable std::map<uint16_t, uint32_t> m_ueUlBytesServedAtLastKpm;

    mutable ThroughputTracker m_dlTput{100.0};
    mutable ThroughputTracker m_ulTput{100.0};

    mutable WaitingTimeTracker m_dlWaiting;
    mutable WaitingTimeTracker m_ulWaiting;

    /**
     * Per-UE allocation cached by AssignDLRBG / AssignULRBG and read
     * by CreateDlDci / CreateUlDci within the same scheduling slot.
     */
    struct PendingAlloc
    {
        uint16_t rbgStart{0};
        uint16_t rbgLen{0};
        uint8_t mcs{0};
        uint32_t symAvail{0}; ///< symbols at assignment time
        uint16_t bandwidthInRbg{0};
        uint32_t bufBytes{0};
    };

    mutable std::map<uint16_t, PendingAlloc> m_pendingDlAlloc; ///< keyed by RNTI
    mutable std::map<uint16_t, PendingAlloc> m_pendingUlAlloc; ///< keyed by RNTI

    // Linear SINR → wideband CQI [1,15] (3GPP TS 36.213 Table 7.2.3-1).
    uint8_t GetCqiFromSinrLinear(double sinrLinear) const;

    /**
     * 5QI with the best priority level (TS 23.501 Table 5.7.4-1) across
     * the DRBs of one UE direction. SRBs (LCID 0-2) are left out.
     * Returns 9 when the UE has no DRB.
     */
    uint8_t GetHighestPriorityFiveQi(const std::unordered_map<uint8_t, LCGPtr>& lcgMap) const;

    // E2 SETUP and E3 SETUP round-trip, run once before the first indication.
    void DoSetupExchange() const;

    // DRB.RlcSduTransmittedVolumeDL/UL of the reported UEs since the previous indication.
    void FillKpmVolume(E2KpmIndication* e2KpmIndication, LinkDirection direction) const;

    /**
     * Populate one UE entry in E3Indication + E2KpmIndication structs.
     *
     * Fills CQI, MCS, buffer size, throughput, packet-delay, bytes served,
     * content criticality, content ID, and QoS ID and UE ID from ueInfo and tracking maps.
     */
    void FillUeIndication(const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                          uint32_t totalBuffer,
                          uint32_t ueIdx,
                          LinkDirection direction,
                          bool isUl,
                          uint32_t symAvail,
                          E3Indication* e3Indication,
                          E2KpmIndication* e2KpmIndication) const;

    // Get current time slot information
    struct SlotInfo
    {
        uint32_t frameNo;
        uint8_t subframeNo;
        uint8_t slotNo;
    };

    SlotInfo GetCurrentSlotInfo() const;
};

} // namespace ns3
