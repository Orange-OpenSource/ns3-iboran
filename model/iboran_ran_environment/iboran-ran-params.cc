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

#include "iboran-ran-params.h"

#include "ns3/command-line.h"

#include <iomanip>

namespace ns3
{

SimParams::SimParams()
    : ratType(RatType::LTE) // default: LTE
{
    // -- Common Parameters ----------------------------------------------
    common.simTime = 20.0;
    common.simAreaLength = 500.0;
    common.ueNum = 10;
    common.xnbNum = 1;
    common.remoteHostNum = 1;

    common.handoverHysteresis = 3.0;
    common.handoverTimeToTrigger = 256;

    common.ueMinTxPower = -40.0;
    common.ueMaxTxPower = 23.0;
    common.xnbTxPower = 46.0;
    /* Urban macrocell: 46 [dBm] , micro/pico-cell: 30–33 [dBm].*/
    common.compensationFactor = 0.8;
    /* Urban macrocell: (0.7-0.8; small cell: (0.4-0.6) */

    common.backhaulDataRate = DataRate("100Gbps");
    common.backhaulMtu = 1500;
    common.backhaulDelay = 10.0;

    common.enableDlFlow = true;
    common.dlTxRepeat = 1;
    common.dlPacketSize = 1400;
    common.dlBitRate = 200e3;
    common.dlMinInterval = 0.0;
    common.dlUseUdp = true;

    common.enableUlFlow = true;
    common.ulTxRepeat = 1;
    common.ulPacketSize = 1400;
    common.ulBitRate = 100e3;
    common.ulMinInterval = 0.0;
    common.ulUseUdp = true;

    common.dataFormat = DataFormat::IMAGE;
    /* IMAGE, VIDEO, EMBEDDING, TEXT, CUSTOM */

    common.enableIbOranSchedCoreLog = false;
    common.enableRrcLog = false;

    // -- LTE Parameters -------------------------------------------------
    lte.dlEarfcn = 2750;  // downlink EARFCN 2750: 2620–2690 MHz, band 7
    lte.ulEarfcn = 20750; // uplink EARFCN 20750: 2500–2570 MHz, band 7
    /* Calculate 4G LTE EARFCN: https://5g-tools.com/4g-lte-earfcn-calculator/ */
    lte.dlRbNum = 100; // 100 RBs: 20 MHz bandwidth
    lte.ulRbNum = 100; // 100 RBs: 20 MHz bandwidth
    /*
     * Since 'RbNum'=100 is considered default for fadding traces,
     * it must not be changed as far as we use the same fading model.
     * Also, define as 'MAX_PRB_NUM_LTE'
     */
    lte.transmissionMode = 2;
    /*
     * 3GPP transmission modes (TMs):
     * TM1 -> SISO, 1x1 antenna, single layer
     * TM2 -> Closed-loop 2x2 MIMO, 2 layers, uses UE feedback (CQI/PMI/RI) for precoding
     * TM3 -> Open-loop 2x2 MIMO, 2 layers, fixed precoding (no feedback)
     * TM4 -> Closed-loop rank 1 precoding, 2x2 MIMO, single layer, uses UE feedback
     * TM5 -> Multi-user MIMO, 2x2 MIMO, serves multiple UEs on same resources
     * TM6 -> Closed-loop SU-MIMO with rank adaptation, 2x2 MIMO, layers adapted per UE
     * TM7 -> Advanced 4x2 MIMO, multiple UEs, advanced precoding
     * Note: In ns-3.42, TM1–TM3 are fully implemented; TM4–TM7 may have limited support.
     */
    lte.p0NominalPusch = -96.0;
    /* Urban macrocell:  = (−96 to −106) [dBm] */
}

SimParams&
SimParams::GetInstance()
{
    static SimParams instance;
    return instance;
}

// void
// SimParams::SetRatType(RatType rat)
// {
//     ratType = rat;
// }

void
SimParams::SetRatType(std::string_view type)
{
    ratType = type;
}

void
SimParams::LoadFromCommandLine(int argc, char* argv[])
{
    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation time", common.simTime);
    cmd.AddValue("ueNum", "Number of UEs", common.ueNum);
    cmd.AddValue("xnbTxPower", "(eNodeB/gNB Transmission power", common.xnbTxPower);
    // ... add all parameters
    cmd.Parse(argc, argv);

    // recalculate dependent values
    common.dlMinInterval = common.dlPacketSize * 8.0 / common.dlBitRate;
    common.ulMinInterval = common.ulPacketSize * 8.0 / common.ulBitRate;
}

std::ostream&
operator<<(std::ostream& os, const SimParams& params)
{
    const CommonRanParams& commonParams = params.GetCommonRanParams();
    os << "\n-- RAN Simulation Parameters ----------\n"
       << std::left << std::setw(30) << "Radio Access Technology Type" << ": "
       << "LTE" << "\n"
       << std::setw(30) << "Simulation Time" << ": " << std::setw(1) << commonParams.simTime
       << " s\n"
       << std::setw(30) << "Simulation Area Length" << ": " << std::setw(1)
       << commonParams.simAreaLength << " m\n"
       << std::setw(30) << "Number of UEs" << ": " << std::setw(1) << commonParams.ueNum << "\n"
       << std::setw(30) << "Number of eNodeBs" << ": " << std::setw(1) << commonParams.xnbNum
       << "\n"
       << std::setw(30) << "Number of Remote Hosts" << ": " << std::setw(1)
       << commonParams.remoteHostNum << "\n"
       << std::setw(30) << "Handover Hysteresis" << ": " << std::setw(1)
       << commonParams.handoverHysteresis << " dB\n"
       << std::setw(30) << "Handover Time-to-Trigger" << ": " << std::setw(1)
       << commonParams.handoverTimeToTrigger << " ms\n"
       << std::setw(30) << "UE Min Transmission Power" << ": " << std::setw(1)
       << commonParams.ueMinTxPower << " dBm\n"
       << std::setw(30) << "UE Max Transmission Power" << ": " << std::setw(1)
       << commonParams.ueMaxTxPower << " dBm\n"
       << std::setw(30) << "eNodeB Transmission Power"
       << ": " << std::setw(1) << commonParams.xnbTxPower << " dBm\n"
       << std::setw(30) << "Pathloss Compensation Factor" << ": " << std::setw(1)
       << commonParams.compensationFactor << "\n"
       << std::setw(30) << "Backhaul Data Rate" << ": " << std::setw(1)
       << (commonParams.backhaulDataRate.GetBitRate() / 1e9) << " Gbps\n"
       << std::setw(30) << "Backhaul MTU" << ": " << std::setw(1) << commonParams.backhaulMtu
       << " bytes\n"
       << std::setw(30) << "Backhaul Delay" << ": " << std::setw(1) << commonParams.backhaulDelay
       << " ms\n"
       << std::setw(30) << "Downlink Flow" << ": " << std::setw(1)
       << (commonParams.enableDlFlow ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "Downlink Data Format" << ": " << std::setw(1)
       << (commonParams.dataFormat == DataFormat::IMAGE       ? "IMAGE"
           : commonParams.dataFormat == DataFormat::VIDEO     ? "VIDEO"
           : commonParams.dataFormat == DataFormat::EMBEDDING ? "EMBEDDING"
           : commonParams.dataFormat == DataFormat::TEXT      ? "TEXT"
                                                              : "CUSTOM")
       << "\n"
       << std::setw(30) << "Downlink Transmission Repeat" << ": " << std::setw(1)
       << commonParams.dlTxRepeat << "\n"
       << std::setw(30) << "Downlink Packet Size" << ": " << std::setw(1)
       << commonParams.dlPacketSize << " bytes\n"
       << std::setw(30) << "Downlink Bit Rate" << ": " << std::setw(1)
       << commonParams.dlBitRate / 1e6 << " Mbps\n"
       << std::setw(30) << "Downlink Min Packet Interval" << ": " << std::setw(1)
       << commonParams.dlMinInterval * 1e3 << " ms\n"
       << std::setw(30) << "Downlink L4 Protocol" << ": " << std::setw(1)
       << (commonParams.dlUseUdp ? "UDP" : "TCP") << "\n"
       << std::setw(30) << "Uplink Flow" << ": " << std::setw(1)
       << (commonParams.enableUlFlow ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "Uplink Data Format" << ": " << std::setw(1)
       << (commonParams.dataFormat == DataFormat::IMAGE       ? "IMAGE"
           : commonParams.dataFormat == DataFormat::VIDEO     ? "VIDEO"
           : commonParams.dataFormat == DataFormat::EMBEDDING ? "EMBEDDING"
           : commonParams.dataFormat == DataFormat::TEXT      ? "TEXT"
                                                              : "CUSTOM")
       << "\n"
       << std::setw(30) << "Uplink Transmission Repeat" << ": " << std::setw(1)
       << commonParams.ulTxRepeat << "\n"
       << std::setw(30) << "Uplink Packet Size" << ": " << std::setw(1) << commonParams.ulPacketSize
       << " bytes\n"
       << std::setw(30) << "Uplink Bit Rate" << ": " << std::setw(1) << commonParams.ulBitRate / 1e3
       << " kbps\n"
       << std::setw(30) << "Uplink Min Packet Interval" << ": " << std::setw(1)
       << commonParams.ulMinInterval * 1e3 << " ms\n"
       << std::setw(30) << "Uplink L4 Protocol" << ": " << std::setw(1)
       << (commonParams.ulUseUdp ? "UDP" : "TCP") << "\n"
       << std::setw(30) << "ib-ORAN MAC Scheduler Logging" << ": " << std::setw(1)
       << (commonParams.enableIbOranSchedCoreLog ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "RRC Logging" << ": " << std::setw(1)
       << (commonParams.enableRrcLog ? "Enabled" : "Disabled") << "\n";

    if (params.GetRatType() == RatType::LTE)
    {
        const LteParams& lteParams = params.GetLteParams();
        os << "\n-- LTE Specifications -----------------\n"
           << std::setw(30) << "Downlink EARFCN" << ": " << std::setw(1) << lteParams.dlEarfcn
           << "\n"
           << std::setw(30) << "Uplink EARFCN" << ": " << std::setw(1) << lteParams.ulEarfcn << "\n"
           << std::setw(30) << "Downlink Resource Blocks" << ": " << std::setw(1)
           << lteParams.dlRbNum << "\n"
           << std::setw(30) << "Uplink Resource Blocks" << ": " << std::setw(1) << lteParams.ulRbNum
           << "\n"
           << std::setw(30) << "Transmission Mode" << ": " << std::setw(1)
           << lteParams.transmissionMode << "\n"
           << std::setw(30) << "Nominal PSD Target for PUSCH" << ": " << std::setw(1)
           << lteParams.p0NominalPusch << " dBm\n";
    }
    return os;
}

void
SimParams::Print(std::ostream& os) const
{
    os << *this;
}

} // namespace ns3
