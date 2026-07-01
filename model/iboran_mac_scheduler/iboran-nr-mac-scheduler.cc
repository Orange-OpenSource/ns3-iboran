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

#include "iboran-nr-mac-scheduler.h"

#include "../iboran_application/iboran-side-info-tracker.h"

#include <ns3/log.h>
#include <ns3/simulator.h>

#include <algorithm>
#include <cfloat>
#include <climits>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IbOranNrMacScheduler");
NS_OBJECT_ENSURE_REGISTERED(IbOranNrMacScheduler);

TypeId
IbOranNrMacScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::IbOranNrMacScheduler")
                            .SetParent<NrMacSchedulerOfdma>()
                            .SetGroupName("Nr")
                            .AddConstructor<IbOranNrMacScheduler>()
                            .AddAttribute("Numerology",
                                          "NR numerology index (0-6)",
                                          UintegerValue(1),
                                          MakeUintegerAccessor(&IbOranNrMacScheduler::m_numerology),
                                          MakeUintegerChecker<uint32_t>(0, 6));
    return tid;
}

IbOranNrMacScheduler::IbOranNrMacScheduler()
{
    NS_LOG_FUNCTION(this);

    auto* shmInterface = Ns3AiMsgInterface::Get();
    shmInterface->SetIsMemoryCreator(false);
    shmInterface->SetUseVector(false);
    shmInterface->SetHandleFinish(true);
    shmInterface->SetNames("iboranSegment", "iboranCpp2Py", "iboranPy2Cpp", "iboran");
    m_shmInterface = shmInterface->GetInterface<Cpp2Py, Py2Cpp>();
}

IbOranNrMacScheduler::~IbOranNrMacScheduler()
{
    NS_LOG_FUNCTION(this);
    m_shmInterface = nullptr;
}

std::shared_ptr<NrMacSchedulerUeInfo>
IbOranNrMacScheduler::CreateUeRepresentation(
    const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const
{
    NS_LOG_FUNCTION(this);
    return std::make_shared<NrMacSchedulerUeInfo>(
        params.m_rnti,
        params.m_beamId,
        std::bind(&NrMacSchedulerNs3::GetNumRbPerRbg, this));
}

std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq&,
                   const NrMacSchedulerNs3::UePtrAndBufferReq&)>
IbOranNrMacScheduler::GetUeCompareDlFn() const
{
    // All UEs treated as equal; order determined by controller, not this fn.
    return [](const UePtrAndBufferReq& /*a*/, const UePtrAndBufferReq& /*b*/) { return false; };
}

std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq&,
                   const NrMacSchedulerNs3::UePtrAndBufferReq&)>
IbOranNrMacScheduler::GetUeCompareUlFn() const
{
    return [](const UePtrAndBufferReq& /*a*/, const UePtrAndBufferReq& /*b*/) { return false; };
}

void
IbOranNrMacScheduler::AssignedDlResources(const UePtrAndBufferReq& /*ue*/,
                                          const FTResources& /*assigned*/,
                                          const FTResources& /*totalAssigned*/) const
{
}

void
IbOranNrMacScheduler::AssignedUlResources(const UePtrAndBufferReq& /*ue*/,
                                          const FTResources& /*assigned*/,
                                          const FTResources& /*totalAssigned*/) const
{
}

void
IbOranNrMacScheduler::NotAssignedDlResources(const UePtrAndBufferReq& /*ue*/,
                                             const FTResources& /*notAssigned*/,
                                             const FTResources& /*totalAssigned*/) const
{
}

void
IbOranNrMacScheduler::NotAssignedUlResources(const UePtrAndBufferReq& /*ue*/,
                                             const FTResources& /*notAssigned*/,
                                             const FTResources& /*totalAssigned*/) const
{
}

void
IbOranNrMacScheduler::BeforeDlSched(const UePtrAndBufferReq& /*ue*/,
                                    const FTResources& /*assignableInIteration*/) const
{
}

void
IbOranNrMacScheduler::BeforeUlSched(const UePtrAndBufferReq& /*ue*/,
                                    const FTResources& /*assignableInIteration*/) const
{
}

uint8_t
IbOranNrMacScheduler::GetCqiFromSinrLinear(double sinrLinear) const
{
    const double sinrDb = (sinrLinear > 0.0) ? 10.0 * std::log10(sinrLinear) : -100.0;

    // 3GPP TS 36.213 Table 7.2.3-1: lower SINR bound for each CQI index.
    static constexpr double kSinrTable[16] = {
        -1e9, // CQI  0: sentinel — never selected
        -6.7, // CQI  1
        -4.7, // CQI  2
        -2.3, // CQI  3
        0.2,  // CQI  4
        2.4,  // CQI  5
        4.3,  // CQI  6
        5.9,  // CQI  7
        8.1,  // CQI  8
        10.3, // CQI  9
        11.7, // CQI 10
        14.1, // CQI 11
        16.3, // CQI 12
        18.7, // CQI 13
        21.0, // CQI 14
        22.7, // CQI 15
    };

    uint8_t cqi = 0;
    for (uint8_t i = 1; i <= 15; ++i)
    {
        if (sinrDb >= kSinrTable[i])
        {
            cqi = i;
        }
        else
        {
            break;
        }
    }
    return std::min(static_cast<uint8_t>(15), std::max(static_cast<uint8_t>(1), cqi));
}

uint8_t
IbOranNrMacScheduler::GetHighestPriorityQci(const std::unordered_map<uint8_t, LCGPtr>& lcgMap) const
{
    // GBR QCIs 1–4 outrank non-GBR 5–9.
    // Within the same class, the lowest index has highest 3GPP priority.
    uint8_t best = 9; // default: best-effort non-GBR

    for (const auto& lcgPair : lcgMap)
    {
        // Retrieve every LC ID in this LCG, then query its QCI.
        const auto lcIds = lcgPair.second->GetLCId();
        for (const uint8_t lcId : lcIds)
        {
            const uint8_t qci = lcgPair.second->GetFiveQi(lcId);

            // GBR bearer found — always beats non-GBR
            if (qci >= 1 && qci <= 4)
            {
                if (best > 4 || qci < best)
                {
                    best = qci;
                }
            }
            // Non-GBR: only update if we haven't found a GBR yet
            else if (best > 4 && qci < best)
            {
                best = qci;
            }
        }
    }
    return best;
}

void
IbOranNrMacScheduler::FillUeIndication(const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                                       uint32_t totalBuffer,
                                       uint32_t ueIdx,
                                       LinkDirection direction,
                                       bool isUl,
                                       E3Indication* e3Indication,
                                       E2KpmReport* e2KpmReport) const
{
    const uint16_t rnti = ueInfo->m_rnti;

    // Handle channel quality
    uint8_t rntiCqi{1};
    uint8_t rntiMcs{0};

    if (isUl)
    {
        double avgSinrLinear = 0.0;
        if (!ueInfo->m_ulCqi.m_sinr.empty())
        {
            for (const double s : ueInfo->m_ulCqi.m_sinr)
            {
                avgSinrLinear += s;
            }
            avgSinrLinear /= static_cast<double>(ueInfo->m_ulCqi.m_sinr.size());
        }
        rntiCqi = GetCqiFromSinrLinear(avgSinrLinear);
        rntiMcs = ueInfo->m_ulMcs;
    }
    else
    {
        rntiCqi = static_cast<uint8_t>(ueInfo->m_dlCqi.m_wbCqi);
        if (rntiCqi == 0)
        {
            rntiCqi = 1; // clamp: CQI 0 means "no report", use minimum
        }
        rntiMcs = ueInfo->m_dlMcs;
    }

    // Derive throughput metrics
    double rntiAchievableRate = 0.0;
    double rntiActualThroughput = 0.0;
    {
        const auto& allocMap = isUl ? m_ueUlAllocatedRbg : m_ueDlAllocatedRbg;
        auto it = allocMap.find(rnti);
        if (it != allocMap.end() && it->second > 0)
        {
            const uint32_t numPrb = static_cast<uint32_t>(it->second) * GetNumRbPerRbg();

            if (isUl)
            {
                rntiAchievableRate = (m_ulAmc->GetSpectralEfficiencyForCqi(rntiCqi) * 180e3 *
                                      static_cast<double>(numPrb)) /
                                     1e6;
            }
            else
            {
                rntiAchievableRate = (m_dlAmc->GetSpectralEfficiencyForCqi(rntiCqi) * 180e3 *
                                      static_cast<double>(numPrb)) /
                                     1e6;
            }

            const Ptr<NrAmc>& amc = isUl ? m_ulAmc : m_dlAmc;
            const double tbBits = static_cast<double>(amc->CalculateTbSize(rntiMcs, 1, numPrb) * 8);
            rntiActualThroughput = std::min(rntiAchievableRate, (tbBits / 1e-3) / 1e6);
        }
    }

    // Derive packet-delay proxy (buffer / achievable-rate)
    uint32_t rntiPacketDelay = 0;
    if (totalBuffer > 0 && rntiAchievableRate > 0.0)
    {
        rntiPacketDelay = static_cast<uint32_t>(static_cast<double>(totalBuffer) * 8.0 /
                                                (rntiAchievableRate * 1e6) * 1000.0);
    }

    // Compute cumulative bytes served
    uint32_t rntiTotalBytesServed = 0;
    {
        const auto& servedMap = isUl ? m_ueUlTotalBytesServed : m_ueDlTotalBytesServed;
        auto it = servedMap.find(rnti);
        if (it != servedMap.end())
        {
            rntiTotalBytesServed = it->second;
        }
    }

    // Get application-layer side info
    const uint8_t rntiCriticality =
        SideInfoTracker::GetInstance().GetRntiCriticality(rnti, direction);
    const std::string rntiContentId =
        SideInfoTracker::GetInstance().GetRntiObjectId(rnti, direction);

    const auto& lcgMap = isUl ? ueInfo->m_ulLCG : ueInfo->m_dlLCG;
    const uint8_t rntiQci = GetHighestPriorityQci(lcgMap);

    e3Indication->ue[ueIdx].rnti = rnti;
    e3Indication->ue[ueIdx].cqi = rntiCqi;
    e3Indication->ue[ueIdx].bufferLevel = totalBuffer;
    e3Indication->ue[ueIdx].contentCriticality = rntiCriticality;
    std::snprintf(e3Indication->ue[ueIdx].contentId, MAX_ID_LEN, "%s", rntiContentId.c_str());
    e3Indication->ue[ueIdx].qci = rntiQci;
    e3Indication->ue[ueIdx].achievableRate = rntiAchievableRate;
    e3Indication->ue[ueIdx].actualThroughput = rntiActualThroughput;
    e3Indication->ue[ueIdx].totalBytesServed = rntiTotalBytesServed;
    e3Indication->ue[ueIdx].packetDelay = rntiPacketDelay;

    e2KpmReport->ue[ueIdx].rnti = rnti;
    e2KpmReport->ue[ueIdx].cqi = rntiCqi;
    e2KpmReport->ue[ueIdx].bufferLevel = totalBuffer;
    e2KpmReport->ue[ueIdx].qci = rntiQci;
    e2KpmReport->ue[ueIdx].achievableRate = rntiAchievableRate;
    e2KpmReport->ue[ueIdx].actualThroughput = rntiActualThroughput;
    e2KpmReport->ue[ueIdx].totalBytesServed = rntiTotalBytesServed;
    e2KpmReport->ue[ueIdx].packetDelay = rntiPacketDelay;
}

IbOranNrMacScheduler::SlotInfo
IbOranNrMacScheduler::GetCurrentSlotInfo() const
{
    const uint32_t numerology = m_numerology; // inherited from NrMacSchedulerNs3
    const uint64_t nowNs = Simulator::Now().GetNanoSeconds();

    const uint64_t slotDurationNs = 1'000'000ULL >> numerology;    // 1ms / 2^µ in ns
    const uint64_t slotsPerFrame = 10'000'000ULL / slotDurationNs; // 10ms / slotDuration
    const uint64_t slotsPerSf = 1'000'000ULL / slotDurationNs;     // 1ms  / slotDuration

    const uint64_t totalSlots = nowNs / slotDurationNs;

    SlotInfo info;
    info.frameNo = static_cast<uint32_t>(totalSlots / slotsPerFrame % 1024); // wraps at 1024
    info.subframeNo = static_cast<uint8_t>((totalSlots % slotsPerFrame) / slotsPerSf);
    info.slotNo = static_cast<uint8_t>(totalSlots % slotsPerSf);
    return info;
}

NrMacSchedulerNs3::BeamSymbolMap
IbOranNrMacScheduler::AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const
{
    NS_LOG_FUNCTION(this << symAvail);

    BeamSymbolMap ret;
    m_pendingDlAlloc.clear();

    const uint16_t bandwidthInRbg = GetBandwidthInRbg();

    // All beams share the full symbol budget in OFDMA.
    for (const auto& beam : activeDl)
    {
        ret[beam.first] = symAvail;
    }

    // RBG occupancy map — all free at this point.
    std::vector<bool> rbgMap(bandwidthInRbg, false);
    const uint16_t numFreeRbg =
        std::min(static_cast<uint16_t>(std::count(rbgMap.begin(), rbgMap.end(), false)),
                 static_cast<uint16_t>(MAX_PRB_NUM_NR / GetNumRbPerRbg()));
    uint16_t initFreeRbg = 0;
    for (uint16_t r = 0; r < bandwidthInRbg; ++r)
    {
        if (!rbgMap[r])
        {
            initFreeRbg = r;
            break;
        }
    }

    // -- SHARED-MEMORY INTERFACE (CPP2PY) ---------------------------
    m_shmInterface->CppSendBegin();
    Cpp2Py* cpp2Py = m_shmInterface->GetCpp2PyStruct();
    E3Indication* e3Indication = &cpp2Py->e3Indication;
    E2KpmReport* e2KpmReport = &cpp2Py->e2KpmReport;

    uint32_t ueIdx = 0;
    uint32_t totalBuffer = 0;

    for (const auto& beam : activeDl)
    {
        for (const auto& uePair : beam.second)
        {
            if (ueIdx >= MAX_UE_NUM)
            {
                break;
            }
            FillUeIndication(uePair.first,
                             uePair.second,
                             ueIdx,
                             LinkDirection::DOWNLINK,
                             /*isUl=*/false,
                             e3Indication,
                             e2KpmReport);
            totalBuffer += uePair.second;
            ++ueIdx;
        }
        if (ueIdx >= MAX_UE_NUM)
        {
            break;
        }
    }

    if (ueIdx == 0)
    {
        // No active UEs — still need to complete the shm round-trip.
        m_shmInterface->CppSendEnd();
        m_shmInterface->CppRecvBegin();
        m_shmInterface->CppRecvEnd();
        return ret;
    }

    const uint64_t timestamp = Simulator::Now().GetNanoSeconds();
    const auto slotInfo = GetCurrentSlotInfo();
    ++m_dlE3IndicationId;
    ++m_dlE2KpmReportId;

    e3Indication->type = E3SM_INDICATION;
    e3Indication->timestamp = timestamp;
    e3Indication->subframeNo = slotInfo.subframeNo;
    e3Indication->frameNo = slotInfo.frameNo;
    e3Indication->numUe = ueIdx;
    e3Indication->totalBufferLevel = totalBuffer;
    e3Indication->initRbg = initFreeRbg;
    e3Indication->numRbg = numFreeRbg;
    e3Indication->rbgSize = static_cast<uint8_t>(GetNumRbPerRbg());
    e3Indication->linkDirection = DOWNLINK;
    e3Indication->id = m_dlE3IndicationId;

    e2KpmReport->type = E2SM_KPM_REPORT;
    e2KpmReport->timestamp = timestamp;
    e2KpmReport->frameNo = slotInfo.frameNo;
    e2KpmReport->numUe = ueIdx;
    e2KpmReport->totalBufferLevel = totalBuffer;
    e2KpmReport->numRbg = numFreeRbg;
    e2KpmReport->rbgSize = static_cast<uint8_t>(GetNumRbPerRbg());
    e2KpmReport->linkDirection = DOWNLINK;
    e2KpmReport->id = m_dlE2KpmReportId;

    m_shmInterface->CppSendEnd();

    // -- SHARED-MEMORY INTERFACE (PY2CPP) ---------------------------
    m_shmInterface->CppRecvBegin();
    Py2Cpp* py2Cpp = m_shmInterface->GetPy2CppStruct();
    E3Control* e3Control = &py2Cpp->e3Control;
    E2RcControl* e2RcControl = &py2Cpp->e2RcControl;

    auto earlyExit = [&]() -> BeamSymbolMap {
        m_shmInterface->CppRecvEnd();
        return ret;
    };

    if (e3Control->type != E3SM_CONTROL)
    {
        std::cerr << "ib-ORAN NR MAC Scheduler: dApp invalid message type: Skipping.\n";
        return earlyExit();
    }
    if (e2RcControl->type != E2SM_RC_CONTROL)
    {
        std::cerr << "ib-ORAN NR MAC Scheduler: xApp invalid message type: Skipping.\n";
        return earlyExit();
    }
    if (e3Control->linkDirection != DOWNLINK)
    {
        std::cerr << "ib-ORAN NR MAC Scheduler: dApp wrong direction: Skipping.\n";
        return earlyExit();
    }
    if (e2RcControl->linkDirection != DOWNLINK)
    {
        std::cerr << "ib-ORAN NR MAC Scheduler: xApp wrong direction: Skipping.\n";
        return earlyExit();
    }

    const uint32_t maxUeSchedSize = e2RcControl->maxUeSchedSize;
    uint32_t ueSchedSize = std::min(e3Control->ueSchedSize, maxUeSchedSize);
    if (e3Control->skipThisStep || e2RcControl->skipThisStep || ueSchedSize == 0)
    {
        m_shmInterface->CppRecvEnd();
        return ret;
    }

    // Absorb per-UE maximum RBG cap from xApp.
    for (uint32_t i = 0; i < e2RcControl->maxUeSchedSize; ++i)
    {
        m_ueDlMaxRbgAllocation[e2RcControl->ue[i].rnti] = e2RcControl->ue[i].maxRbgAllocation;
    }

    const std::vector<bool> dlBitmask = GetDlBitmask();

    for (uint32_t i = 0; i < ueSchedSize; ++i)
    {
        const uint16_t rnti = e3Control->ue[i].rnti;
        const uint16_t rbgStart = e3Control->ue[i].initAllocatedRbg;
        const uint8_t mcs = e3Control->ue[i].mcs;

        if (mcs > 28)
        {
            std::cerr << "ib-ORAN NR MAC Scheduler: invalid MCS " << +mcs << " RNTI=" << rnti
                      << ": Skipping.\n";
            continue;
        }

        auto itMax = m_ueDlMaxRbgAllocation.find(rnti);
        if (itMax == m_ueDlMaxRbgAllocation.end())
        {
            std::cerr << "ib-ORAN NR MAC Scheduler: RNTI=" << rnti
                      << " missing xApp cap: Skipping.\n";
            continue;
        }
        const uint16_t maxRbg = itMax->second;
        const uint16_t rbgLen =
            std::min(static_cast<uint16_t>(e3Control->ue[i].numAllocatedRbg), maxRbg);

        if (rbgLen == 0 || rbgStart + rbgLen > bandwidthInRbg || rbgLen > maxRbg)
        {
            std::cerr << "ib-ORAN NR MAC Scheduler: bad RBG range rbgStart=" << rbgStart
                      << " rbgLen=" << rbgLen << " RNTI=" << rnti << ": Skipping.\n";
            continue;
        }

        // Conflict / notch check.
        bool skip = false;
        for (uint16_t r = rbgStart; r < rbgStart + rbgLen; ++r)
        {
            if (rbgMap[r])
            {
                NS_LOG_ERROR("DL: RBG " << r << " already occupied RNTI=" << rnti);
                skip = true;
                break;
            }
            if (!dlBitmask[r])
            {
                NS_LOG_WARN("DL: RBG " << r << " notched RNTI=" << rnti);
                skip = true;
                break;
            }
        }
        if (skip)
        {
            continue;
        }

        // Mark RBGs occupied.
        for (uint16_t r = rbgStart; r < rbgStart + rbgLen; ++r)
        {
            rbgMap[r] = true;
        }

        // Cache for CreateDlDci.
        PendingAlloc pa;
        pa.rbgStart = rbgStart;
        pa.rbgLen = rbgLen;
        pa.mcs = mcs;
        pa.symAvail = symAvail;
        pa.bandwidthInRbg = bandwidthInRbg;
        m_pendingDlAlloc[rnti] = pa;

        // Set m_dlRBG and m_dlSym on the UeInfo so the base-class
        // DoScheduleDlData loop calls CreateDlDci for this UE.
        for (const auto& beam : activeDl)
        {
            for (const auto& uePair : beam.second)
            {
                if (uePair.first->m_rnti == rnti)
                {
                    auto& ueInfo = uePair.first;
                    ueInfo->m_dlRBG.clear();
                    ueInfo->m_dlSym.clear();
                    for (uint16_t r = rbgStart; r < rbgStart + rbgLen; ++r)
                    {
                        ueInfo->m_dlRBG.push_back(r);
                    }
                    for (uint32_t s = 0; s < symAvail; ++s)
                    {
                        ueInfo->m_dlSym.push_back(static_cast<uint8_t>(s));
                    }
                    break;
                }
            }
        }

        NS_LOG_DEBUG("DL AssignRBG RNTI=" << rnti << " rbgStart=" << rbgStart << " rbgLen="
                                          << rbgLen << " mcs=" << +mcs << " symAvail=" << symAvail);
    }

    m_shmInterface->CppRecvEnd();
    return ret;
}

NrMacSchedulerNs3::BeamSymbolMap
IbOranNrMacScheduler::AssignULRBG(uint32_t symAvail, const ActiveUeMap& activeUl) const
{
    NS_LOG_FUNCTION(this << symAvail);

    BeamSymbolMap ret;
    m_pendingUlAlloc.clear();

    const uint16_t bandwidthInRbg = GetBandwidthInRbg();

    for (const auto& beam : activeUl)
    {
        ret[beam.first] = symAvail;
    }

    std::vector<bool> ulRbgMap(bandwidthInRbg, false);
    const uint16_t numFreeRbg =
        std::min(static_cast<uint16_t>(std::count(ulRbgMap.begin(), ulRbgMap.end(), false)),
                 static_cast<uint16_t>(MAX_PRB_NUM_NR / GetNumRbPerRbg()));
    uint16_t initFreeRbg = 0;
    for (uint16_t r = 0; r < bandwidthInRbg; ++r)
    {
        if (!ulRbgMap[r])
        {
            initFreeRbg = r;
            break;
        }
    }

    // -- SHARED-MEMORY INTERFACE (CPP2PY) ---------------------------
    m_shmInterface->CppSendBegin();
    Cpp2Py* cpp2Py = m_shmInterface->GetCpp2PyStruct();
    E3Indication* e3Indication = &cpp2Py->e3Indication;
    E2KpmReport* e2KpmReport = &cpp2Py->e2KpmReport;

    uint32_t ueIdx = 0;
    uint32_t totalBuffer = 0;

    for (const auto& beam : activeUl)
    {
        for (const auto& uePair : beam.second)
        {
            if (ueIdx >= MAX_UE_NUM)
            {
                break;
            }
            FillUeIndication(uePair.first,
                             uePair.second,
                             ueIdx,
                             LinkDirection::UPLINK,
                             /*isUl=*/true,
                             e3Indication,
                             e2KpmReport);
            totalBuffer += uePair.second;
            ++ueIdx;
        }
        if (ueIdx >= MAX_UE_NUM)
        {
            break;
        }
    }

    if (ueIdx == 0)
    {
        m_shmInterface->CppSendEnd();
        m_shmInterface->CppRecvBegin();
        m_shmInterface->CppRecvEnd();
        return ret;
    }

    const uint64_t timestamp = Simulator::Now().GetNanoSeconds();
    const auto slotInfo = GetCurrentSlotInfo();
    ++m_ulE3IndicationId;
    ++m_ulE2KpmReportId;

    e3Indication->type = E3SM_INDICATION;
    e3Indication->timestamp = timestamp;
    e3Indication->subframeNo = slotInfo.subframeNo;
    e3Indication->frameNo = slotInfo.frameNo;
    e3Indication->numUe = ueIdx;
    e3Indication->totalBufferLevel = totalBuffer;
    e3Indication->initRbg = initFreeRbg;
    e3Indication->numRbg = numFreeRbg;
    e3Indication->rbgSize = static_cast<uint8_t>(GetNumRbPerRbg());
    e3Indication->linkDirection = UPLINK;
    e3Indication->id = m_ulE3IndicationId;

    e2KpmReport->type = E2SM_KPM_REPORT;
    e2KpmReport->timestamp = timestamp;
    e2KpmReport->frameNo = slotInfo.frameNo;
    e2KpmReport->numUe = ueIdx;
    e2KpmReport->totalBufferLevel = totalBuffer;
    e2KpmReport->numRbg = numFreeRbg;
    e2KpmReport->rbgSize = static_cast<uint8_t>(GetNumRbPerRbg());
    e2KpmReport->linkDirection = UPLINK;
    e2KpmReport->id = m_ulE2KpmReportId;

    m_shmInterface->CppSendEnd();

    // -- SHARED-MEMORY INTERFACE (PY2CPP) ---------------------------
    m_shmInterface->CppRecvBegin();
    Py2Cpp* py2Cpp = m_shmInterface->GetPy2CppStruct();
    E3Control* e3Control = &py2Cpp->e3Control;
    E2RcControl* e2RcControl = &py2Cpp->e2RcControl;

    auto earlyExit = [&]() -> BeamSymbolMap {
        m_shmInterface->CppRecvEnd();
        return ret;
    };

    if (e3Control->type != E3SM_CONTROL)
    {
        std::cerr << "ib-ORAN NR MAC Scheduler: dApp invalid message type: Skipping.\n";
        return earlyExit();
    }
    if (e2RcControl->type != E2SM_RC_CONTROL)
    {
        std::cerr << "ib-ORAN NR MAC Scheduler: xApp invalid message type: Skipping.\n";
        return earlyExit();
    }
    if (e3Control->linkDirection != UPLINK)
    {
        std::cerr << "ib-ORAN NR MAC Scheduler: dApp wrong direction: Skipping.\n";
        return earlyExit();
    }
    if (e2RcControl->linkDirection != UPLINK)
    {
        std::cerr << "ib-ORAN NR MAC Scheduler: xApp wrong direction: Skipping.\n";
        return earlyExit();
    }

    const uint32_t maxUeSchedSize = e2RcControl->maxUeSchedSize;
    uint32_t ueSchedSize = std::min(e3Control->ueSchedSize, maxUeSchedSize);
    if (e3Control->skipThisStep || e2RcControl->skipThisStep || ueSchedSize == 0)
    {
        m_shmInterface->CppRecvEnd();
        return ret;
    }

    for (uint32_t i = 0; i < e2RcControl->maxUeSchedSize; ++i)
    {
        m_ueUlMaxRbgAllocation[e2RcControl->ue[i].rnti] = e2RcControl->ue[i].maxRbgAllocation;
    }

    const std::vector<bool> ulBitmask = GetUlBitmask();

    for (uint32_t i = 0; i < ueSchedSize; ++i)
    {
        const uint16_t rnti = e3Control->ue[i].rnti;
        const uint16_t rbgStart = e3Control->ue[i].initAllocatedRbg;
        const uint8_t mcs = e3Control->ue[i].mcs;

        if (mcs > 28)
        {
            std::cerr << "ib-ORAN NR MAC Scheduler: invalid MCS " << +mcs << " RNTI=" << rnti
                      << ": Skipping.\n";
            continue;
        }

        auto itMax = m_ueUlMaxRbgAllocation.find(rnti);
        if (itMax == m_ueUlMaxRbgAllocation.end())
        {
            std::cerr << "ib-ORAN NR MAC Scheduler: RNTI=" << rnti
                      << " missing xApp cap: Skipping.\n";
            continue;
        }
        const uint16_t maxRbg = itMax->second;
        const uint16_t rbgLen =
            std::min(static_cast<uint16_t>(e3Control->ue[i].numAllocatedRbg), maxRbg);

        if (rbgLen == 0 || rbgStart + rbgLen > bandwidthInRbg || rbgLen > maxRbg)
        {
            std::cerr << "ib-ORAN NR MAC Scheduler: bad RBG range rbgStart=" << rbgStart
                      << " rbgLen=" << rbgLen << " RNTI=" << rnti << ": Skipping.\n";
            continue;
        }

        bool skip = false;
        for (uint16_t r = rbgStart; r < rbgStart + rbgLen; ++r)
        {
            if (ulRbgMap[r])
            {
                NS_LOG_ERROR("UL: RBG " << r << " already occupied RNTI=" << rnti);
                skip = true;
                break;
            }
            if (!ulBitmask[r])
            {
                NS_LOG_WARN("UL: RBG " << r << " notched RNTI=" << rnti);
                skip = true;
                break;
            }
        }
        if (skip)
        {
            continue;
        }

        for (uint16_t r = rbgStart; r < rbgStart + rbgLen; ++r)
        {
            ulRbgMap[r] = true;
        }

        PendingAlloc pa;
        pa.rbgStart = rbgStart;
        pa.rbgLen = rbgLen;
        pa.mcs = mcs;
        pa.symAvail = symAvail;
        pa.bandwidthInRbg = bandwidthInRbg;
        m_pendingUlAlloc[rnti] = pa;

        for (const auto& beam : activeUl)
        {
            for (const auto& uePair : beam.second)
            {
                if (uePair.first->m_rnti == rnti)
                {
                    auto& ueInfo = uePair.first;
                    ueInfo->m_ulRBG.clear();
                    ueInfo->m_ulSym.clear();
                    for (uint16_t r = rbgStart; r < rbgStart + rbgLen; ++r)
                    {
                        ueInfo->m_ulRBG.push_back(r);
                    }
                    for (uint32_t s = 0; s < symAvail; ++s)
                    {
                        ueInfo->m_ulSym.push_back(static_cast<uint8_t>(s));
                    }
                    break;
                }
            }
        }

        NS_LOG_DEBUG("UL AssignRBG RNTI=" << rnti << " rbgStart=" << rbgStart << " rbgLen="
                                          << rbgLen << " mcs=" << +mcs << " symAvail=" << symAvail);
    }

    m_shmInterface->CppRecvEnd();
    return ret;
}

std::shared_ptr<DciInfoElementTdma>
IbOranNrMacScheduler::CreateDlDci(PointInFTPlane* spoint,
                                  const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                                  uint32_t maxSym) const
{
    NS_LOG_FUNCTION(this);

    const uint16_t rnti = ueInfo->m_rnti;
    auto it = m_pendingDlAlloc.find(rnti);
    if (it == m_pendingDlAlloc.end())
    {
        NS_LOG_DEBUG("CreateDlDci: no pending allocation for RNTI=" << rnti);
        return nullptr;
    }

    const PendingAlloc& pa = it->second;

    const uint8_t numSym =
        static_cast<uint8_t>(std::min(pa.symAvail, static_cast<uint32_t>(maxSym)));
    if (numSym == 0)
    {
        NS_LOG_WARN("CreateDlDci: zero symbols RNTI=" << rnti);
        return nullptr;
    }

    // Build the RBG bitmask from the cached contiguous assignment.
    std::vector<bool> rbgBitmask(pa.bandwidthInRbg, false);
    for (uint16_t r = pa.rbgStart; r < pa.rbgStart + pa.rbgLen; ++r)
    {
        rbgBitmask[r] = true;
    }

    // TBS: PRBs = rbgLen × numRbPerRbg; multiply by numSym symbols (OFDMA).
    const uint32_t numPrb = static_cast<uint32_t>(pa.rbgLen) * GetNumRbPerRbg() * numSym;
    const uint32_t tbSize = m_dlAmc->CalculateTbSize(pa.mcs, 1 /*rank*/, numPrb);

    if (tbSize < 7)
    {
        // Base-class convention: return nullptr when TBS is too small for headers.
        NS_LOG_DEBUG("CreateDlDci: TBS=" << tbSize << " too small RNTI=" << rnti);
        return nullptr;
    }

    constexpr uint8_t rank{1};
    Ptr<const ComplexMatrixArray> precMats{nullptr};

    auto dci =
        std::make_shared<DciInfoElementTdma>(rnti,
                                             DciInfoElementTdma::DL,
                                             spoint->m_sym, // symStart = current DL starting symbol
                                             numSym,
                                             pa.mcs,
                                             rank,
                                             precMats,
                                             tbSize,
                                             1, // NDI = 1 (new data)
                                             0, // RV  = 0 (first tx)
                                             DciInfoElementTdma::DATA,
                                             GetBwpId(),
                                             GetTpc());

    dci->m_rbgBitmask = rbgBitmask;

    // Update per-slot tracking.
    m_ueDlAllocatedRbg[rnti] = pa.rbgLen;
    m_ueDlTotalBytesServed[rnti] += tbSize;

    NS_LOG_DEBUG("CreateDlDci RNTI=" << rnti << " symStart=" << +spoint->m_sym
                                     << " numSym=" << +numSym << " rbgStart=" << pa.rbgStart
                                     << " rbgLen=" << pa.rbgLen << " mcs=" << +pa.mcs
                                     << " tbs=" << tbSize);

    return dci;
}

std::shared_ptr<DciInfoElementTdma>
IbOranNrMacScheduler::CreateUlDci(PointInFTPlane* spoint,
                                  const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                                  uint32_t maxSym) const
{
    NS_LOG_FUNCTION(this);

    const uint16_t rnti = ueInfo->m_rnti;
    auto it = m_pendingUlAlloc.find(rnti);
    if (it == m_pendingUlAlloc.end())
    {
        NS_LOG_DEBUG("CreateUlDci: no pending allocation for RNTI=" << rnti);
        return nullptr;
    }

    const PendingAlloc& pa = it->second;

    const uint8_t numSym =
        static_cast<uint8_t>(std::min(pa.symAvail, static_cast<uint32_t>(maxSym)));
    if (numSym == 0)
    {
        NS_LOG_WARN("CreateUlDci: zero symbols RNTI=" << rnti);
        return nullptr;
    }

    std::vector<bool> rbgBitmask(pa.bandwidthInRbg, false);
    for (uint16_t r = pa.rbgStart; r < pa.rbgStart + pa.rbgLen; ++r)
    {
        rbgBitmask[r] = true;
    }

    const uint32_t numPrb = static_cast<uint32_t>(pa.rbgLen) * GetNumRbPerRbg() * numSym;
    const uint32_t tbSize = m_ulAmc->CalculateTbSize(pa.mcs, 1 /*rank*/, numPrb);

    if (tbSize < 7)
    {
        NS_LOG_DEBUG("CreateUlDci: TBS=" << tbSize << " too small RNTI=" << rnti);
        return nullptr;
    }

    // UL symbols are placed backwards from spoint->m_sym (the last available
    // data symbol).  The base-class OFDMA UL path sets spoint accordingly.
    const uint8_t symStart = static_cast<uint8_t>(spoint->m_sym - numSym);

    constexpr uint8_t rank{1};
    Ptr<const ComplexMatrixArray> precMats{nullptr};

    auto dci = std::make_shared<DciInfoElementTdma>(rnti,
                                                    DciInfoElementTdma::UL,
                                                    symStart,
                                                    numSym,
                                                    pa.mcs,
                                                    rank,
                                                    precMats,
                                                    tbSize,
                                                    1, // NDI
                                                    0, // RV
                                                    DciInfoElementTdma::DATA,
                                                    GetBwpId(),
                                                    GetTpc());

    dci->m_rbgBitmask = rbgBitmask;

    m_ueUlAllocatedRbg[rnti] = pa.rbgLen;
    m_ueUlTotalBytesServed[rnti] += tbSize;

    NS_LOG_DEBUG("CreateUlDci RNTI=" << rnti << " symStart=" << +symStart << " numSym=" << +numSym
                                     << " rbgStart=" << pa.rbgStart << " rbgLen=" << pa.rbgLen
                                     << " mcs=" << +pa.mcs << " tbs=" << tbSize);

    return dci;
}

void
IbOranNrMacScheduler::ChangeDlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const
{
    spoint->m_sym += symOfBeam;
    spoint->m_rbg = 0;
}

void
IbOranNrMacScheduler::ChangeUlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const
{
    spoint->m_sym -= symOfBeam;
    spoint->m_rbg = 0;
}

} // namespace ns3
