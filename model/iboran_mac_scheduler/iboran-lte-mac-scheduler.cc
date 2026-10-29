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

#include "iboran-lte-mac-scheduler.h"

#include "../iboran_application/iboran-side-info-tracker.h"

#include "ns3/ff-mac-scheduler.h"
#include <ns3/boolean.h>
#include <ns3/log.h>
#include <ns3/lte-amc.h>
#include <ns3/lte-common.h>
#include <ns3/lte-vendor-specific-parameters.h>
#include <ns3/math.h>
#include <ns3/pointer.h>
#include <ns3/simulator.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <climits>
#include <iterator>
#include <set>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IbOranLteMacScheduler");

/// Type 0 allocation RBG (see table 7.1.6.1-1 of 36.213)
static const int Type0AllocationRbg[4] = {
    10,  // RBG size 1
    26,  // RBG size 2
    63,  // RBG size 3
    110, // RBG size 4
};

/**
 * Type 0 allocation RBG table as defined in 3GPP TS 36.213, Section 7.1.6.1-1.
 * Each entry corresponds to the total number of Resource Block (RBs)
 * for a given downlink system bandwidth configuration (i.e., RBG size index).
 * LTE uses different RBG sizes depending on the total number of RBs
 * in the system bandwidth.
 *
 * Index → RBG size configuration:
 *  0 → 10  RBs  (6–10 RB bandwidth)   → RBG size = 1
 *  1 → 26  RBs  (11–26 RB bandwidth)  → RBG size = 2
 *  2 → 63  RBs  (27–63 RB bandwidth)  → RBG size = 3
 *  3 → 110 RBs  (64–110 RB bandwidth) → RBG size = 4
 */

NS_OBJECT_ENSURE_REGISTERED(IbOranLteMacScheduler);

IbOranLteMacScheduler::IbOranLteMacScheduler()
    : m_cschedSapUser(nullptr),
      m_schedSapUser(nullptr),
      m_nextRntiUl(0)
{
    // shared memory interface setup
    auto shmInterface = Ns3AiMsgInterface::Get();
    shmInterface->SetIsMemoryCreator(false);
    shmInterface->SetUseVector(false);
    shmInterface->SetHandleFinish(true);
    shmInterface->SetNames("iboranSegment", // boost::interprocess segment name
                           "iboranCpp2Py",  // C++ → Python message name
                           "iboranPy2Cpp",  // Python → C++ message name
                           "iboran"         // mutex/condition variable name
    );
    m_shmInterface = shmInterface->GetInterface<Cpp2Py, Py2Cpp>();

    m_amc = CreateObject<LteAmc>();
    m_cschedSapProvider = new MemberCschedSapProvider<IbOranLteMacScheduler>(this);
    m_schedSapProvider = new MemberSchedSapProvider<IbOranLteMacScheduler>(this);
    m_ffrSapProvider = nullptr;
    m_ffrSapUser = new MemberLteFfrSapUser<IbOranLteMacScheduler>(this);
}

IbOranLteMacScheduler::~IbOranLteMacScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
IbOranLteMacScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_shmInterface = nullptr;
    m_dlHarqProcessesDciBuffer.clear();
    m_dlHarqProcessesTimer.clear();
    m_dlHarqProcessesRlcPduListBuffer.clear();
    m_dlInfoListBuffered.clear();
    m_ulHarqCurrentProcessId.clear();
    m_ulHarqProcessesStatus.clear();
    m_ulHarqProcessesDciBuffer.clear();
    m_ueDlTotalBytesServed.clear();
    m_ueUlTotalBytesServed.clear();
    m_ueDlMaxRbgAllocation.clear();
    m_ueUlMaxRbgAllocation.clear();
    delete m_cschedSapProvider;
    delete m_schedSapProvider;
    delete m_ffrSapUser;
}

TypeId
IbOranLteMacScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::IbOranLteMacScheduler")
            .SetParent<FfMacScheduler>()
            .SetGroupName("Lte")
            .AddConstructor<IbOranLteMacScheduler>()
            .AddAttribute("CqiTimerThreshold",
                          "The number of TTIs a CQI is valid (default 1000 - 1 sec.)",
                          UintegerValue(1000),
                          MakeUintegerAccessor(&IbOranLteMacScheduler::m_cqiTimersThreshold),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("HarqEnabled",
                          "Activate/Deactivate the HARQ [by default is active].",
                          BooleanValue(true),
                          MakeBooleanAccessor(&IbOranLteMacScheduler::m_harqOn),
                          MakeBooleanChecker())
            .AddAttribute("UlGrantMcs",
                          "The MCS of the UL grant, must be [0..15] (default 0)",
                          UintegerValue(0),
                          MakeUintegerAccessor(&IbOranLteMacScheduler::m_ulGrantMcs),
                          MakeUintegerChecker<uint8_t>());
    return tid;
}

void
IbOranLteMacScheduler::SetFfMacCschedSapUser(FfMacCschedSapUser* s)
{
    m_cschedSapUser = s;
}

void
IbOranLteMacScheduler::SetFfMacSchedSapUser(FfMacSchedSapUser* s)
{
    m_schedSapUser = s;
}

FfMacCschedSapProvider*
IbOranLteMacScheduler::GetFfMacCschedSapProvider()
{
    return m_cschedSapProvider;
}

FfMacSchedSapProvider*
IbOranLteMacScheduler::GetFfMacSchedSapProvider()
{
    return m_schedSapProvider;
}

void
IbOranLteMacScheduler::SetLteFfrSapProvider(LteFfrSapProvider* s)
{
    m_ffrSapProvider = s;
}

LteFfrSapUser*
IbOranLteMacScheduler::GetLteFfrSapUser()
{
    return m_ffrSapUser;
}

void
IbOranLteMacScheduler::DoCschedCellConfigReq(
    const FfMacCschedSapProvider::CschedCellConfigReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    // Read the subset of parameters used
    m_cschedCellConfig = params;
    m_rachAllocationMap.resize(m_cschedCellConfig.m_ulBandwidth, 0);
    FfMacCschedSapUser::CschedUeConfigCnfParameters cnf;
    cnf.m_result = SUCCESS;
    m_cschedSapUser->CschedUeConfigCnf(cnf);
}

void
IbOranLteMacScheduler::DoCschedUeConfigReq(
    const FfMacCschedSapProvider::CschedUeConfigReqParameters& params)
{
    NS_LOG_FUNCTION(this << " RNTI " << params.m_rnti << " txMode "
                         << (uint16_t)params.m_transmissionMode);
    auto it = m_uesTxMode.find(params.m_rnti);
    if (it == m_uesTxMode.end())
    {
        m_uesTxMode[params.m_rnti] = params.m_transmissionMode;
        // generate HARQ buffers
        m_dlHarqCurrentProcessId[params.m_rnti] = 0;
        DlHarqProcessesStatus_t dlHarqPrcStatus;
        dlHarqPrcStatus.resize(8, 0);
        m_dlHarqProcessesStatus[params.m_rnti] = dlHarqPrcStatus;
        DlHarqProcessesTimer_t dlHarqProcessesTimer;
        dlHarqProcessesTimer.resize(8, 0);
        m_dlHarqProcessesTimer[params.m_rnti] = dlHarqProcessesTimer;
        DlHarqProcessesDciBuffer_t dlHarqdci;
        dlHarqdci.resize(8);
        m_dlHarqProcessesDciBuffer[params.m_rnti] = dlHarqdci;
        DlHarqRlcPduListBuffer_t dlHarqRlcPdu;
        dlHarqRlcPdu.resize(2);
        dlHarqRlcPdu.at(0).resize(8);
        dlHarqRlcPdu.at(1).resize(8);
        m_dlHarqProcessesRlcPduListBuffer[params.m_rnti] = dlHarqRlcPdu;
        m_ulHarqCurrentProcessId[params.m_rnti] = 0;
        UlHarqProcessesStatus_t ulHarqPrcStatus;
        ulHarqPrcStatus.resize(8, 0);
        m_ulHarqProcessesStatus[params.m_rnti] = ulHarqPrcStatus;
        UlHarqProcessesDciBuffer_t ulHarqdci;
        ulHarqdci.resize(8);
        m_ulHarqProcessesDciBuffer[params.m_rnti] = ulHarqdci;
    }
    else
    {
        (*it).second = params.m_transmissionMode;
    }
}

void
IbOranLteMacScheduler::DoCschedLcConfigReq(
    const FfMacCschedSapProvider::CschedLcConfigReqParameters& params)
{
    NS_LOG_FUNCTION(this << " New LC, rnti: " << params.m_rnti);

    for (uint16_t i = 0; i < params.m_logicalChannelConfigList.size(); i++)
    {
        uint16_t rnti = params.m_rnti;
        uint8_t lcid = params.m_logicalChannelConfigList[i].m_logicalChannelIdentity;
        uint8_t qci = params.m_logicalChannelConfigList[i].m_qci;
        StoreQciInfo(rnti, lcid, qci);
    }
}

void
IbOranLteMacScheduler::DoCschedLcReleaseReq(
    const FfMacCschedSapProvider::CschedLcReleaseReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    for (std::size_t i = 0; i < params.m_logicalChannelIdentity.size(); i++)
    {
        auto it = m_rlcBufferReq.begin();
        while (it != m_rlcBufferReq.end())
        {
            if (((*it).first.m_rnti == params.m_rnti) &&
                ((*it).first.m_lcId == params.m_logicalChannelIdentity.at(i)))
            {
                auto temp = it;
                it++;
                m_rlcBufferReq.erase(temp);
            }
            else
            {
                it++;
            }
        }
    }
}

void
IbOranLteMacScheduler::DoCschedUeReleaseReq(
    const FfMacCschedSapProvider::CschedUeReleaseReqParameters& params)
{
    NS_LOG_FUNCTION(this);

    m_uesTxMode.erase(params.m_rnti);
    m_dlHarqCurrentProcessId.erase(params.m_rnti);
    m_dlHarqProcessesStatus.erase(params.m_rnti);
    m_dlHarqProcessesTimer.erase(params.m_rnti);
    m_dlHarqProcessesDciBuffer.erase(params.m_rnti);
    m_dlHarqProcessesRlcPduListBuffer.erase(params.m_rnti);
    m_dlTput.Erase(params.m_rnti);
    m_ulHarqCurrentProcessId.erase(params.m_rnti);
    m_ulHarqProcessesStatus.erase(params.m_rnti);
    m_ulHarqProcessesDciBuffer.erase(params.m_rnti);
    m_ceBsrRxed.erase(params.m_rnti);
    m_ulTput.Erase(params.m_rnti);
    auto it = m_rlcBufferReq.begin();
    while (it != m_rlcBufferReq.end())
    {
        if ((*it).first.m_rnti == params.m_rnti)
        {
            auto temp = it;
            it++;
            m_rlcBufferReq.erase(temp);
        }
        else
        {
            it++;
        }
    }
    if (m_nextRntiUl == params.m_rnti)
    {
        m_nextRntiUl = 0;
    }
}

void
IbOranLteMacScheduler::DoSchedDlRlcBufferReq(
    const FfMacSchedSapProvider::SchedDlRlcBufferReqParameters& params)
{
    NS_LOG_FUNCTION(this << params.m_rnti << (uint32_t)params.m_logicalChannelIdentity);
    // API generated by RLC for updating RLC parameters on a LC (tx and retx queues)

    LteFlowId_t flow(params.m_rnti, params.m_logicalChannelIdentity);

    auto it = m_rlcBufferReq.find(flow);

    if (it == m_rlcBufferReq.end())
    {
        m_rlcBufferReq[flow] = params;
    }
    else
    {
        (*it).second = params;
    }
}

void
IbOranLteMacScheduler::DoSchedDlPagingBufferReq(
    const FfMacSchedSapProvider::SchedDlPagingBufferReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    NS_FATAL_ERROR("method not implemented");
}

void
IbOranLteMacScheduler::DoSchedDlMacBufferReq(
    const FfMacSchedSapProvider::SchedDlMacBufferReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    NS_FATAL_ERROR("method not implemented");
}

int
IbOranLteMacScheduler::GetRbgSize(int dlbandwidth)
{
    for (int i = 0; i < 4; i++)
    {
        if (dlbandwidth < Type0AllocationRbg[i])
        {
            return i + 1;
        }
    }

    return -1;
}

uint8_t
IbOranLteMacScheduler::GetCqiFromSinr(double sinr)
{
    static const double sinrTable[16] = {
        -INFINITY, // CQI 0: out of range
        -6.7,      // CQI 1: QPSK, code rate 0.076
        -4.7,      // CQI 2: QPSK, code rate 0.12
        -2.3,      // CQI 3: QPSK, code rate 0.19
        0.2,       // CQI 4: QPSK, code rate 0.30
        2.4,       // CQI 5: QPSK, code rate 0.44
        4.3,       // CQI 6: QPSK, code rate 0.59
        5.9,       // CQI 7: 16QAM, code rate 0.37
        8.1,       // CQI 8: 16QAM, code rate 0.48
        10.3,      // CQI 9: 16QAM, code rate 0.60
        11.7,      // CQI 10: 64QAM, code rate 0.45
        14.1,      // CQI 11: 64QAM, code rate 0.55
        16.3,      // CQI 12: 64QAM, code rate 0.65
        18.7,      // CQI 13: 64QAM, code rate 0.75
        21.0,      // CQI 14: 64QAM, code rate 0.85
        22.7       // CQI 15: 64QAM, code rate 0.93
    };

    uint8_t cqi = 0;
    for (uint8_t i = 1; i <= 15; i++)
    {
        if (sinr >= sinrTable[i])
        {
            cqi = i;
        }
        else
        {
            break;
        }
    }
    return std::min((uint8_t)15, std::max((uint8_t)1, (uint8_t)(cqi)));
}

void
IbOranLteMacScheduler::StoreQciInfo(uint16_t rnti, uint8_t lcid, uint8_t qci)
{
    auto itRnti = m_ueQci.find(rnti);
    if (itRnti == m_ueQci.end())
    {
        std::map<uint8_t, uint8_t> lcidQciMap;
        lcidQciMap[lcid] = qci;
        m_ueQci[rnti] = lcidQciMap;
    }
    else
    {
        itRnti->second[lcid] = qci;
    }

    auto itHighest = m_ueHighestPriorityQci.find(rnti);
    if (itHighest == m_ueHighestPriorityQci.end())
    {
        m_ueHighestPriorityQci[rnti] = qci;
    }
    else
    {
        if (qci <= 4 && qci < itHighest->second)
        {
            itHighest->second = qci;
        }
        else if (qci > 4 && itHighest->second > 4 && qci < itHighest->second)
        {
            itHighest->second = qci;
        }
    }
}

uint8_t
IbOranLteMacScheduler::GetQciForRnti(uint16_t rnti)
{
    auto it = m_ueHighestPriorityQci.find(rnti);
    if (it != m_ueHighestPriorityQci.end())
    {
        return it->second;
    }

    auto itQci = m_ueQci.find(rnti);
    if (itQci != m_ueQci.end() && !itQci->second.empty())
    {
        uint8_t highestPriorityQci = 9;
        for (auto& lcPair : itQci->second)
        {
            if (lcPair.second < highestPriorityQci)
            {
                highestPriorityQci = lcPair.second;
            }
        }
        return highestPriorityQci;
    }

    return 9;
}

bool
IbOranLteMacScheduler::SortRlcBufferReq(FfMacSchedSapProvider::SchedDlRlcBufferReqParameters i,
                                        FfMacSchedSapProvider::SchedDlRlcBufferReqParameters j)
{
    return (i.m_rnti < j.m_rnti);
}

unsigned int
IbOranLteMacScheduler::LcActivePerFlow(uint16_t rnti)
{
    unsigned int lcActive = 0;
    for (auto it = m_rlcBufferReq.begin(); it != m_rlcBufferReq.end(); it++)
    {
        if (((*it).first.m_rnti == rnti) && (((*it).second.m_rlcTransmissionQueueSize > 0) ||
                                             ((*it).second.m_rlcRetransmissionQueueSize > 0) ||
                                             ((*it).second.m_rlcStatusPduSize > 0)))
        {
            lcActive++;
        }
        if ((*it).first.m_rnti > rnti)
        {
            break;
        }
    }
    return lcActive;
}

bool
IbOranLteMacScheduler::HarqProcessAvailability(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);

    auto it = m_dlHarqCurrentProcessId.find(rnti);
    if (it == m_dlHarqCurrentProcessId.end())
    {
        NS_FATAL_ERROR("No Process Id found for this RNTI " << rnti);
    }
    auto itStat = m_dlHarqProcessesStatus.find(rnti);
    if (itStat == m_dlHarqProcessesStatus.end())
    {
        NS_FATAL_ERROR("No Process Id Statusfound for this RNTI " << rnti);
    }
    uint8_t i = (*it).second;
    do
    {
        i = (i + 1) % HARQ_PROC_NUM;
    } while (((*itStat).second.at(i) != 0) && (i != (*it).second));

    return (*itStat).second.at(i) == 0;
}

uint8_t
IbOranLteMacScheduler::UpdateHarqProcessId(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);

    if (!m_harqOn)
    {
        return 0;
    }

    auto it = m_dlHarqCurrentProcessId.find(rnti);
    if (it == m_dlHarqCurrentProcessId.end())
    {
        NS_FATAL_ERROR("No Process Id found for this RNTI " << rnti);
    }
    auto itStat = m_dlHarqProcessesStatus.find(rnti);
    if (itStat == m_dlHarqProcessesStatus.end())
    {
        NS_FATAL_ERROR("No Process Id Statusfound for this RNTI " << rnti);
    }
    uint8_t i = (*it).second;
    do
    {
        i = (i + 1) % HARQ_PROC_NUM;
    } while (((*itStat).second.at(i) != 0) && (i != (*it).second));
    if ((*itStat).second.at(i) == 0)
    {
        (*it).second = i;
        (*itStat).second.at(i) = 1;
    }
    else
    {
        NS_FATAL_ERROR("No HARQ process available for RNTI "
                       << rnti << " check before update with HarqProcessAvailability");
    }

    return (*it).second;
}

void
IbOranLteMacScheduler::RefreshHarqProcesses()
{
    NS_LOG_FUNCTION(this);

    for (auto itTimers = m_dlHarqProcessesTimer.begin(); itTimers != m_dlHarqProcessesTimer.end();
         itTimers++)
    {
        for (uint16_t i = 0; i < HARQ_PROC_NUM; i++)
        {
            if ((*itTimers).second.at(i) == HARQ_DL_TIMEOUT)
            {
                NS_LOG_DEBUG(this << " Reset HARQ proc " << i << " for RNTI " << (*itTimers).first);
                auto itStat = m_dlHarqProcessesStatus.find((*itTimers).first);
                if (itStat == m_dlHarqProcessesStatus.end())
                {
                    NS_FATAL_ERROR("No Process Id Status found for this RNTI "
                                   << (*itTimers).first);
                }
                (*itStat).second.at(i) = 0;
                (*itTimers).second.at(i) = 0;
            }
            else
            {
                (*itTimers).second.at(i)++;
            }
        }
    }
}

void
IbOranLteMacScheduler::DoSchedDlTriggerReq(
    const FfMacSchedSapProvider::SchedDlTriggerReqParameters& params)
{
    NS_LOG_FUNCTION(this << " Frame no. " << (params.m_sfnSf >> 4) << " subframe no. "
                         << (0xF & params.m_sfnSf));

    RefreshDlCqiMaps();

    const uint64_t nowNs = Simulator::Now().GetNanoSeconds();
    {
        std::set<uint16_t> dlBacklogged;
        for (const auto& [flow, req] : m_rlcBufferReq)
        {
            if (req.m_rlcTransmissionQueueSize > 0 || req.m_rlcRetransmissionQueueSize > 0 ||
                req.m_rlcStatusPduSize > 0)
            {
                dlBacklogged.insert(flow.m_rnti);
            }
        }
        m_dlWaiting.Retain(dlBacklogged);
    }

    int rbgSize = GetRbgSize(m_cschedCellConfig.m_dlBandwidth);
    int rbgNum = m_cschedCellConfig.m_dlBandwidth / rbgSize;
    std::map<uint16_t, std::vector<uint16_t>> allocationMap; // RBs map per RNTI
    std::vector<bool> rbgMap;                                // global RBGs map
    uint16_t rbgAllocatedNum = 0;
    std::set<uint16_t> rntiAllocated;
    rbgMap.resize(m_cschedCellConfig.m_dlBandwidth / rbgSize, false);

    rbgMap = m_ffrSapProvider->GetAvailableDlRbg();
    for (auto it = rbgMap.begin(); it != rbgMap.end(); it++)
    {
        if (*it)
        {
            rbgAllocatedNum++;
        }
    }

    FfMacSchedSapUser::SchedDlConfigIndParameters ret;

    // update UL HARQ proc id
    for (auto itProcId = m_ulHarqCurrentProcessId.begin();
         itProcId != m_ulHarqCurrentProcessId.end();
         itProcId++)
    {
        (*itProcId).second = ((*itProcId).second + 1) % HARQ_PROC_NUM;
    }

    // RACH Allocation
    std::vector<bool> ulRbMap;
    ulRbMap.resize(m_cschedCellConfig.m_ulBandwidth, false);
    ulRbMap = m_ffrSapProvider->GetAvailableUlRbg();
    uint8_t maxContinuousUlBandwidth = 0;
    uint8_t tmpMinBandwidth = 0;
    uint16_t ffrRbStartOffset = 0;
    uint16_t tmpFfrRbStartOffset = 0;
    uint16_t index = 0;

    for (auto it = ulRbMap.begin(); it != ulRbMap.end(); it++)
    {
        if (*it)
        {
            if (tmpMinBandwidth > maxContinuousUlBandwidth)
            {
                maxContinuousUlBandwidth = tmpMinBandwidth;
                ffrRbStartOffset = tmpFfrRbStartOffset;
            }
            tmpMinBandwidth = 0;
        }
        else
        {
            if (tmpMinBandwidth == 0)
            {
                tmpFfrRbStartOffset = index;
            }
            tmpMinBandwidth++;
        }
        index++;
    }

    if (tmpMinBandwidth > maxContinuousUlBandwidth)
    {
        maxContinuousUlBandwidth = tmpMinBandwidth;
        ffrRbStartOffset = tmpFfrRbStartOffset;
    }

    m_rachAllocationMap.resize(m_cschedCellConfig.m_ulBandwidth, 0);
    uint16_t rbStart = 0;
    rbStart = ffrRbStartOffset;
    for (auto itRach = m_rachList.begin(); itRach != m_rachList.end(); itRach++)
    {
        NS_ASSERT_MSG(m_amc->GetUlTbSizeFromMcs(m_ulGrantMcs, m_cschedCellConfig.m_ulBandwidth) >
                          (*itRach).m_estimatedSize,
                      " Default UL Grant MCS does not allow to send RACH messages");
        BuildRarListElement_s newRar;
        newRar.m_rnti = (*itRach).m_rnti;
        newRar.m_grant.m_rnti = newRar.m_rnti;
        newRar.m_grant.m_mcs = m_ulGrantMcs;
        uint16_t rbgLen = 1;
        uint16_t tbSizeBits = 0;
        while ((tbSizeBits < (*itRach).m_estimatedSize) &&
               (rbStart + rbgLen < (ffrRbStartOffset + maxContinuousUlBandwidth)))
        {
            rbgLen++;
            tbSizeBits = m_amc->GetUlTbSizeFromMcs(m_ulGrantMcs, rbgLen);
        }
        if (tbSizeBits < (*itRach).m_estimatedSize)
        {
            break;
        }
        newRar.m_grant.m_rbStart = rbStart;
        newRar.m_grant.m_rbLen = rbgLen;
        newRar.m_grant.m_tbSize = tbSizeBits / 8;
        newRar.m_grant.m_hopping = false;
        newRar.m_grant.m_tpc = 0;
        newRar.m_grant.m_cqiRequest = false;
        newRar.m_grant.m_ulDelay = false;
        NS_LOG_INFO(this << " UL grant allocated to RNTI " << (*itRach).m_rnti << " rbStart "
                         << rbStart << " rbgLen " << rbgLen << " MCS " << m_ulGrantMcs << " tbSize "
                         << newRar.m_grant.m_tbSize);
        for (uint16_t i = rbStart; i < rbStart + rbgLen; i++)
        {
            m_rachAllocationMap.at(i) = (*itRach).m_rnti;
        }

        if (m_harqOn)
        {
            UlDciListElement_s uldci;
            uldci.m_rnti = newRar.m_rnti;
            uldci.m_rbLen = rbgLen;
            uldci.m_rbStart = rbStart;
            uldci.m_mcs = m_ulGrantMcs;
            uldci.m_tbSize = tbSizeBits / 8;
            uldci.m_ndi = 1;
            uldci.m_cceIndex = 0;
            uldci.m_aggrLevel = 1;
            uldci.m_ueTxAntennaSelection = 3;
            uldci.m_hopping = false;
            uldci.m_n2Dmrs = 0;
            uldci.m_tpc = 0;
            uldci.m_cqiRequest = false;
            uldci.m_ulIndex = 0;
            uldci.m_dai = 1;
            uldci.m_freqHopping = 0;
            uldci.m_pdcchPowerOffset = 0;

            uint8_t harqId = 0;
            auto itProcId = m_ulHarqCurrentProcessId.find(uldci.m_rnti);
            if (itProcId == m_ulHarqCurrentProcessId.end())
            {
                NS_FATAL_ERROR("No info find in HARQ buffer for UE " << uldci.m_rnti);
            }
            harqId = (*itProcId).second;
            auto itDci = m_ulHarqProcessesDciBuffer.find(uldci.m_rnti);
            if (itDci == m_ulHarqProcessesDciBuffer.end())
            {
                NS_FATAL_ERROR("Unable to find RNTI entry in UL DCI HARQ buffer for RNTI "
                               << uldci.m_rnti);
            }
            (*itDci).second.at(harqId) = uldci;
        }

        rbStart = rbStart + rbgLen;
        ret.m_buildRarList.push_back(newRar);
    }
    m_rachList.clear();

    // Process DL HARQ feedback
    RefreshHarqProcesses();
    if (!m_dlInfoListBuffered.empty())
    {
        if (!params.m_dlInfoList.empty())
        {
            NS_LOG_INFO(this << " Received DL-HARQ feedback");
            m_dlInfoListBuffered.insert(m_dlInfoListBuffered.end(),
                                        params.m_dlInfoList.begin(),
                                        params.m_dlInfoList.end());
        }
    }
    else
    {
        if (!params.m_dlInfoList.empty())
        {
            m_dlInfoListBuffered = params.m_dlInfoList;
        }
    }
    if (!m_harqOn)
    {
        m_dlInfoListBuffered.clear();
    }
    std::vector<DlInfoListElement_s> dlInfoListUntxed;
    for (std::size_t i = 0; i < m_dlInfoListBuffered.size(); i++)
    {
        auto itRnti = rntiAllocated.find(m_dlInfoListBuffered.at(i).m_rnti);
        if (itRnti != rntiAllocated.end())
        {
            continue;
        }
        auto nLayers = m_dlInfoListBuffered.at(i).m_harqStatus.size();
        std::vector<bool> retx;
        NS_LOG_INFO(this << " Processing DLHARQ feedback");
        if (nLayers == 1)
        {
            retx.push_back(m_dlInfoListBuffered.at(i).m_harqStatus.at(0) ==
                           DlInfoListElement_s::NACK);
            retx.push_back(false);
        }
        else
        {
            retx.push_back(m_dlInfoListBuffered.at(i).m_harqStatus.at(0) ==
                           DlInfoListElement_s::NACK);
            retx.push_back(m_dlInfoListBuffered.at(i).m_harqStatus.at(1) ==
                           DlInfoListElement_s::NACK);
        }
        if (retx.at(0) || retx.at(1))
        {
            uint16_t rnti = m_dlInfoListBuffered.at(i).m_rnti;
            uint8_t harqId = m_dlInfoListBuffered.at(i).m_harqProcessId;
            NS_LOG_INFO(this << " HARQ retx RNTI " << rnti << " harqId " << (uint16_t)harqId);
            auto itHarq = m_dlHarqProcessesDciBuffer.find(rnti);
            if (itHarq == m_dlHarqProcessesDciBuffer.end())
            {
                NS_FATAL_ERROR("No info find in HARQ buffer for UE " << rnti);
            }

            DlDciListElement_s dci = (*itHarq).second.at(harqId);
            int rv = 0;
            if (dci.m_rv.size() == 1)
            {
                rv = dci.m_rv.at(0);
            }
            else
            {
                rv = (dci.m_rv.at(0) > dci.m_rv.at(1) ? dci.m_rv.at(0) : dci.m_rv.at(1));
            }

            if (rv == 3)
            {
                NS_LOG_INFO("Maximum number of retransmissions reached -> drop process");
                auto it = m_dlHarqProcessesStatus.find(rnti);
                if (it == m_dlHarqProcessesStatus.end())
                {
                    NS_LOG_ERROR("No info find in HARQ buffer for UE (might change eNB) "
                                 << m_dlInfoListBuffered.at(i).m_rnti);
                }
                (*it).second.at(harqId) = 0;
                auto itRlcPdu = m_dlHarqProcessesRlcPduListBuffer.find(rnti);
                if (itRlcPdu == m_dlHarqProcessesRlcPduListBuffer.end())
                {
                    NS_FATAL_ERROR("Unable to find RlcPdcList in HARQ buffer for RNTI "
                                   << m_dlInfoListBuffered.at(i).m_rnti);
                }
                for (std::size_t k = 0; k < (*itRlcPdu).second.size(); k++)
                {
                    (*itRlcPdu).second.at(k).at(harqId).clear();
                }
                continue;
            }
            std::vector<int> dciRbg;
            uint32_t mask = 0x1;
            NS_LOG_INFO("Original RBGs " << dci.m_rbBitmap << " rnti " << dci.m_rnti);
            for (int j = 0; j < 32; j++)
            {
                if (((dci.m_rbBitmap & mask) >> j) == 1)
                {
                    dciRbg.push_back(j);
                    NS_LOG_INFO("\t" << j);
                }
                mask = (mask << 1);
            }
            bool free = true;
            for (std::size_t j = 0; j < dciRbg.size(); j++)
            {
                if (rbgMap.at(dciRbg.at(j)))
                {
                    free = false;
                    break;
                }
            }
            if (free)
            {
                for (std::size_t j = 0; j < dciRbg.size(); j++)
                {
                    rbgMap.at(dciRbg.at(j)) = true;
                    NS_LOG_INFO("RBG " << dciRbg.at(j) << " assigned");
                    rbgAllocatedNum++;
                }
                NS_LOG_INFO(this << " Send retx in the same RBGs");
            }
            else
            {
                uint8_t j = 0;
                uint8_t rbgId = (dciRbg.at(dciRbg.size() - 1) + 1) % rbgNum;
                uint8_t startRbg = dciRbg.at(dciRbg.size() - 1);
                std::vector<bool> rbgMapCopy = rbgMap;
                while ((j < dciRbg.size()) && (startRbg != rbgId))
                {
                    if (!rbgMapCopy.at(rbgId))
                    {
                        rbgMapCopy.at(rbgId) = true;
                        dciRbg.at(j) = rbgId;
                        j++;
                    }
                    rbgId = (rbgId + 1) % rbgNum;
                }
                if (j == dciRbg.size())
                {
                    uint32_t rbgMask = 0;
                    for (std::size_t k = 0; k < dciRbg.size(); k++)
                    {
                        rbgMask = rbgMask + (0x1 << dciRbg.at(k));
                        rbgAllocatedNum++;
                    }
                    dci.m_rbBitmap = rbgMask;
                    rbgMap = rbgMapCopy;
                    NS_LOG_INFO(this << " Move retx in RBGs " << dciRbg.size());
                }
                else
                {
                    dlInfoListUntxed.push_back(m_dlInfoListBuffered.at(i));
                    NS_LOG_INFO(this << " No resource for this retx -> buffer it");
                }
            }
            BuildDataListElement_s newEl;
            auto itRlcPdu = m_dlHarqProcessesRlcPduListBuffer.find(rnti);
            if (itRlcPdu == m_dlHarqProcessesRlcPduListBuffer.end())
            {
                NS_FATAL_ERROR("Unable to find RlcPdcList in HARQ buffer for RNTI " << rnti);
            }
            for (std::size_t j = 0; j < nLayers; j++)
            {
                if (retx.at(j))
                {
                    if (j >= dci.m_ndi.size())
                    {
                        dci.m_ndi.push_back(0);
                        dci.m_rv.push_back(0);
                        dci.m_mcs.push_back(0);
                        dci.m_tbsSize.push_back(0);
                        NS_LOG_INFO(this << " layer " << (uint16_t)j
                                         << " no txed (MIMO transition)");
                    }
                    else
                    {
                        dci.m_ndi.at(j) = 0;
                        dci.m_rv.at(j)++;
                        (*itHarq).second.at(harqId).m_rv.at(j)++;
                        NS_LOG_INFO(this << " layer " << (uint16_t)j << " RV "
                                         << (uint16_t)dci.m_rv.at(j));
                    }
                }
                else
                {
                    dci.m_ndi.at(j) = 0;
                    dci.m_rv.at(j) = 0;
                    dci.m_mcs.at(j) = 0;
                    dci.m_tbsSize.at(j) = 0;
                    NS_LOG_INFO(this << " layer " << (uint16_t)j << " no retx");
                }
            }
            for (std::size_t k = 0; k < (*itRlcPdu).second.at(0).at(dci.m_harqProcess).size(); k++)
            {
                std::vector<RlcPduListElement_s> rlcPduListPerLc;
                for (std::size_t j = 0; j < nLayers; j++)
                {
                    if (retx.at(j))
                    {
                        if (j < dci.m_ndi.size())
                        {
                            NS_LOG_INFO(" layer " << (uint16_t)j << " tb size "
                                                  << dci.m_tbsSize.at(j));
                            rlcPduListPerLc.push_back(
                                (*itRlcPdu).second.at(j).at(dci.m_harqProcess).at(k));
                        }
                    }
                    else
                    {
                        NS_LOG_INFO(" layer " << (uint16_t)j << " tb size " << dci.m_tbsSize.at(j));
                        RlcPduListElement_s emptyElement;
                        emptyElement.m_logicalChannelIdentity = (*itRlcPdu)
                                                                    .second.at(j)
                                                                    .at(dci.m_harqProcess)
                                                                    .at(k)
                                                                    .m_logicalChannelIdentity;
                        emptyElement.m_size = 0;
                        rlcPduListPerLc.push_back(emptyElement);
                    }
                }

                if (!rlcPduListPerLc.empty())
                {
                    newEl.m_rlcPduList.push_back(rlcPduListPerLc);
                }
            }
            newEl.m_rnti = rnti;
            newEl.m_dci = dci;
            (*itHarq).second.at(harqId).m_rv = dci.m_rv;
            auto itHarqTimer = m_dlHarqProcessesTimer.find(rnti);
            if (itHarqTimer == m_dlHarqProcessesTimer.end())
            {
                NS_FATAL_ERROR("Unable to find HARQ timer for RNTI " << (uint16_t)rnti);
            }
            (*itHarqTimer).second.at(harqId) = 0;
            ret.m_buildDataList.push_back(newEl);
            rntiAllocated.insert(rnti);
        }
        else
        {
            NS_LOG_INFO(this << " HARQ received ACK for UE " << m_dlInfoListBuffered.at(i).m_rnti);
            auto it = m_dlHarqProcessesStatus.find(m_dlInfoListBuffered.at(i).m_rnti);
            if (it == m_dlHarqProcessesStatus.end())
            {
                NS_FATAL_ERROR("No info find in HARQ buffer for UE "
                               << m_dlInfoListBuffered.at(i).m_rnti);
            }
            (*it).second.at(m_dlInfoListBuffered.at(i).m_harqProcessId) = 0;
            auto itRlcPdu =
                m_dlHarqProcessesRlcPduListBuffer.find(m_dlInfoListBuffered.at(i).m_rnti);
            if (itRlcPdu == m_dlHarqProcessesRlcPduListBuffer.end())
            {
                NS_FATAL_ERROR("Unable to find RlcPdcList in HARQ buffer for RNTI "
                               << m_dlInfoListBuffered.at(i).m_rnti);
            }
            for (std::size_t k = 0; k < (*itRlcPdu).second.size(); k++)
            {
                (*itRlcPdu).second.at(k).at(m_dlInfoListBuffered.at(i).m_harqProcessId).clear();
            }
        }
    }
    m_dlInfoListBuffered.clear();
    m_dlInfoListBuffered = dlInfoListUntxed;

    if (rbgAllocatedNum == rbgNum)
    {
        if (!ret.m_buildDataList.empty() || !ret.m_buildRarList.empty())
        {
            m_schedSapUser->SchedDlConfigInd(ret);
        }
        return;
    }

    int nflows = 0;
    for (auto it = m_rlcBufferReq.begin(); it != m_rlcBufferReq.end(); it++)
    {
        if ((*it).second.m_rlcTransmissionQueueSize > 0 ||
            (*it).second.m_rlcRetransmissionQueueSize > 0 || (*it).second.m_rlcStatusPduSize > 0)
        {
            nflows++;
        }
    }

    if (nflows == 0)
    {
        ret.m_nrOfPdcchOfdmSymbols = 1;
        m_schedSapUser->SchedDlConfigInd(ret);
        return;
    }

    // -- SHARED-MEMORY INTERFACE (CPP2PY) ---------------------------
    m_shmInterface->CppSendBegin();
    Cpp2Py* cpp2Py = m_shmInterface->GetCpp2PyStruct();
    E3Indication* e3Indication = &(cpp2Py->e3Indication);
    E2KpmReport* e2KpmReport = &(cpp2Py->e2KpmReport);

    InitE3Indication(e3Indication);
    InitE2KpmReport(e2KpmReport);

    int rbgStart = 0;
    for (size_t i = 0; i < rbgMap.size(); i++)
    {
        if (!rbgMap[i])
        {
            rbgStart = static_cast<int>(i);
            break;
        }
    }
    uint16_t numRbg = rbgNum - rbgAllocatedNum;

    // collect per-UE info
    uint32_t ueIdx = 0;
    uint32_t totalBuffer = 0;
    std::set<uint16_t> addedRntis;

    std::map<uint16_t, unsigned int> lcActivesPerRnti;
    for (auto it = m_rlcBufferReq.begin(); it != m_rlcBufferReq.end(); it++)
    {
        uint16_t rnti = (*it).first.m_rnti;
        if ((*it).second.m_rlcTransmissionQueueSize > 0 ||
            (*it).second.m_rlcRetransmissionQueueSize > 0 || (*it).second.m_rlcStatusPduSize > 0)
        {
            lcActivesPerRnti[rnti]++;
        }
    }

    for (auto it = m_rlcBufferReq.begin(); it != m_rlcBufferReq.end() && ueIdx < MAX_UE_NUM; ++it)
    {
        uint16_t rnti = (*it).first.m_rnti;

        if (addedRntis.find(rnti) != addedRntis.end())
        {
            continue;
        }

        if (lcActivesPerRnti.find(rnti) == lcActivesPerRnti.end())
        {
            continue;
        }

        uint32_t rntiTotalBuffer = 0;
        for (auto itLC = m_rlcBufferReq.begin(); itLC != m_rlcBufferReq.end(); itLC++)
        {
            if ((*itLC).first.m_rnti == rnti)
            {
                rntiTotalBuffer += (*itLC).second.m_rlcTransmissionQueueSize +
                                   (*itLC).second.m_rlcRetransmissionQueueSize +
                                   (*itLC).second.m_rlcStatusPduSize;
            }
        }

        if (rntiTotalBuffer == 0)
        {
            continue;
        }

        uint8_t rntiCriticality =
            SideInfoTracker::GetInstance().GetRntiCriticality(rnti, LinkDirection::DOWNLINK);

        std::string rntiContentId =
            SideInfoTracker::GetInstance().GetRntiObjectId(rnti, LinkDirection::DOWNLINK);

        std::map<uint16_t, uint8_t>::iterator itCqi = m_p10CqiRxed.find(rnti);
        uint8_t rntiCqi = (itCqi != m_p10CqiRxed.end()) ? (*itCqi).second : 1;

        // Full-band rate at current CQI (TBS bits per 1 ms TTI -> Mbps)
        const uint8_t rntiMcsFull = m_amc->GetMcsFromCqi(rntiCqi);
        const double rntiAchievableRate =
            m_amc->GetDlTbSizeFromMcs(rntiMcsFull, rbgNum * rbgSize) / 1e3;
        const double rntiActualThroughput = m_dlTput.GetMbps(rnti, nowNs);

        uint8_t rntiQci = GetQciForRnti(rnti);

        const uint32_t rntiPacketDelay = m_dlWaiting.GetWaitingMs(rnti, nowNs);

        uint32_t rntiTotalBytesServed = 0;
        auto itByteServed = m_ueDlTotalBytesServed.find(rnti);
        if (itByteServed != m_ueDlTotalBytesServed.end())
        {
            rntiTotalBytesServed = itByteServed->second;
        }

        e3Indication->ue[ueIdx].rnti = rnti;
        e3Indication->ue[ueIdx].cqi = rntiCqi;
        e3Indication->ue[ueIdx].bufferLevel = rntiTotalBuffer;
        e3Indication->ue[ueIdx].contentCriticality = rntiCriticality;
        std::snprintf(e3Indication->ue[ueIdx].contentId, MAX_ID_LEN, "%s", rntiContentId.c_str());
        e3Indication->ue[ueIdx].qci = rntiQci;
        e3Indication->ue[ueIdx].achievableRate = rntiAchievableRate;
        e3Indication->ue[ueIdx].actualThroughput = rntiActualThroughput;
        e3Indication->ue[ueIdx].totalBytesServed = rntiTotalBytesServed;
        e3Indication->ue[ueIdx].packetDelay = rntiPacketDelay;

        e2KpmReport->ue[ueIdx].rnti = rnti;
        e2KpmReport->ue[ueIdx].cqi = rntiCqi;
        e2KpmReport->ue[ueIdx].bufferLevel = rntiTotalBuffer;
        std::snprintf(e2KpmReport->ue[ueIdx].contentId, MAX_ID_LEN, "%s", rntiContentId.c_str());
        e2KpmReport->ue[ueIdx].qci = rntiQci;
        e2KpmReport->ue[ueIdx].achievableRate = rntiAchievableRate;
        e2KpmReport->ue[ueIdx].actualThroughput = rntiActualThroughput;
        e2KpmReport->ue[ueIdx].totalBytesServed = rntiTotalBytesServed;
        e2KpmReport->ue[ueIdx].packetDelay = rntiPacketDelay;

        totalBuffer += rntiTotalBuffer;
        addedRntis.insert(rnti);
        ++ueIdx;
    }

    if (ueIdx == 0)
    {
        if ((!ret.m_buildDataList.empty()) || (!ret.m_buildRarList.empty()))
        {
            NS_LOG_INFO(this << " No UEs to schedule; finishing DL scheduling");
            m_schedSapUser->SchedDlConfigInd(ret);
        }
        return;
    }

    uint64_t timestamp = Simulator::Now().GetNanoSeconds();
    uint32_t subframeNo = (params.m_sfnSf & 0x000F) - 1;
    uint32_t frameNo = (params.m_sfnSf >> 4) & 0x03FF;
    m_dlE3IndicationId += 1;
    m_dlE2KpmReportId += 1;

    e3Indication->type = E3SM_INDICATION;
    e3Indication->timestamp = timestamp;
    e3Indication->subframeNo = subframeNo;
    e3Indication->frameNo = frameNo;
    e3Indication->numUe = ueIdx;
    e3Indication->totalBufferLevel = totalBuffer;
    e3Indication->initRbg = rbgStart;
    e3Indication->numRbg = numRbg;
    e3Indication->rbgSize = rbgSize;
    e3Indication->linkDirection = DOWNLINK;
    e3Indication->id = m_dlE3IndicationId;

    e2KpmReport->type = E2SM_KPM_REPORT;
    e2KpmReport->timestamp = timestamp;
    e2KpmReport->frameNo = frameNo;
    e2KpmReport->numUe = ueIdx;
    e2KpmReport->totalBufferLevel = totalBuffer;
    e2KpmReport->numRbg = numRbg;
    e2KpmReport->rbgSize = rbgSize;
    e2KpmReport->linkDirection = DOWNLINK;
    e2KpmReport->id = m_dlE2KpmReportId;

    m_shmInterface->CppSendEnd();

    // -- SHARED-MEMORY INTERFACE (PY2CPP) ---------------------------
    m_shmInterface->CppRecvBegin();
    Py2Cpp* py2Cpp = m_shmInterface->GetPy2CppStruct();
    E3Control* e3Control = &(py2Cpp->e3Control);
    E2RcControl* e2RcControl = &(py2Cpp->e2RcControl);

    bool validCtrl = true;
    if (e3Control->type != E3SM_CONTROL)
    {
        std::cerr << "ib-ORAN LTE MAC Scheduler: dApp responded with an invalid message type.\n";
        validCtrl = false;
    }
    else if (e2RcControl->type != E2SM_RC_CONTROL)
    {
        std::cerr << "ib-ORAN LTE MAC Scheduler: xApp responded with an invalid message type.\n";
        validCtrl = false;
    }
    else if (e3Control->linkDirection != DOWNLINK || e2RcControl->linkDirection != DOWNLINK)
    {
        std::cerr
            << "ib-ORAN LTE MAC Scheduler: controller responded to a different link direction.\n";
        validCtrl = false;
    }

    if (validCtrl)
    {
        uint32_t maxUeSchedSize = e2RcControl->maxUeSchedSize;
        uint32_t ueSchedSize = std::min(e3Control->ueSchedSize, maxUeSchedSize);
        if (!e3Control->skipThisStep && !e2RcControl->skipThisStep && ueSchedSize > 0)
        {
            for (uint32_t i = 0; i < e2RcControl->maxUeSchedSize; i++)
            {
                uint16_t rntiFromXApp = e2RcControl->ue[i].rnti;
                m_ueDlMaxRbgAllocation[rntiFromXApp] = e2RcControl->ue[i].maxRbgAllocation;
            }

            std::map<uint16_t, unsigned int> lcActivesPerRnti;
            for (auto it = m_rlcBufferReq.begin(); it != m_rlcBufferReq.end(); it++)
            {
                uint16_t rnti = (*it).first.m_rnti;
                if ((*it).second.m_rlcTransmissionQueueSize > 0 ||
                    (*it).second.m_rlcRetransmissionQueueSize > 0 ||
                    (*it).second.m_rlcStatusPduSize > 0)
                {
                    lcActivesPerRnti[rnti]++;
                }
            }

            uint32_t totalRbAllocated = 0;
            for (uint32_t i = 0; i < ueSchedSize; i++)
            {
                uint16_t rnti = e3Control->ue[i].rnti;
                uint16_t rbgStart = e3Control->ue[i].initAllocatedRbg;
                uint16_t rbgLen = e3Control->ue[i].numAllocatedRbg;
                uint8_t mcs = e3Control->ue[i].mcs;

                uint16_t maxRbgAllocation = 0;
                auto it = m_ueDlMaxRbgAllocation.find(rnti);
                if (it != m_ueDlMaxRbgAllocation.end())
                {
                    maxRbgAllocation = it->second;
                    rbgLen = std::min(rbgLen, maxRbgAllocation);
                }
                else
                {
                    maxRbgAllocation = rbgLen;
                }

                std::map<uint16_t, uint8_t>::iterator itTxMode = m_uesTxMode.find(rnti);
                if (itTxMode == m_uesTxMode.end())
                {
                    std::cerr << "ib-ORAN LTE MAC Scheduler: dApp sent invalid RNTI " << rnti
                              << ": Skipping.\n";
                    continue;
                }

                if (mcs > 28)
                {
                    std::cerr << "ib-ORAN LTE MAC Scheduler: dApp sent invalid MCS " << (int)mcs
                              << " for RNTI " << rnti << ": Skipping.\n";
                    continue;
                }

                if (rbgLen == 0)
                {
                    continue;
                }

                if (rbgStart + rbgLen > rbgNum || rbgLen > maxRbgAllocation)
                {
                    std::cerr << "ib-ORAN LTE MAC Scheduler: dApp sent out-of-bound RB allocation "
                                 "for RNTI "
                              << rnti << ": Skipping.\n";
                    continue;
                }

                if (totalRbAllocated + rbgLen > (uint32_t)(rbgNum - rbgAllocatedNum))
                {
                    NS_LOG_WARN("Controller over-allocated RBs: Stopping allocations");
                    break;
                }

                // Checks that consume nothing
                int nLayers = TransmissionModesLayers::TxMode2LayerNum((*itTxMode).second);
                if (nLayers <= 0 || nLayers > 2)
                {
                    NS_LOG_ERROR("Invalid nLayers " << nLayers << " for RNTI " << rnti);
                    continue;
                }

                auto itLcRnti = lcActivesPerRnti.find(rnti);
                if (itLcRnti == lcActivesPerRnti.end())
                {
                    NS_LOG_WARN("No LC info for RNTI " << rnti);
                    continue;
                }

                if (!HarqProcessAvailability(rnti))
                {
                    NS_LOG_WARN("No free HARQ process for RNTI " << rnti << ": skipping");
                    continue;
                }

                uint32_t rbgMask = 0;
                bool conflictFound = false;
                for (int k = rbgStart; k < rbgStart + rbgLen && k < rbgNum; k++)
                {
                    if (rbgMap.at(k))
                    {
                        conflictFound = true;
                        break;
                    }
                    rbgMask |= (0x1u << k);
                }

                if (conflictFound)
                {
                    NS_LOG_ERROR("RB conflict for RNTI " << rnti << ": skipping UE");
                    continue;
                }

                // Commit resources
                for (int k = rbgStart; k < rbgStart + rbgLen && k < rbgNum; k++)
                {
                    rbgMap.at(k) = true;
                    rbgAllocatedNum++;
                }
                totalRbAllocated += rbgLen;

                BuildDataListElement_s newEl;
                newEl.m_rnti = rnti;
                DlDciListElement_s dlDci;
                dlDci.m_rnti = rnti;
                dlDci.m_harqProcess = UpdateHarqProcessId(rnti);
                dlDci.m_resAlloc = 0;
                dlDci.m_rbBitmap = rbgMask;

                int lcNum = itLcRnti->second;

                int tbSize = (m_amc->GetDlTbSizeFromMcs(mcs, rbgLen * rbgSize) / 8);
                if (tbSize < lcNum * MINIMUM_PDU_SIZE)
                {
                    NS_LOG_WARN("Allocated TBS (" << tbSize << ") too small for " << lcNum
                                                  << " LCs. Reducing LC count.");
                    lcNum = std::max(1, (int)(tbSize / MINIMUM_PDU_SIZE));
                }
                uint16_t rlcPduSize = tbSize / lcNum;

                m_ueDlTotalBytesServed[rnti] += tbSize;

                for (int layer = 0; layer < nLayers; layer++)
                {
                    dlDci.m_mcs.push_back(mcs);
                    dlDci.m_ndi.push_back(1);
                    dlDci.m_rv.push_back(0);
                    dlDci.m_tbsSize.push_back(tbSize);
                }

                uint32_t rntiBufBefore = 0;
                for (const auto& [flow, req] : m_rlcBufferReq)
                {
                    if (flow.m_rnti == rnti)
                    {
                        rntiBufBefore += req.m_rlcTransmissionQueueSize +
                                         req.m_rlcRetransmissionQueueSize + req.m_rlcStatusPduSize;
                    }
                }
                m_dlTput.AddServed(
                    rnti,
                    nowNs,
                    std::min<uint32_t>(static_cast<uint32_t>(tbSize) * nLayers, rntiBufBefore));

                std::map<LteFlowId_t,
                         FfMacSchedSapProvider::SchedDlRlcBufferReqParameters>::iterator itRlc;
                for (itRlc = m_rlcBufferReq.begin(); itRlc != m_rlcBufferReq.end(); itRlc++)
                {
                    if ((*itRlc).first.m_rnti != rnti)
                    {
                        continue;
                    }

                    uint32_t lcBufferSize = (*itRlc).second.m_rlcTransmissionQueueSize +
                                            (*itRlc).second.m_rlcRetransmissionQueueSize +
                                            (*itRlc).second.m_rlcStatusPduSize;

                    if (lcBufferSize == 0)
                    {
                        continue;
                    }

                    if (lcNum <= 0)
                    {
                        NS_LOG_WARN("lcNum exhausted for RNTI " << rnti
                                                                << ": stopping LC scheduling");
                        break;
                    }

                    std::vector<struct RlcPduListElement_s> newRlcPduLe;
                    for (uint8_t layer = 0; layer < nLayers; layer++)
                    {
                        RlcPduListElement_s newRlcEl;
                        newRlcEl.m_logicalChannelIdentity = (*itRlc).first.m_lcId;
                        newRlcEl.m_size = rlcPduSize;

                        NS_LOG_INFO(this << " RNTI " << rnti << " LCID "
                                         << (uint32_t)newRlcEl.m_logicalChannelIdentity << " size "
                                         << rlcPduSize << " layer " << (uint16_t)layer);

                        UpdateDlRlcBufferInfo((*itRlc).first.m_rnti,
                                              newRlcEl.m_logicalChannelIdentity,
                                              rlcPduSize);

                        newRlcPduLe.push_back(newRlcEl);

                        if (m_harqOn)
                        {
                            std::map<uint16_t, DlHarqRlcPduListBuffer_t>::iterator itRlcPdu =
                                m_dlHarqProcessesRlcPduListBuffer.find(rnti);
                            if (itRlcPdu != m_dlHarqProcessesRlcPduListBuffer.end())
                            {
                                (*itRlcPdu)
                                    .second.at(layer)
                                    .at(dlDci.m_harqProcess)
                                    .push_back(newRlcEl);
                            }
                            else
                            {
                                NS_FATAL_ERROR("No RNTI entry in DL RLC PDU HARQ buffer for RNTI "
                                               << rnti);
                            }
                        }
                    }
                    newEl.m_rlcPduList.push_back(newRlcPduLe);
                    lcNum--;
                }

                dlDci.m_tpc = 1;
                newEl.m_dci = dlDci;

                if (m_harqOn)
                {
                    std::map<uint16_t, DlHarqProcessesDciBuffer_t>::iterator itDci =
                        m_dlHarqProcessesDciBuffer.find(rnti);
                    if (itDci == m_dlHarqProcessesDciBuffer.end())
                    {
                        NS_FATAL_ERROR("No RNTI entry in DL DCI HARQ buffer for RNTI " << rnti);
                    }
                    (*itDci).second.at(dlDci.m_harqProcess) = dlDci;

                    std::map<uint16_t, DlHarqProcessesTimer_t>::iterator itHarqTimer =
                        m_dlHarqProcessesTimer.find(rnti);
                    if (itHarqTimer == m_dlHarqProcessesTimer.end())
                    {
                        NS_FATAL_ERROR("No RNTI entry in DL HARQ timer buffer for RNTI " << rnti);
                    }
                    (*itHarqTimer).second.at(dlDci.m_harqProcess) = 0;
                }
                m_dlWaiting.MarkServed(rnti, nowNs);
                ret.m_buildDataList.push_back(newEl);
            }
        }
    }

    m_shmInterface->CppRecvEnd();
    ret.m_nrOfPdcchOfdmSymbols = 1;
    m_schedSapUser->SchedDlConfigInd(ret);
}

void
IbOranLteMacScheduler::DoSchedDlRachInfoReq(
    const FfMacSchedSapProvider::SchedDlRachInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    m_rachList = params.m_rachList;
}

void
IbOranLteMacScheduler::DoSchedDlCqiInfoReq(
    const FfMacSchedSapProvider::SchedDlCqiInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    m_ffrSapProvider->ReportDlCqiInfo(params);

    for (unsigned int i = 0; i < params.m_cqiList.size(); i++)
    {
        if (params.m_cqiList.at(i).m_cqiType == CqiListElement_s::P10)
        {
            NS_LOG_LOGIC("wideband CQI " << (uint32_t)params.m_cqiList.at(i).m_wbCqi.at(0)
                                         << " reported");
            uint16_t rnti = params.m_cqiList.at(i).m_rnti;
            auto it = m_p10CqiRxed.find(rnti);
            if (it == m_p10CqiRxed.end())
            {
                m_p10CqiRxed[rnti] = params.m_cqiList.at(i).m_wbCqi.at(0);
                m_p10CqiTimers[rnti] = m_cqiTimersThreshold;
            }
            else
            {
                (*it).second = params.m_cqiList.at(i).m_wbCqi.at(0);
                auto itTimers = m_p10CqiTimers.find(rnti);
                (*itTimers).second = m_cqiTimersThreshold;
            }
        }
        else if (params.m_cqiList.at(i).m_cqiType == CqiListElement_s::A30)
        {
            uint16_t rnti = params.m_cqiList.at(i).m_rnti;
            auto it = m_a30CqiRxed.find(rnti);
            if (it == m_a30CqiRxed.end())
            {
                m_a30CqiRxed[rnti] = params.m_cqiList.at(i).m_sbMeasResult;
                m_a30CqiTimers[rnti] = m_cqiTimersThreshold;
            }
            else
            {
                (*it).second = params.m_cqiList.at(i).m_sbMeasResult;
                auto itTimers = m_a30CqiTimers.find(rnti);
                (*itTimers).second = m_cqiTimersThreshold;
            }
        }
        else
        {
            NS_LOG_ERROR(this << " CQI type unknown");
        }
    }
}

double
IbOranLteMacScheduler::EstimateUlSinr(uint16_t rnti, uint16_t rb)
{
    auto itCqi = m_ueCqi.find(rnti);
    if (itCqi == m_ueCqi.end())
    {
        return NO_SINR;
    }
    else
    {
        double sinrSum = 0;
        unsigned int sinrNum = 0;
        for (uint32_t i = 0; i < m_cschedCellConfig.m_ulBandwidth; i++)
        {
            double sinr = (*itCqi).second.at(i);
            if (sinr != NO_SINR)
            {
                sinrSum += sinr;
                sinrNum++;
            }
        }
        double estimatedSinr = (sinrNum > 0) ? (sinrSum / sinrNum) : DBL_MAX;
        (*itCqi).second.at(rb) = estimatedSinr;
        return estimatedSinr;
    }
}

void
IbOranLteMacScheduler::DoSchedUlTriggerReq(
    const FfMacSchedSapProvider::SchedUlTriggerReqParameters& params)
{
    NS_LOG_FUNCTION(this << " UL - Frame no. " << (params.m_sfnSf >> 4) << " subframe no. "
                         << (0xF & params.m_sfnSf) << " size " << params.m_ulInfoList.size());

    RefreshUlCqiMaps();
    m_ffrSapProvider->ReportUlCqiInfo(m_ueCqi);

    const uint64_t nowNs = Simulator::Now().GetNanoSeconds();
    {
        std::set<uint16_t> ulBacklogged;
        for (const auto& [rnti, buffer] : m_ceBsrRxed)
        {
            if (buffer > 0)
            {
                ulBacklogged.insert(rnti);
            }
        }
        m_ulWaiting.Retain(ulBacklogged);
    }

    FfMacSchedSapUser::SchedUlConfigIndParameters ret;
    std::vector<bool> rbMap;
    uint16_t rbAllocatedNum = 0;
    std::set<uint16_t> rntiAllocated;
    std::vector<uint16_t> rbgAllocationMap;
    rbgAllocationMap = m_rachAllocationMap;
    m_rachAllocationMap.clear();
    m_rachAllocationMap.resize(m_cschedCellConfig.m_ulBandwidth, 0);

    rbMap.resize(m_cschedCellConfig.m_ulBandwidth, false);
    rbMap = m_ffrSapProvider->GetAvailableUlRbg();

    for (auto it = rbMap.begin(); it != rbMap.end(); it++)
    {
        if (*it)
        {
            rbAllocatedNum++;
        }
    }

    for (uint16_t i = 0; i < m_cschedCellConfig.m_ulBandwidth; i++)
    {
        if (rbgAllocationMap.at(i) != 0)
        {
            rbMap.at(i) = true;
            NS_LOG_DEBUG(this << " Allocated for RACH " << i);
        }
    }

    if (m_harqOn)
    {
        for (std::size_t i = 0; i < params.m_ulInfoList.size(); i++)
        {
            if (params.m_ulInfoList.at(i).m_receptionStatus == UlInfoListElement_s::NotOk)
            {
                uint16_t rnti = params.m_ulInfoList.at(i).m_rnti;
                auto itProcId = m_ulHarqCurrentProcessId.find(rnti);
                if (itProcId == m_ulHarqCurrentProcessId.end())
                {
                    NS_LOG_ERROR("No info find in HARQ buffer for UE (might change eNB) " << rnti);
                }
                uint8_t harqId = (uint8_t)((*itProcId).second - HARQ_PERIOD) % HARQ_PROC_NUM;
                NS_LOG_INFO(this << " UL-HARQ retx RNTI " << rnti << " harqId " << (uint16_t)harqId
                                 << " i " << i << " size " << params.m_ulInfoList.size());
                auto itHarq = m_ulHarqProcessesDciBuffer.find(rnti);
                if (itHarq == m_ulHarqProcessesDciBuffer.end())
                {
                    NS_LOG_ERROR("No info find in HARQ buffer for UE (might change eNB) " << rnti);
                    continue;
                }
                UlDciListElement_s dci = (*itHarq).second.at(harqId);
                auto itStat = m_ulHarqProcessesStatus.find(rnti);
                if (itStat == m_ulHarqProcessesStatus.end())
                {
                    NS_LOG_ERROR("No info find in HARQ buffer for UE (might change eNB) " << rnti);
                }
                if ((*itStat).second.at(harqId) >= 3)
                {
                    NS_LOG_INFO("Max number of retransmissions reached (UL)-> drop process");
                    continue;
                }
                bool free = true;
                for (int j = dci.m_rbStart; j < dci.m_rbStart + dci.m_rbLen; j++)
                {
                    if (rbMap.at(j))
                    {
                        free = false;
                        NS_LOG_INFO(this << " BUSY " << j);
                    }
                }
                if (free)
                {
                    for (int j = dci.m_rbStart; j < dci.m_rbStart + dci.m_rbLen; j++)
                    {
                        rbMap.at(j) = true;
                        rbgAllocationMap.at(j) = dci.m_rnti;
                        NS_LOG_INFO("\tRB " << j);
                        rbAllocatedNum++;
                    }
                    NS_LOG_INFO(this << " Send retx in the same RBs " << (uint16_t)dci.m_rbStart
                                     << " to " << dci.m_rbStart + dci.m_rbLen << " RV "
                                     << (*itStat).second.at(harqId) + 1);
                }
                else
                {
                    NS_LOG_INFO("Cannot allocate retx due to RACH allocations for UE " << rnti);
                    continue;
                }
                dci.m_ndi = 0;
                (*itStat).second.at((*itProcId).second) = (*itStat).second.at(harqId) + 1;
                (*itStat).second.at(harqId) = 0;
                (*itHarq).second.at((*itProcId).second) = dci;
                ret.m_dciList.push_back(dci);
                rntiAllocated.insert(dci.m_rnti);
            }
            else
            {
                NS_LOG_INFO(this << " HARQ-ACK feedback from RNTI "
                                 << params.m_ulInfoList.at(i).m_rnti);
            }
        }
    }

    std::map<uint16_t, uint32_t>::iterator it;
    int nflows = 0;
    for (it = m_ceBsrRxed.begin(); it != m_ceBsrRxed.end(); it++)
    {
        auto itRnti = rntiAllocated.find((*it).first);
        if (((*it).second > 0) && (itRnti == rntiAllocated.end()))
        {
            nflows++;
        }
    }

    if (nflows == 0)
    {
        if (!ret.m_dciList.empty())
        {
            m_allocationMaps[params.m_sfnSf] = rbgAllocationMap;
            m_schedSapUser->SchedUlConfigInd(ret);
        }
        return;
    }

    // -- SHARED-MEMORY INTERFACE (CPP2PY) ---------------------------
    m_shmInterface->CppSendBegin();
    Cpp2Py* cpp2Py = m_shmInterface->GetCpp2PyStruct();
    E3Indication* e3Indication = &(cpp2Py->e3Indication);
    E2KpmReport* e2KpmReport = &(cpp2Py->e2KpmReport);

    InitE3Indication(e3Indication);
    InitE2KpmReport(e2KpmReport);

    int rbgStart = 0;
    for (size_t i = 0; i < rbMap.size(); i++)
    {
        if (!rbMap[i])
        {
            rbgStart = static_cast<int>(i);
            break;
        }
    }
    uint16_t numRb = m_cschedCellConfig.m_ulBandwidth - rbAllocatedNum;

    uint32_t ueIdx = 0;
    uint32_t totalBuffer = 0;

    for (it = m_ceBsrRxed.begin(); it != m_ceBsrRxed.end() && ueIdx < MAX_UE_NUM; ++it)
    {
        uint16_t rnti = (*it).first;
        uint32_t rntiTotalBuffer = (*it).second;

        if (rntiAllocated.find(rnti) != rntiAllocated.end() || rntiTotalBuffer == 0)
        {
            continue;
        }

        uint8_t rntiCriticality =
            SideInfoTracker::GetInstance().GetRntiCriticality(rnti, LinkDirection::UPLINK);

        std::string rntiContentId =
            SideInfoTracker::GetInstance().GetRntiObjectId(rnti, LinkDirection::UPLINK);

        std::map<uint16_t, std::vector<double>>::iterator itCqi = m_ueCqi.find(rnti);

        uint8_t rntiCqi = 1;
        if (itCqi != m_ueCqi.end())
        {
            double sum = 0.0;
            unsigned n = 0;
            for (double sinr : itCqi->second)
            {
                if (sinr != NO_SINR)
                {
                    sum += sinr;
                    ++n;
                }
            }
            if (n > 0)
            {
                rntiCqi = GetCqiFromSinr(sum / n);
            }
        }

        const uint8_t rntiMcsFull = m_amc->GetMcsFromCqi(rntiCqi);
        const double rntiAchievableRate =
            m_amc->GetUlTbSizeFromMcs(rntiMcsFull, m_cschedCellConfig.m_ulBandwidth) / 1e3;
        const double rntiActualThroughput = m_ulTput.GetMbps(rnti, nowNs);

        uint8_t rntiQci = GetQciForRnti(rnti);

        const uint32_t rntiPacketDelay = m_ulWaiting.GetWaitingMs(rnti, nowNs);

        uint32_t rntiTotalBytesServed = 0;
        auto itUlByteServed = m_ueUlTotalBytesServed.find(rnti);
        if (itUlByteServed != m_ueUlTotalBytesServed.end())
        {
            rntiTotalBytesServed = itUlByteServed->second;
        }

        e3Indication->ue[ueIdx].rnti = rnti;
        e3Indication->ue[ueIdx].cqi = rntiCqi;
        e3Indication->ue[ueIdx].bufferLevel = rntiTotalBuffer;
        e3Indication->ue[ueIdx].contentCriticality = rntiCriticality;
        std::snprintf(e3Indication->ue[ueIdx].contentId, MAX_ID_LEN, "%s", rntiContentId.c_str());
        e3Indication->ue[ueIdx].qci = rntiQci;
        e3Indication->ue[ueIdx].achievableRate = rntiAchievableRate;
        e3Indication->ue[ueIdx].actualThroughput = rntiActualThroughput;
        e3Indication->ue[ueIdx].totalBytesServed = rntiTotalBytesServed;
        e3Indication->ue[ueIdx].packetDelay = rntiPacketDelay;

        e2KpmReport->ue[ueIdx].rnti = rnti;
        e2KpmReport->ue[ueIdx].cqi = rntiCqi;
        e2KpmReport->ue[ueIdx].bufferLevel = rntiTotalBuffer;
        std::snprintf(e2KpmReport->ue[ueIdx].contentId, MAX_ID_LEN, "%s", rntiContentId.c_str());
        e2KpmReport->ue[ueIdx].qci = rntiQci;
        e2KpmReport->ue[ueIdx].achievableRate = rntiAchievableRate;
        e2KpmReport->ue[ueIdx].actualThroughput = rntiActualThroughput;
        e2KpmReport->ue[ueIdx].totalBytesServed = rntiTotalBytesServed;
        e2KpmReport->ue[ueIdx].packetDelay = rntiPacketDelay;

        totalBuffer += rntiTotalBuffer;
        ++ueIdx;
    }

    if (ueIdx == 0)
    {
        if (!ret.m_dciList.empty())
        {
            m_allocationMaps.insert(
                std::pair<uint16_t, std::vector<uint16_t>>(params.m_sfnSf, rbgAllocationMap));
            m_schedSapUser->SchedUlConfigInd(ret);
        }
        return;
    }

    uint64_t timestamp = Simulator::Now().GetNanoSeconds();
    uint32_t subframeNo = (params.m_sfnSf & 0x000F) - 1;
    uint32_t frameNo = (params.m_sfnSf >> 4) & 0x03FF;
    m_ulE3IndicationId += 1;
    m_ulE2KpmReportId += 1;

    e3Indication->type = E3SM_INDICATION;
    e3Indication->timestamp = timestamp;
    e3Indication->subframeNo = subframeNo;
    e3Indication->frameNo = frameNo;
    e3Indication->numUe = ueIdx;
    e3Indication->totalBufferLevel = totalBuffer;
    e3Indication->initRbg = rbgStart;
    e3Indication->numRbg = numRb;
    e3Indication->rbgSize = 1;
    e3Indication->linkDirection = UPLINK;
    e3Indication->id = m_ulE3IndicationId;

    e2KpmReport->type = E2SM_KPM_REPORT;
    e2KpmReport->timestamp = timestamp;
    e2KpmReport->frameNo = frameNo;
    e2KpmReport->numUe = ueIdx;
    e2KpmReport->totalBufferLevel = totalBuffer;
    e2KpmReport->numRbg = numRb;
    e2KpmReport->rbgSize = 1;
    e2KpmReport->linkDirection = UPLINK;
    e2KpmReport->id = m_ulE2KpmReportId;

    m_shmInterface->CppSendEnd();

    // -- SHARED-MEMORY INTERFACE (PY2CPP) ---------------------------
    m_shmInterface->CppRecvBegin();
    Py2Cpp* py2Cpp = m_shmInterface->GetPy2CppStruct();
    E3Control* e3Control = &(py2Cpp->e3Control);
    E2RcControl* e2RcControl = &(py2Cpp->e2RcControl);

    bool validCtrl = true;
    if (e3Control->type != E3SM_CONTROL)
    {
        std::cerr << "ib-ORAN LTE MAC Scheduler: dApp responded with an invalid message type.\n";
        validCtrl = false;
    }
    else if (e2RcControl->type != E2SM_RC_CONTROL)
    {
        std::cerr << "ib-ORAN LTE MAC Scheduler: xApp responded with an invalid message type.\n";
        validCtrl = false;
    }
    else if (e3Control->linkDirection != UPLINK || e2RcControl->linkDirection != UPLINK)
    {
        std::cerr
            << "ib-ORAN LTE MAC Scheduler: controller responded to a different link direction.\n";
        validCtrl = false;
    }

    if (validCtrl)
    {
        uint32_t maxUeSchedSize = e2RcControl->maxUeSchedSize;
        uint32_t ueSchedSize = std::min(e3Control->ueSchedSize, maxUeSchedSize);
        if (!e3Control->skipThisStep && !e2RcControl->skipThisStep && ueSchedSize > 0)
        {
            for (uint32_t i = 0; i < e2RcControl->maxUeSchedSize; i++)
            {
                uint16_t rntiFromXApp = e2RcControl->ue[i].rnti;
                m_ueUlMaxRbgAllocation[rntiFromXApp] = e2RcControl->ue[i].maxRbgAllocation;
            }

            uint32_t totalRbAllocated = 0;
            for (uint32_t i = 0; i < ueSchedSize; i++)
            {
                uint16_t rnti = e3Control->ue[i].rnti;
                uint16_t rbgStart = e3Control->ue[i].initAllocatedRbg;
                uint16_t rbgLen = e3Control->ue[i].numAllocatedRbg;
                uint8_t mcs = e3Control->ue[i].mcs;

                uint16_t maxRbgAllocation = 0;
                auto it = m_ueUlMaxRbgAllocation.find(rnti);
                if (it != m_ueUlMaxRbgAllocation.end())
                {
                    maxRbgAllocation = it->second;
                    rbgLen = std::min(rbgLen, maxRbgAllocation);
                }
                else
                {
                    maxRbgAllocation = rbgLen;
                }

                if (rbgLen == 0)
                {
                    continue;
                }

                if (mcs > 28)
                {
                    std::cerr << "ib-ORAN LTE MAC Scheduler: dApp sent invalid MCS " << (int)mcs
                              << " for RNTI " << rnti << ": Skipping.\n";
                    continue;
                }

                if (rbgStart + rbgLen > m_cschedCellConfig.m_ulBandwidth ||
                    rbgLen > maxRbgAllocation)
                {
                    std::cerr << "ib-ORAN LTE MAC Scheduler: dApp sent out-of-bound RB allocation "
                                 "for RNTI "
                              << rnti << ": Skipping.\n";
                    continue;
                }

                if (totalRbAllocated + rbgLen >
                    (uint32_t)(m_cschedCellConfig.m_ulBandwidth - rbAllocatedNum))
                {
                    NS_LOG_WARN("Controller over-allocated UL RBs: Stopping allocations");
                    break;
                }

                // check if RBs are free
                bool free = true;
                for (uint8_t rb = rbgStart; rb < rbgStart + rbgLen; rb++)
                {
                    if (rbMap.at(rb))
                    {
                        free = false;
                        NS_LOG_INFO(this << " BUSY " << rb);
                    }
                }
                if (!free)
                {
                    NS_LOG_WARN(this << " RB allocation conflict for RNTI " << rnti
                                     << ": Skipping.\n");
                    continue;
                }

                for (uint8_t rb = rbgStart; rb < rbgStart + rbgLen; rb++)
                {
                    rbMap.at(rb) = true;
                    rbgAllocationMap.at(rb) = rnti;
                    NS_LOG_INFO("\t Allocated RB " << rb);
                }
                totalRbAllocated += rbgLen;

                UlDciListElement_s ulDci;
                ulDci.m_rnti = rnti;
                ulDci.m_rbStart = rbgStart;
                ulDci.m_rbLen = rbgLen;
                ulDci.m_mcs = mcs;
                uint32_t tbSize = m_amc->GetUlTbSizeFromMcs(mcs, rbgLen) / 8;
                ulDci.m_tbSize = tbSize;
                m_ueUlTotalBytesServed[rnti] += tbSize;
                const uint32_t bsrBefore = m_ceBsrRxed[rnti];
                m_ulTput.AddServed(rnti, nowNs, std::min(tbSize, bsrBefore));
                UpdateUlRlcBufferInfo(ulDci.m_rnti, ulDci.m_tbSize);
                ulDci.m_ndi = 1;
                ulDci.m_cceIndex = 0;
                ulDci.m_aggrLevel = 1;
                ulDci.m_ueTxAntennaSelection = 3;
                ulDci.m_hopping = false;
                ulDci.m_n2Dmrs = 0;
                ulDci.m_tpc = 0;
                ulDci.m_cqiRequest = false;
                ulDci.m_ulIndex = 0;
                ulDci.m_dai = 1;
                ulDci.m_freqHopping = 0;
                ulDci.m_pdcchPowerOffset = 0;
                if (m_harqOn)
                {
                    std::map<uint16_t, uint8_t>::iterator itProcId =
                        m_ulHarqCurrentProcessId.find(rnti);
                    if (itProcId == m_ulHarqCurrentProcessId.end())
                    {
                        NS_FATAL_ERROR("No RNTI entry in UL HARQ ID buffer for RNTI " << rnti);
                    }
                    uint8_t harqId = (*itProcId).second;
                    std::map<uint16_t, UlHarqProcessesDciBuffer_t>::iterator itDci =
                        m_ulHarqProcessesDciBuffer.find(rnti);
                    if (itDci == m_ulHarqProcessesDciBuffer.end())
                    {
                        NS_FATAL_ERROR("No RNTI entry in UL DCI HARQ buffer for RNTI " << rnti);
                    }
                    (*itDci).second.at(harqId) = ulDci;
                    std::map<uint16_t, UlHarqProcessesStatus_t>::iterator itStat =
                        m_ulHarqProcessesStatus.find(rnti);
                    if (itStat == m_ulHarqProcessesStatus.end())
                    {
                        NS_FATAL_ERROR("No RNTI entry in UL HARQ status buffer for RNTI " << rnti);
                    }
                    (*itStat).second.at(harqId) = 1;
                }
                m_ulWaiting.MarkServed(rnti, nowNs);
                ret.m_dciList.push_back(ulDci);
            }
        }
    }

    m_shmInterface->CppRecvEnd();
    m_allocationMaps.insert(
        std::pair<uint16_t, std::vector<uint16_t>>(params.m_sfnSf, rbgAllocationMap));
    m_schedSapUser->SchedUlConfigInd(ret);
}

void
IbOranLteMacScheduler::DoSchedUlNoiseInterferenceReq(
    const FfMacSchedSapProvider::SchedUlNoiseInterferenceReqParameters& params)
{
    NS_LOG_FUNCTION(this);
}

void
IbOranLteMacScheduler::DoSchedUlSrInfoReq(
    const FfMacSchedSapProvider::SchedUlSrInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);
}

void
IbOranLteMacScheduler::DoSchedUlMacCtrlInfoReq(
    const FfMacSchedSapProvider::SchedUlMacCtrlInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);

    for (unsigned int i = 0; i < params.m_macCeList.size(); i++)
    {
        if (params.m_macCeList.at(i).m_macCeType == MacCeListElement_s::BSR)
        {
            uint32_t buffer = 0;
            for (uint8_t lcg = 0; lcg < 4; ++lcg)
            {
                uint8_t bsrId = params.m_macCeList.at(i).m_macCeValue.m_bufferStatus.at(lcg);
                buffer += BufferSizeLevelBsr::BsrId2BufferSize(bsrId);
            }

            uint16_t rnti = params.m_macCeList.at(i).m_rnti;
            NS_LOG_LOGIC(this << "RNTI=" << rnti << " buffer=" << buffer);
            auto it = m_ceBsrRxed.find(rnti);
            if (it == m_ceBsrRxed.end())
            {
                m_ceBsrRxed[rnti] = buffer;
            }
            else
            {
                (*it).second = buffer;
            }
        }
    }
}

void
IbOranLteMacScheduler::DoSchedUlCqiInfoReq(
    const FfMacSchedSapProvider::SchedUlCqiInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    m_ffrSapProvider->ReportUlCqiInfo(params);

    switch (m_ulCqiFilter)
    {
    case FfMacScheduler::SRS_UL_CQI: {
        if (params.m_ulCqi.m_type != UlCqi_s::SRS)
        {
            return;
        }
    }
    break;
    case FfMacScheduler::PUSCH_UL_CQI: {
        if (params.m_ulCqi.m_type != UlCqi_s::PUSCH)
        {
            return;
        }
    }
    break;

    default:
        NS_FATAL_ERROR("Unknown UL CQI type");
    }

    switch (params.m_ulCqi.m_type)
    {
    case UlCqi_s::PUSCH: {
        NS_LOG_DEBUG(this << " Collect PUSCH CQIs of Frame no. " << (params.m_sfnSf >> 4)
                          << " subframe no. " << (0xF & params.m_sfnSf));
        auto itMap = m_allocationMaps.find(params.m_sfnSf);
        if (itMap == m_allocationMaps.end())
        {
            return;
        }
        for (uint32_t i = 0; i < (*itMap).second.size(); i++)
        {
            double sinr = LteFfConverter::fpS11dot3toDouble(params.m_ulCqi.m_sinr.at(i));
            auto itCqi = m_ueCqi.find((*itMap).second.at(i));
            if (itCqi == m_ueCqi.end())
            {
                std::vector<double> newCqi;
                for (uint32_t j = 0; j < m_cschedCellConfig.m_ulBandwidth; j++)
                {
                    if (i == j)
                    {
                        newCqi.push_back(sinr);
                    }
                    else
                    {
                        newCqi.push_back(NO_SINR);
                    }
                }
                m_ueCqi[(*itMap).second.at(i)] = newCqi;
                m_ueCqiTimers[(*itMap).second.at(i)] = m_cqiTimersThreshold;
            }
            else
            {
                (*itCqi).second.at(i) = sinr;
                NS_LOG_DEBUG(this << " RNTI " << (*itMap).second.at(i) << " RB " << i << " SINR "
                                  << sinr);
                auto itTimers = m_ueCqiTimers.find((*itMap).second.at(i));
                (*itTimers).second = m_cqiTimersThreshold;
            }
        }
        m_allocationMaps.erase(itMap);
    }
    break;
    case UlCqi_s::SRS: {
        NS_LOG_DEBUG(this << " Collect SRS CQIs of Frame no. " << (params.m_sfnSf >> 4)
                          << " subframe no. " << (0xF & params.m_sfnSf));
        uint16_t rnti = 0;
        NS_ASSERT(!params.m_vendorSpecificList.empty());
        for (std::size_t i = 0; i < params.m_vendorSpecificList.size(); i++)
        {
            if (params.m_vendorSpecificList.at(i).m_type == SRS_CQI_RNTI_VSP)
            {
                Ptr<SrsCqiRntiVsp> vsp =
                    DynamicCast<SrsCqiRntiVsp>(params.m_vendorSpecificList.at(i).m_value);
                rnti = vsp->GetRnti();
            }
        }
        auto itCqi = m_ueCqi.find(rnti);
        if (itCqi == m_ueCqi.end())
        {
            std::vector<double> newCqi;
            for (uint32_t j = 0; j < m_cschedCellConfig.m_ulBandwidth; j++)
            {
                double sinr = LteFfConverter::fpS11dot3toDouble(params.m_ulCqi.m_sinr.at(j));
                newCqi.push_back(sinr);
                NS_LOG_INFO(this << " RNTI " << rnti << " new SRS-CQI for RB  " << j << " value "
                                 << sinr);
            }
            m_ueCqi[rnti] = newCqi;
            m_ueCqiTimers[rnti] = m_cqiTimersThreshold;
        }
        else
        {
            for (uint32_t j = 0; j < m_cschedCellConfig.m_ulBandwidth; j++)
            {
                double sinr = LteFfConverter::fpS11dot3toDouble(params.m_ulCqi.m_sinr.at(j));
                (*itCqi).second.at(j) = sinr;
                NS_LOG_INFO(this << " RNTI " << rnti << " update SRS-CQI for RB  " << j << " value "
                                 << sinr);
            }
            auto itTimers = m_ueCqiTimers.find(rnti);
            (*itTimers).second = m_cqiTimersThreshold;
        }
    }
    break;
    case UlCqi_s::PUCCH_1:
    case UlCqi_s::PUCCH_2:
    case UlCqi_s::PRACH: {
        NS_FATAL_ERROR("IbOranLteMacScheduler supports only PUSCH and SRS UL-CQIs");
    }
    break;
    default:
        NS_FATAL_ERROR("Unknown type of UL-CQI");
    }
}

void
IbOranLteMacScheduler::RefreshDlCqiMaps()
{
    auto itP10 = m_p10CqiTimers.begin();
    while (itP10 != m_p10CqiTimers.end())
    {
        NS_LOG_INFO(this << " P10-CQI for user " << (*itP10).first << " is "
                         << (uint32_t)(*itP10).second << " thr " << (uint32_t)m_cqiTimersThreshold);
        if ((*itP10).second == 0)
        {
            auto itMap = m_p10CqiRxed.find((*itP10).first);
            NS_ASSERT_MSG(itMap != m_p10CqiRxed.end(),
                          " Does not find CQI report for user " << (*itP10).first);
            NS_LOG_INFO(this << " P10-CQI expired for user " << (*itP10).first);
            m_p10CqiRxed.erase(itMap);
            auto temp = itP10;
            itP10++;
            m_p10CqiTimers.erase(temp);
        }
        else
        {
            (*itP10).second--;
            itP10++;
        }
    }

    auto itA30 = m_a30CqiTimers.begin();
    while (itA30 != m_a30CqiTimers.end())
    {
        NS_LOG_INFO(this << " A30-CQI for user " << (*itA30).first << " is "
                         << (uint32_t)(*itA30).second << " thr " << (uint32_t)m_cqiTimersThreshold);
        if ((*itA30).second == 0)
        {
            auto itMap = m_a30CqiRxed.find((*itA30).first);
            NS_ASSERT_MSG(itMap != m_a30CqiRxed.end(),
                          " Does not find CQI report for user " << (*itA30).first);
            NS_LOG_INFO(this << " A30-CQI expired for user " << (*itA30).first);
            m_a30CqiRxed.erase(itMap);
            auto temp = itA30;
            itA30++;
            m_a30CqiTimers.erase(temp);
        }
        else
        {
            (*itA30).second--;
            itA30++;
        }
    }
}

void
IbOranLteMacScheduler::RefreshUlCqiMaps()
{
    auto itUl = m_ueCqiTimers.begin();
    while (itUl != m_ueCqiTimers.end())
    {
        NS_LOG_INFO(this << " UL-CQI for user " << (*itUl).first << " is "
                         << (uint32_t)(*itUl).second << " thr " << (uint32_t)m_cqiTimersThreshold);
        if ((*itUl).second == 0)
        {
            auto itMap = m_ueCqi.find((*itUl).first);
            NS_ASSERT_MSG(itMap != m_ueCqi.end(),
                          " Does not find CQI report for user " << (*itUl).first);
            NS_LOG_INFO(this << " UL-CQI exired for user " << (*itUl).first);
            (*itMap).second.clear();
            m_ueCqi.erase(itMap);
            auto temp = itUl;
            itUl++;
            m_ueCqiTimers.erase(temp);
        }
        else
        {
            (*itUl).second--;
            itUl++;
        }
    }
}

void
IbOranLteMacScheduler::UpdateDlRlcBufferInfo(uint16_t rnti, uint8_t lcid, uint16_t size)
{
    LteFlowId_t flow(rnti, lcid);
    auto it = m_rlcBufferReq.find(flow);
    if (it != m_rlcBufferReq.end())
    {
        NS_LOG_INFO(this << " UE " << rnti << " LC " << (uint16_t)lcid << " txqueue "
                         << (*it).second.m_rlcTransmissionQueueSize << " retxqueue "
                         << (*it).second.m_rlcRetransmissionQueueSize << " status "
                         << (*it).second.m_rlcStatusPduSize << " decrease " << size);
        if (((*it).second.m_rlcStatusPduSize > 0) && (size >= (*it).second.m_rlcStatusPduSize))
        {
            (*it).second.m_rlcStatusPduSize = 0;
        }
        else if (((*it).second.m_rlcRetransmissionQueueSize > 0) &&
                 (size >= (*it).second.m_rlcRetransmissionQueueSize))
        {
            (*it).second.m_rlcRetransmissionQueueSize = 0;
        }
        else if ((*it).second.m_rlcTransmissionQueueSize > 0)
        {
            uint32_t rlcOverhead;
            if (lcid == 1)
            {
                rlcOverhead = 4;
            }
            else
            {
                rlcOverhead = 2;
            }
            if ((*it).second.m_rlcTransmissionQueueSize <= size - rlcOverhead)
            {
                (*it).second.m_rlcTransmissionQueueSize = 0;
            }
            else
            {
                (*it).second.m_rlcTransmissionQueueSize -= size - rlcOverhead;
            }
        }
    }
    else
    {
        NS_LOG_ERROR(this << " Does not find DL RLC Buffer Report of UE " << rnti);
    }
}

void
IbOranLteMacScheduler::UpdateUlRlcBufferInfo(uint16_t rnti, uint16_t size)
{
    size = size - 2;
    auto it = m_ceBsrRxed.find(rnti);
    if (it != m_ceBsrRxed.end())
    {
        NS_LOG_INFO(this << " UE " << rnti << " size " << size << " BSR " << (*it).second);
        if ((*it).second >= size)
        {
            (*it).second -= size;
        }
        else
        {
            (*it).second = 0;
        }
    }
    else
    {
        NS_LOG_ERROR(this << " Does not find BSR report info of UE " << rnti);
    }
}

void
IbOranLteMacScheduler::TransmissionModeConfigurationUpdate(uint16_t rnti, uint8_t txMode)
{
    NS_LOG_FUNCTION(this << " RNTI " << rnti << " txMode " << (uint16_t)txMode);
    FfMacCschedSapUser::CschedUeConfigUpdateIndParameters params{};
    params.m_rnti = rnti;
    params.m_transmissionMode = txMode;
    m_cschedSapUser->CschedUeConfigUpdateInd(params);
}

} // namespace ns3
