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
    /**
     * @brief Constructor
     *
     * Creates the MAC Scheduler interface implementation
     */
    IbOranLteMacScheduler();

    /**
     * Destructor
     */
    ~IbOranLteMacScheduler() override;

    // inherited from Object
    void DoDispose() override;
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
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

    /**
     * @brief transmission mode configuration update
     *
     * @param rnti the RNTI
     * @param txMode the transmit mode
     */
    void TransmissionModeConfigurationUpdate(uint16_t rnti, uint8_t txMode);

  private:
    //
    // Implementation of the CSCHED API primitives
    // (See 4.1 for description of the primitives)
    //

    /**
     * @brief CSched cell config request
     *
     * @param params FfMacCschedSapProvider::CschedCellConfigReqParameters
     */
    void DoCschedCellConfigReq(const FfMacCschedSapProvider::CschedCellConfigReqParameters& params);

    /**
     * @brief CSched UE config request
     *
     * @param params FfMacCschedSapProvider::CschedUeConfigReqParameters
     */
    void DoCschedUeConfigReq(const FfMacCschedSapProvider::CschedUeConfigReqParameters& params);

    /**
     * @brief CSched LC config request
     *
     * @param params FfMacCschedSapProvider::CschedLcConfigReqParameters
     */
    void DoCschedLcConfigReq(const FfMacCschedSapProvider::CschedLcConfigReqParameters& params);

    /**
     * @brief CSched LC release request
     *
     * @param params FfMacCschedSapProvider::CschedLcReleaseReqParameters
     */
    void DoCschedLcReleaseReq(const FfMacCschedSapProvider::CschedLcReleaseReqParameters& params);

    /**
     * @brief CSched UE release request
     *
     * @param params FfMacCschedSapProvider::CschedLcReleaseReqParameters
     */
    void DoCschedUeReleaseReq(const FfMacCschedSapProvider::CschedUeReleaseReqParameters& params);

    //
    // Implementation of the SCHED API primitives
    // (See 4.2 for description of the primitives)
    //

    /**
     * @brief Sched DL RLC buffer request
     *
     * @param params FfMacSchedSapProvider::SchedDlRlcBufferReqParameters
     */
    void DoSchedDlRlcBufferReq(const FfMacSchedSapProvider::SchedDlRlcBufferReqParameters& params);

    /**
     * @brief Sched DL paging buffer request
     *
     * @param params FfMacSchedSapProvider::SchedDlPagingBufferReqParameters
     */
    void DoSchedDlPagingBufferReq(
        const FfMacSchedSapProvider::SchedDlPagingBufferReqParameters& params);

    /**
     * @brief Sched DL MAC buffer request
     *
     * @param params FfMacSchedSapProvider::SchedDlMacBufferReqParameters
     */
    void DoSchedDlMacBufferReq(const FfMacSchedSapProvider::SchedDlMacBufferReqParameters& params);

    /**
     * @brief Sched DL trigger request
     *
     * @param params FfMacSchedSapProvider::SchedDlTriggerReqParameters
     */
    void DoSchedDlTriggerReq(const FfMacSchedSapProvider::SchedDlTriggerReqParameters& params);

    /**
     * @brief Sched DL RACH info request
     *
     * @param params FfMacSchedSapProvider::SchedDlRachInfoReqParameters
     */
    void DoSchedDlRachInfoReq(const FfMacSchedSapProvider::SchedDlRachInfoReqParameters& params);

    /**
     * @brief Sched DL CQI info request
     *
     * @param params FfMacSchedSapProvider::SchedDlCqiInfoReqParameters
     */
    void DoSchedDlCqiInfoReq(const FfMacSchedSapProvider::SchedDlCqiInfoReqParameters& params);

    /**
     * @brief Sched UL trigger request
     *
     * @param params FfMacSchedSapProvider::SchedUlTriggerReqParameters
     */
    void DoSchedUlTriggerReq(const FfMacSchedSapProvider::SchedUlTriggerReqParameters& params);

    /**
     * @brief Sched UL noise interference request
     *
     * @param params FfMacSchedSapProvider::SchedUlNoiseInterferenceReqParameters
     */
    void DoSchedUlNoiseInterferenceReq(
        const FfMacSchedSapProvider::SchedUlNoiseInterferenceReqParameters& params);

    /**
     * @brief Sched UL SR info request
     *
     * @param params FfMacSchedSapProvider::SchedUlSrInfoReqParameters
     */
    void DoSchedUlSrInfoReq(const FfMacSchedSapProvider::SchedUlSrInfoReqParameters& params);

    /**
     * @brief Sched UL MAC control info request
     *
     * @param params FfMacSchedSapProvider::SchedUlMacCtrlInfoReqParameters
     */
    void DoSchedUlMacCtrlInfoReq(
        const FfMacSchedSapProvider::SchedUlMacCtrlInfoReqParameters& params);

    /**
     * @brief Sched UL CQI info request
     *
     * @param params FfMacSchedSapProvider::SchedUlCqiInfoReqParameters
     */
    void DoSchedUlCqiInfoReq(const FfMacSchedSapProvider::SchedUlCqiInfoReqParameters& params);

    /**
     * @brief Get RBG size
     *
     * @param dlbandwidth dDL bandwidth
     * @returns the RBG size
     */
    int GetRbgSize(int dlbandwidth);

    /**
     * @brief Calculate CQI from SINR
     *
     * @param sinr the SINR
     * @returns the CQI
     */
    uint8_t GetCqiFromSinr(double sinr);

    /**
     * @brief Calculate QCI for RNTI
     *
     * @param rnti the RNTI
     * @returns the QCI
     */
    uint8_t GetQciForRnti(uint16_t rnti);

    /**
     * @brief Store QCI information
     *
     * @param rnti the RNTI
     * @param lcid the LCID
     * @param qci the QCI
     */
    void StoreQciInfo(uint16_t rnti, uint8_t lcid, uint8_t qci);

    /**
     * @brief Sort RLC buffer reports by RNTI in ascending order
     *
     * @param i the first RLC buffer report to compare
     * @param j the second RLC buffer report to compare
     * @returns true if i.m_rnti < j.m_rnti, false otherwise
     */
    static bool SortRlcBufferReq(FfMacSchedSapProvider::SchedDlRlcBufferReqParameters i,
                                 FfMacSchedSapProvider::SchedDlRlcBufferReqParameters j);

    /**
     * @brief LC active per flow
     *
     * @param rnti the RNTI
     * @returns the LC active per flow
     */
    unsigned int LcActivePerFlow(uint16_t rnti);

    /**
     * @brief Estimate UL SINR
     *
     * @param rnti the RNTI
     * @param rb the RB
     * @returns the SINR
     */
    double EstimateUlSinr(uint16_t rnti, uint16_t rb);

    /// Refresh DL CQI maps
    void RefreshDlCqiMaps();
    /// Refresh UL CQI maps
    void RefreshUlCqiMaps();

    /**
     * @brief Update DL RCL buffer info
     *
     * @param rnti the RNTI
     * @param lcid the LCID
     * @param size the size
     */
    void UpdateDlRlcBufferInfo(uint16_t rnti, uint8_t lcid, uint16_t size);
    /**
     * @brief Update UL RCL buffer info
     *
     * @param rnti the RNTI
     * @param size the size
     */
    void UpdateUlRlcBufferInfo(uint16_t rnti, uint16_t size);

    /**
     * @brief Update and return a new process Id for the RNTI specified
     *
     * @param rnti the RNTI of the UE to be updated
     * @return the process id  value
     */
    uint8_t UpdateHarqProcessId(uint16_t rnti);

    /**
     * @brief Return the availability of free process for the RNTI specified
     *
     * @param rnti the RNTI of the UE to be updated
     * @return the availability
     */
    bool HarqProcessAvailability(uint16_t rnti);

    /**
     * @brief Refresh HARQ processes according to the timers
     *
     */
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
     * Map of UEs' timers on QCI
     */
    std::map<uint16_t, std::map<uint8_t, uint8_t>> m_ueQci;
    /**
     * Map of UEs' timers on highest priority QCI
     */
    std::map<uint16_t, uint8_t> m_ueHighestPriorityQci;

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

    // Per-direction message ID counters
    uint32_t m_dlE3IndicationId{0};
    uint32_t m_ulE3IndicationId{0};
    uint32_t m_dlE2KpmReportId{0};
    uint32_t m_ulE2KpmReportId{0};

    // Per-UE tracking
    std::map<uint16_t, uint32_t> m_ueDlTotalBytesServed;
    std::map<uint16_t, uint32_t> m_ueUlTotalBytesServed;
    std::map<uint16_t, uint8_t> m_ueDlAllocatedRb;
    std::map<uint16_t, uint8_t> m_ueUlAllocatedRb;
    std::map<uint16_t, uint16_t> m_ueDlMaxRbgAllocation;
    std::map<uint16_t, uint16_t> m_ueUlMaxRbgAllocation;
};

} // namespace ns3

#endif /* IBORAN_LTE_MAC_SCHEDULER_H */
