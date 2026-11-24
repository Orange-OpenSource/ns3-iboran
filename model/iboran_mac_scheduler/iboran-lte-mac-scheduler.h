/*
 * Software Name: ib-ORAN
 * SPDX-FileCopyrightText: Copyright (c) Orange SA
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This software is derived from ns-3 and ns3-ai, which are distributed
 * under the GNU General Public License v2.0.
 * See https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * Authors: see CONTRIBUTORS.md
 */

#ifndef IBORAN_LTE_MAC_SCHEDULER_H
#define IBORAN_LTE_MAC_SCHEDULER_H

#include "../iboran_interfaces/iboran-shm-structure.h"
#include "iboran-throughput-tracker.h"
#include "iboran-waiting-time-tracker.h"

#include "ns3/ff-mac-csched-sap.h"
#include "ns3/ff-mac-sched-sap.h"
#include "ns3/ff-mac-scheduler.h"
#include "ns3/lte-amc.h"
#include "ns3/lte-common.h"
#include "ns3/lte-ffr-sap.h"
#include "ns3/ns3-ai-msg-interface.h"
#include "ns3/nstime.h"

#include <map>
#include <vector>

namespace ns3
{

static const uint16_t MINIMUM_PDU_SIZE = 20; // bytes

class IbOranLteMacScheduler : public FfMacScheduler
{
  public:
    IbOranLteMacScheduler();
    ~IbOranLteMacScheduler() override;

    // inherited from Object
    void DoDispose() override;

    // Get the type ID.
    static TypeId GetTypeId();

    // inherited from FfMacScheduler
    void SetFfMacCschedSapUser(FfMacCschedSapUser* s) override;
    void SetFfMacSchedSapUser(FfMacSchedSapUser* s) override;
    FfMacCschedSapProvider* GetFfMacCschedSapProvider() override;
    FfMacSchedSapProvider* GetFfMacSchedSapProvider() override;

    // FFR SAPs
    void SetLteFfrSapProvider(LteFfrSapProvider* s) override;
    LteFfrSapUser* GetLteFfrSapUser() override;

    /// allow MemberCschedSapProvider<IbOranLteMacScheduler> class friend access
    friend class MemberCschedSapProvider<IbOranLteMacScheduler>;
    /// allow MemberSchedSapProvider<IbOranLteMacScheduler> class friend access
    friend class MemberSchedSapProvider<IbOranLteMacScheduler>;

    // transmission mode configuration update
    void TransmissionModeConfigurationUpdate(uint16_t rnti, uint8_t txMode);

  private:
    // Implementation of the CSCHED API primitives
    // (See 4.1 for description of the primitives)
    void DoCschedCellConfigReq(const FfMacCschedSapProvider::CschedCellConfigReqParameters& params);

    void DoCschedUeConfigReq(const FfMacCschedSapProvider::CschedUeConfigReqParameters& params);

    void DoCschedLcConfigReq(const FfMacCschedSapProvider::CschedLcConfigReqParameters& params);

    void DoCschedLcReleaseReq(const FfMacCschedSapProvider::CschedLcReleaseReqParameters& params);

    void DoCschedUeReleaseReq(const FfMacCschedSapProvider::CschedUeReleaseReqParameters& params);

    // Implementation of the SCHED API primitives
    // (See 4.2 for description of the primitives)
    void DoSchedDlRlcBufferReq(const FfMacSchedSapProvider::SchedDlRlcBufferReqParameters& params);

    void DoSchedDlPagingBufferReq(
        const FfMacSchedSapProvider::SchedDlPagingBufferReqParameters& params);

    void DoSchedDlMacBufferReq(const FfMacSchedSapProvider::SchedDlMacBufferReqParameters& params);

    void DoSchedDlTriggerReq(const FfMacSchedSapProvider::SchedDlTriggerReqParameters& params);

    void DoSchedDlRachInfoReq(const FfMacSchedSapProvider::SchedDlRachInfoReqParameters& params);

    void DoSchedDlCqiInfoReq(const FfMacSchedSapProvider::SchedDlCqiInfoReqParameters& params);

    void DoSchedUlTriggerReq(const FfMacSchedSapProvider::SchedUlTriggerReqParameters& params);

    void DoSchedUlNoiseInterferenceReq(
        const FfMacSchedSapProvider::SchedUlNoiseInterferenceReqParameters& params);

    void DoSchedUlSrInfoReq(const FfMacSchedSapProvider::SchedUlSrInfoReqParameters& params);

    void DoSchedUlMacCtrlInfoReq(
        const FfMacSchedSapProvider::SchedUlMacCtrlInfoReqParameters& params);

    void DoSchedUlCqiInfoReq(const FfMacSchedSapProvider::SchedUlCqiInfoReqParameters& params);

    int GetRbgSize(int dlbandwidth);

    uint8_t GetCqiFromSinr(double sinr);

    uint8_t GetQciForRnti(uint16_t rnti);

    void StoreQciInfo(uint16_t rnti, uint8_t lcid, uint8_t qci);

    // E2 SETUP and E3 SETUP round-trip, run once before the first indication.
    void DoSetupExchange();

    // DRB.RlcSduTransmittedVolumeDL/UL of the reported UEs since the previous indication.
    void FillKpmVolume(E2KpmIndication* e2KpmIndication, LinkDirection direction);

    // Sort RLC buffer reports by RNTI in ascending order
    static bool SortRlcBufferReq(FfMacSchedSapProvider::SchedDlRlcBufferReqParameters i,
                                 FfMacSchedSapProvider::SchedDlRlcBufferReqParameters j);

    unsigned int LcActivePerFlow(uint16_t rnti);

    double EstimateUlSinr(uint16_t rnti, uint16_t rb);

    void RefreshDlCqiMaps();
    void RefreshUlCqiMaps();

    void UpdateDlRlcBufferInfo(uint16_t rnti, uint8_t lcid, uint16_t size);
    void UpdateUlRlcBufferInfo(uint16_t rnti, uint16_t size);

    // Update and return a new process Id for the RNTI specified
    uint8_t UpdateHarqProcessId(uint16_t rnti);

    // Return the availability of free process for the RNTI specified
    bool HarqProcessAvailability(uint16_t rnti);

    // Refresh HARQ processes according to the timers
    void RefreshHarqProcesses();

    Ptr<LteAmc> m_amc; ///< AMC

    /**
     * Vectors of UE's LC info
     */
    std::map<LteFlowId_t, FfMacSchedSapProvider::SchedDlRlcBufferReqParameters> m_rlcBufferReq;

    /**
     * Map of UE's DL CQI P01 received
     */
    std::map<uint16_t, uint8_t> m_p10CqiRxed;
    /**
     * Map of UE's timers on DL CQI P01 received
     */
    std::map<uint16_t, uint32_t> m_p10CqiTimers;

    /**
     * Map of UE's DL CQI A30 received
     */
    std::map<uint16_t, SbMeasResult_s> m_a30CqiRxed;
    /**
     * Map of UE's timers on DL CQI A30 received
     */
    std::map<uint16_t, uint32_t> m_a30CqiTimers;

    /**
     * Map of previous allocated UE per RBG
     * (used to retrieve info from UL-CQI)
     */
    std::map<uint16_t, std::vector<uint16_t>> m_allocationMaps;

    /**
     * Map of UEs' UL-CQI per RBG
     */
    std::map<uint16_t, std::vector<double>> m_ueCqi;
    /**
     * Map of UEs' timers on UL-CQI per RBG
     */
    std::map<uint16_t, uint32_t> m_ueCqiTimers;

    /**
     * Map of UEs' QCI per LCID
     */
    std::map<uint16_t, std::map<uint8_t, uint8_t>> m_ueQci;

    /**
     * Map of UE's buffer status reports received
     */
    std::map<uint16_t, uint32_t> m_ceBsrRxed;

    // MAC SAPs
    FfMacCschedSapUser* m_cschedSapUser;         ///< CSched SAP user
    FfMacSchedSapUser* m_schedSapUser;           ///< Sched SAP user
    FfMacCschedSapProvider* m_cschedSapProvider; ///< CSched SAP provider
    FfMacSchedSapProvider* m_schedSapProvider;   ///< Sched SAP provider

    // FFR SAPs
    LteFfrSapUser* m_ffrSapUser;         ///< FFR SAP user
    LteFfrSapProvider* m_ffrSapProvider; ///< FFR SAP provider

    // Internal parameters
    FfMacCschedSapProvider::CschedCellConfigReqParameters
        m_cschedCellConfig; ///< CSched cell config

    uint16_t m_nextRntiUl; ///< RNTI of the next user to be served next scheduling in UL

    uint32_t m_cqiTimersThreshold; ///< # of TTIs for which a CQI can be considered valid

    std::map<uint16_t, uint8_t> m_uesTxMode; ///< txMode of the UEs

    // HARQ attributes
    /**
     * m_harqOn when false inhibit the HARQ mechanisms (by default active)
     */
    bool m_harqOn;
    std::map<uint16_t, uint8_t> m_dlHarqCurrentProcessId; ///< DL HARQ current process ID
    // HARQ status
    //  0: process Id available
    //  x>0: process Id equal to `x` transmission count
    std::map<uint16_t, DlHarqProcessesStatus_t> m_dlHarqProcessesStatus; ///< DL HARQ process status
    std::map<uint16_t, DlHarqProcessesTimer_t> m_dlHarqProcessesTimer;   ///< DL HARQ process timer
    std::map<uint16_t, DlHarqProcessesDciBuffer_t>
        m_dlHarqProcessesDciBuffer; ///< DL HARQ process DCI buffer
    std::map<uint16_t, DlHarqRlcPduListBuffer_t>
        m_dlHarqProcessesRlcPduListBuffer;                 ///< DL HARQ process RLC PDU list buffer
    std::vector<DlInfoListElement_s> m_dlInfoListBuffered; ///< HARQ retx buffered

    std::map<uint16_t, uint8_t> m_ulHarqCurrentProcessId; ///< UL HARQ current process ID
    // HARQ status
    //  0: process Id available
    //  x>0: process Id equal to `x` transmission count
    std::map<uint16_t, UlHarqProcessesStatus_t> m_ulHarqProcessesStatus; ///< UL HARQ process status
    std::map<uint16_t, UlHarqProcessesDciBuffer_t>
        m_ulHarqProcessesDciBuffer; ///< UL HARQ process DCI buffer

    // RACH attributes
    std::vector<RachListElement_s> m_rachList; ///< RACH list
    std::vector<uint16_t> m_rachAllocationMap; ///< RACH allocation map
    uint8_t m_ulGrantMcs;                      ///< MCS for UL grant (default 0)

    // Shared memory interface
    Ns3AiMsgInterfaceImpl<Cpp2Py, Py2Cpp>* m_shmInterface;

    // E2 and E3 session state, set by DoSetupExchange()
    bool m_setupDone{false};
    uint16_t m_cellId{0}; ///< set through the CellId attribute
    uint16_t m_ricRequestorId{0};
    uint16_t m_ricInstanceId{0};
    uint16_t m_dAppId{0};
    uint32_t m_kpmReportPeriodFrames{0}; ///< 0: every TTI
    uint64_t m_lastKpmFrame[2]{0, 0};    ///< per LinkDirection
    ControlOutcome m_outcome[2]{};       ///< per LinkDirection

    // Per-direction message sequence numbers
    uint32_t m_dlE3IndicationSn{0};
    uint32_t m_ulE3IndicationSn{0};
    uint32_t m_dlE2KpmIndicationSn{0};
    uint32_t m_ulE2KpmIndicationSn{0};

    // Per-UE tracking
    std::map<uint16_t, uint32_t> m_ueDlTotalBytesServed;
    std::map<uint16_t, uint32_t> m_ueUlTotalBytesServed;
    std::map<uint64_t, uint16_t> m_ueDlMaxRbgAllocation; ///< keyed by UE ID
    std::map<uint64_t, uint16_t> m_ueUlMaxRbgAllocation; ///< keyed by UE ID
    std::map<uint16_t, uint32_t> m_ueDlBytesServedAtLastKpm;
    std::map<uint16_t, uint32_t> m_ueUlBytesServedAtLastKpm;

    // Throughput tracking
    ThroughputTracker m_dlTput{100.0};
    ThroughputTracker m_ulTput{100.0};

    // Waiting time tracking
    WaitingTimeTracker m_dlWaiting;
    WaitingTimeTracker m_ulWaiting;
};

} // namespace ns3

#endif /* IBORAN_LTE_MAC_SCHEDULER_H */
