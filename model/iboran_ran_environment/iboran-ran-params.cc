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
#include "ns3/log.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <yaml-cpp/yaml.h>

NS_LOG_COMPONENT_DEFINE("SimParams");

namespace
{

ns3::DataFormat
StringToDataFormat(const std::string& str)
{
    ns3::DataFormat format;
    if (ns3::ParseDataFormat(str, format))
    {
        return format;
    }
    NS_LOG_ERROR("Unknown data format string '" << str << "'. Defaulting to EMBEDDING.");
    return ns3::DataFormat::EMBEDDING;
}

} // namespace

namespace ns3
{

SimParams::SimParams()
    : ratType(RatType::LTE) // default: LTE
{
    // -- Common Parameters ----------------------------------------------
    common.simTime = 20.0;
    common.simAreaLength = 500.0;
    common.ueNum = 6;
    common.xnbNum = 1;
    common.remoteHostNum = 1;

    common.handoverHysteresis = 3.0;
    common.handoverTimeToTrigger = 256;

    common.ueMinTxPower = -40.0;
    common.ueMaxTxPower = 23.0;
    common.xnbTxPower = 46.0;
    /* Urban macrocell: 46 [dBm] , micro/pico-cell: 30–33 [dBm].*/
    common.compensationFactor = 0.8;
    /* Urban macrocell: 0.7-0.8; small cell: 0.4-0.6 */

    common.backhaulDataRate = DataRate("100Gbps");
    common.backhaulMtu = 1500;
    common.backhaulDelay = 10.0;

    common.appDlEnable = true;
    common.appDlPacketSize = 512;
    common.appDlBitRate = 2e6;
    common.appDlMinInterval = 0.0;
    common.appDlTransport = "TCP";

    common.appUlEnable = true;
    common.appUlPacketSize = 512;
    common.appUlBitRate = 1e6;
    common.appUlMinInterval = 0.0;
    common.appUlTransport = "TCP";

    // overridden by orchestrator.data_format in LoadFromYaml
    common.dataFormat = DataFormat::EMBEDDING;

    common.appFraming = true;
    common.appEnvelope = "None";
    common.appEmbeddingPrefixCriticality = false;
    common.appFillPolicy = "Auto";
    common.appSaveAllMessages = false;
    common.appSaveMessageLog = false;

    common.enableIbOranSchedCoreLog = false;
    common.enableRrcLog = false;

    common.rngRun = 1;

    // -- LTE Parameters -------------------------------------------------
    lte.dlEarfcn = 2750;  // downlink EARFCN 2750: 2620–2690 MHz, band 7
    lte.ulEarfcn = 20750; // uplink EARFCN 20750: 2500–2570 MHz, band 7
    /* Calculate 4G LTE EARFCN: https://5g-tools.com/4g-lte-earfcn-calculator/ */
    lte.dlRbNum = 100; // 100 RBs: 20 MHz bandwidth
    lte.ulRbNum = 100; // 100 RBs: 20 MHz bandwidth
    /*
     * RbNum = 100 is assumed by the fading traces; do not change it
     * unless the fading model changes too (see MAX_PRB_NUM_LTE).
     */
    lte.transmissionMode = 2;
    /*
     * ns-3 index of the 3GPP transmission mode (TM), counted from 0:
     *   0 -> TM1: single antenna port (SISO)
     *   1 -> TM2: transmit diversity
     *   2 -> TM3: open-loop spatial multiplexing
     *   3 -> TM4: closed-loop spatial multiplexing
     *   4 -> TM5: multi-user MIMO
     *   5 -> TM6: closed-loop single-layer precoding
     *   6 -> TM7: single-layer beamforming (antenna port 5)
     */
    lte.p0NominalPusch = -96.0;
    /* Urban macrocell: -96 to -106 dBm */

    // -- NR Parameters --------------------------------------------------
    nr.centralFrequency = 3.5e9;
    /* 28 GHz (mmWave) or 3.5e9 (sub-6 GHz) */
    nr.bandwidth = 10e6;
    nr.numCc = 1;
    nr.numBwp = 1;
    nr.numerology = 1;
    /*
     * Numerology values:
     * 0 -> 15 kHz SCS (similar to LTE)
     * 1 -> 30 kHz SCS (typical for sub-6 GHz)
     * 2 -> 60 kHz SCS (typical for mmWave)
     * 3 -> 120 kHz SCS (high mobility, mmWave)
     * 4 -> 240 kHz SCS (very high frequency)
     * 5 -> 480 kHz SCS
     * 6 -> 960 kHz SCS
     */
    // NOTE changing numerology, bandwidth, and freq. requires modifying
    // 'MAX_PRB_NUM_NR'

    nr.tddPattern = "DL|DL|DL|DL|F|UL|UL|UL|UL|F|";
    /**
     * Terminology: DL->Downlink, UL->Uplink, F->Flexible, S->Special slot
     * Some examples:
     * All downlink: "DL|DL|DL|DL|DL|DL|DL|DL|DL|DL|"
     * Balanced: "DL|DL|DL|DL|F|UL|UL|UL|UL|F|"
     * Downlink heavy: "DL|DL|DL|DL|DL|DL|DL|F|UL|UL|"
     */

    nr.gNbAntennaNumRows = 4;
    nr.gNbAntennaNumColumns = 4;
    nr.ueAntennaNumRows = 2;
    nr.ueAntennaNumColumns = 2;
}

SimParams&
SimParams::GetInstance()
{
    static SimParams instance;
    return instance;
}

void
SimParams::SetRatType(std::string_view type)
{
    ratType = type;
}

void
SimParams::LoadFromYaml(const std::string& filepath)
{
    YAML::Node root;
    try
    {
        root = YAML::LoadFile(filepath);
    }
    catch (const YAML::BadFile&)
    {
        NS_LOG_ERROR("SimParams: could not open '" << filepath << "'. Using built-in defaults.");
        return;
    }
    catch (const YAML::ParserException& e)
    {
        NS_LOG_ERROR("SimParams: failed to parse '" << filepath << "': " << e.what());
        return;
    }

    if (root["rat_type"])
    {
        ratType = root["rat_type"].as<std::string>();
    }

    // -- Common Parameters ------------------------------------------------
    if (auto c = root["common"])
    {
        if (c["simTime"])
        {
            common.simTime = c["simTime"].as<double>();
        }
        if (c["simAreaLength"])
        {
            common.simAreaLength = c["simAreaLength"].as<double>();
        }
        if (c["ueNum"])
        {
            common.ueNum = c["ueNum"].as<uint32_t>();
        }
        if (c["xnbNum"])
        {
            common.xnbNum = c["xnbNum"].as<uint32_t>();
        }
        if (c["remoteHostNum"])
        {
            common.remoteHostNum = c["remoteHostNum"].as<uint32_t>();
        }

        if (c["handoverHysteresis"])
        {
            common.handoverHysteresis = c["handoverHysteresis"].as<double>();
        }
        if (c["handoverTimeToTrigger"])
        {
            common.handoverTimeToTrigger = c["handoverTimeToTrigger"].as<double>();
        }

        if (c["ueMinTxPower"])
        {
            common.ueMinTxPower = c["ueMinTxPower"].as<double>();
        }
        if (c["ueMaxTxPower"])
        {
            common.ueMaxTxPower = c["ueMaxTxPower"].as<double>();
        }
        if (c["xnbTxPower"])
        {
            common.xnbTxPower = c["xnbTxPower"].as<double>();
        }
        if (c["compensationFactor"])
        {
            common.compensationFactor = c["compensationFactor"].as<double>();
        }

        if (c["backhaulDataRate"])
        {
            common.backhaulDataRate = DataRate(c["backhaulDataRate"].as<std::string>());
        }
        if (c["backhaulMtu"])
        {
            common.backhaulMtu = c["backhaulMtu"].as<uint32_t>();
        }
        if (c["backhaulDelay"])
        {
            common.backhaulDelay = c["backhaulDelay"].as<double>();
        }

        if (c["appDlEnable"])
        {
            common.appDlEnable = c["appDlEnable"].as<bool>();
        }
        if (c["appDlPacketSize"])
        {
            common.appDlPacketSize = c["appDlPacketSize"].as<uint32_t>();
        }
        if (c["appDlBitRate"])
        {
            common.appDlBitRate = c["appDlBitRate"].as<uint32_t>();
        }
        if (c["appDlTransport"])
        {
            common.appDlTransport = c["appDlTransport"].as<std::string>();
        }

        if (c["appUlEnable"])
        {
            common.appUlEnable = c["appUlEnable"].as<bool>();
        }
        if (c["appUlPacketSize"])
        {
            common.appUlPacketSize = c["appUlPacketSize"].as<uint32_t>();
        }
        if (c["appUlBitRate"])
        {
            common.appUlBitRate = c["appUlBitRate"].as<uint32_t>();
        }
        if (c["appUlTransport"])
        {
            common.appUlTransport = c["appUlTransport"].as<std::string>();
        }

        if (c["appFraming"])
        {
            common.appFraming = c["appFraming"].as<bool>();
        }
        if (c["appEnvelope"])
        {
            common.appEnvelope = c["appEnvelope"].as<std::string>();
        }
        if (c["appEmbeddingPrefixCriticality"])
        {
            common.appEmbeddingPrefixCriticality = c["appEmbeddingPrefixCriticality"].as<bool>();
        }
        if (c["appFillPolicy"])
        {
            common.appFillPolicy = c["appFillPolicy"].as<std::string>();
        }
        if (c["appSaveAllMessages"])
        {
            common.appSaveAllMessages = c["appSaveAllMessages"].as<bool>();
        }
        if (c["appSaveMessageLog"])
        {
            common.appSaveMessageLog = c["appSaveMessageLog"].as<bool>();
        }

        if (c["enableIbOranSchedCoreLog"])
        {
            common.enableIbOranSchedCoreLog = c["enableIbOranSchedCoreLog"].as<bool>();
        }
        if (c["enableRrcLog"])
        {
            common.enableRrcLog = c["enableRrcLog"].as<bool>();
        }
        if (c["rngRun"])
        {
            common.rngRun = c["rngRun"].as<uint32_t>();
        }
    }

    if (auto py = root["orchestrator"]; py["data_format"])
    {
        common.dataFormat = StringToDataFormat(py["data_format"].as<std::string>());
    }

    // -- LTE Parameters -----------------------------------------------------
    if (auto l = root["lte"])
    {
        if (l["dlEarfcn"])
        {
            lte.dlEarfcn = l["dlEarfcn"].as<uint32_t>();
        }
        if (l["ulEarfcn"])
        {
            lte.ulEarfcn = l["ulEarfcn"].as<uint32_t>();
        }
        if (l["dlRbNum"])
        {
            lte.dlRbNum = l["dlRbNum"].as<uint32_t>();
        }
        if (l["ulRbNum"])
        {
            lte.ulRbNum = l["ulRbNum"].as<uint32_t>();
        }
        if (l["transmissionMode"])
        {
            lte.transmissionMode = l["transmissionMode"].as<uint32_t>();
        }
        if (l["p0NominalPusch"])
        {
            lte.p0NominalPusch = l["p0NominalPusch"].as<double>();
        }
    }

    // -- NR Parameters --------------------------------------------------------
    if (auto n = root["nr"])
    {
        if (n["centralFrequency"])
        {
            nr.centralFrequency = n["centralFrequency"].as<double>();
        }
        if (n["bandwidth"])
        {
            nr.bandwidth = n["bandwidth"].as<double>();
        }
        if (n["numCc"])
        {
            nr.numCc = n["numCc"].as<uint32_t>();
        }
        if (n["numBwp"])
        {
            nr.numBwp = n["numBwp"].as<uint32_t>();
        }
        if (n["numerology"])
        {
            nr.numerology = n["numerology"].as<uint32_t>();
        }
        if (n["tddPattern"])
        {
            nr.tddPattern = n["tddPattern"].as<std::string>();
        }
        if (n["gNbAntennaNumRows"])
        {
            nr.gNbAntennaNumRows = n["gNbAntennaNumRows"].as<uint32_t>();
        }
        if (n["gNbAntennaNumColumns"])
        {
            nr.gNbAntennaNumColumns = n["gNbAntennaNumColumns"].as<uint32_t>();
        }
        if (n["ueAntennaNumRows"])
        {
            nr.ueAntennaNumRows = n["ueAntennaNumRows"].as<uint32_t>();
        }
        if (n["ueAntennaNumColumns"])
        {
            nr.ueAntennaNumColumns = n["ueAntennaNumColumns"].as<uint32_t>();
        }
    }

    // recalculate dependent values now that packet size / bit rate are loaded
    common.appDlMinInterval = common.appDlPacketSize * 8.0 / common.appDlBitRate;
    common.appUlMinInterval = common.appUlPacketSize * 8.0 / common.appUlBitRate;

    for (std::string* transport : {&common.appDlTransport, &common.appUlTransport})
    {
        if (*transport != "UDP" && *transport != "TCP")
        {
            NS_LOG_ERROR("Unknown transport '" << *transport << "', using TCP");
            *transport = "TCP";
        }
    }
}

std::ostream&
operator<<(std::ostream& os, const SimParams& params)
{
    const CommonRanParams& commonParams = params.GetCommonRanParams();
    os << "\n-- RAN Simulation Parameters ----------\n"
       << std::left << std::setw(30) << "Radio Access Technology Type" << ": "
       << (params.GetRatType() == RatType::LTE ? "LTE" : "NR") << "\n"
       << std::setw(30) << "Simulation Time" << ": " << std::setw(1) << commonParams.simTime
       << " s\n"
       << std::setw(30) << "Simulation Area Length" << ": " << std::setw(1)
       << commonParams.simAreaLength << " m\n"
       << std::setw(30) << "Number of UEs" << ": " << std::setw(1) << commonParams.ueNum << "\n"
       << std::setw(30)
       << (params.GetRatType() == RatType::LTE ? "Number of eNodeBs" : "Number of gNBs") << ": "
       << std::setw(1) << commonParams.xnbNum << "\n"
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
       << std::setw(30)
       << (params.GetRatType() == RatType::LTE ? "eNodeB Transmission Power"
                                               : "gNB Transmission Power")
       << ": " << std::setw(1) << commonParams.xnbTxPower << " dBm\n"
       << std::setw(30) << "Pathloss Compensation Factor" << ": " << std::setw(1)
       << commonParams.compensationFactor << "\n"
       << std::setw(30) << "Backhaul Data Rate" << ": " << std::setw(1)
       << (commonParams.backhaulDataRate.GetBitRate() / 1e9) << " Gbps\n"
       << std::setw(30) << "Backhaul MTU" << ": " << std::setw(1) << commonParams.backhaulMtu
       << " bytes\n"
       << std::setw(30) << "Backhaul Delay" << ": " << std::setw(1) << commonParams.backhaulDelay
       << " ms\n"
       << std::setw(30) << "App Downlink" << ": " << std::setw(1)
       << (commonParams.appDlEnable ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "App Downlink Data Format" << ": " << std::setw(1)
       << DataFormatToString(commonParams.dataFormat) << "\n"
       << std::setw(30) << "App Downlink Packet Size" << ": " << std::setw(1)
       << commonParams.appDlPacketSize << " bytes\n"
       << std::setw(30) << "App Downlink Bit Rate" << ": " << std::setw(1)
       << commonParams.appDlBitRate / 1e3 << " kbps\n"
       << std::setw(30) << "App Downlink Min Interval" << ": " << std::setw(1)
       << commonParams.appDlMinInterval * 1e3 << " ms\n"
       << std::setw(30) << "App Downlink Transport" << ": " << std::setw(1)
       << (commonParams.appDlTransport) << "\n"
       << std::setw(30) << "App Uplink" << ": " << std::setw(1)
       << (commonParams.appUlEnable ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "App Uplink Data Format" << ": " << std::setw(1)
       << DataFormatToString(commonParams.dataFormat) << "\n"
       << std::setw(30) << "App Uplink Packet Size" << ": " << std::setw(1)
       << commonParams.appUlPacketSize << " bytes\n"
       << std::setw(30) << "App Uplink Bit Rate" << ": " << std::setw(1)
       << commonParams.appUlBitRate / 1e3 << " kbps\n"
       << std::setw(30) << "App Uplink Min Interval" << ": " << std::setw(1)
       << commonParams.appUlMinInterval * 1e3 << " ms\n"
       << std::setw(30) << "App Uplink Transport" << ": " << std::setw(1)
       << (commonParams.appUlTransport) << "\n"
       << std::setw(30) << "App Framing" << ": " << std::setw(1)
       << (commonParams.appFraming ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "App Envelope" << ": " << std::setw(1) << commonParams.appEnvelope
       << "\n"
       << std::setw(30) << "App Fill Policy" << ": " << std::setw(1) << commonParams.appFillPolicy
       << "\n"
       << std::setw(30) << "App Save All Messages" << ": " << std::setw(1)
       << (commonParams.appSaveAllMessages ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "App Message Log" << ": " << std::setw(1)
       << (commonParams.appSaveMessageLog ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "MAC Scheduler Logging" << ": " << std::setw(1)
       << (commonParams.enableIbOranSchedCoreLog ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "RRC Logging" << ": " << std::setw(1)
       << (commonParams.enableRrcLog ? "Enabled" : "Disabled") << "\n"
       << std::setw(30) << "RNG Run" << ": " << std::setw(1) << commonParams.rngRun << "\n";

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
    else if (params.GetRatType() == RatType::NR)
    {
        const NrParams& nrParams = params.GetNrParams();
        os << "\n-- NR Specifications ------------------\n"
           << std::setw(30) << "Central Frequency" << ": " << std::setw(1)
           << nrParams.centralFrequency / 1e9 << " GHz\n"
           << std::setw(30) << "Bandwidth" << ": " << std::setw(1) << nrParams.bandwidth / 1e6
           << " MHz\n"
           << std::setw(30) << "Component Carriers" << ": " << std::setw(1) << nrParams.numCc
           << "\n"
           << std::setw(30) << "Number of BWPs" << ": " << std::setw(1) << nrParams.numBwp << "\n"
           << std::setw(30) << "Numerology" << ": " << std::setw(1) << nrParams.numerology << "\n"
           << std::setw(30) << "TDD Pattern" << ": " << std::setw(1) << nrParams.tddPattern << "\n"
           << std::setw(30) << "gNB Antenna Dimension" << ": " << std::setw(1)
           << nrParams.gNbAntennaNumRows << " × " << nrParams.gNbAntennaNumColumns << "\n"
           << std::setw(30) << "UE Antenna Dimension" << ": " << std::setw(1)
           << nrParams.ueAntennaNumRows << " × " << nrParams.ueAntennaNumColumns << "\n";
    }
    return os;
}

void
SimParams::Print(std::ostream& os) const
{
    os << *this;
}

} // namespace ns3
