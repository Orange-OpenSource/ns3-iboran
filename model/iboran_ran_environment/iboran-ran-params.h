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

#ifndef IBORAN_RAN_PARAMS_H
#define IBORAN_RAN_PARAMS_H

#include "../iboran_application/iboran-application.h"

#include "ns3/data-rate.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace ns3
{

// == COMMON PARAMETERS =========================================================
struct CommonRanParams
{
    double simTime;         // simulation time [s]
    double simAreaLength;   // simulation area length (square) [m]
    uint32_t ueNum;         // no. of UEs
    uint32_t xnbNum;        // no. of eNodeBs/gNBs (fixed for now)
    uint32_t remoteHostNum; // no. of Remote Hosts (fixed for now)

    double handoverHysteresis;    // handover hysteresis [dB]
    double handoverTimeToTrigger; // handover time to trigger [ms]

    double ueMinTxPower;       // UE min transmit power [dBm]
    double ueMaxTxPower;       // UE max transmit power [dBm]
    double xnbTxPower;         // eNodeB/gNB maximum transmit power [dBm]
    double compensationFactor; // pathloss compensation factor (0-1)

    DataRate backhaulDataRate; // backhaul link data rate
    uint32_t backhaulMtu;      // backhaul link max transmission unit [bytes]
    double backhaulDelay;      // backhaul link delay [ms]

    bool enableDlFlow;     // enable downlink data flows]
    uint32_t dlTxRepeat;   // number of times to repeat data transmission
    uint32_t dlPacketSize; // downlink packet size [bytes]
    uint32_t dlBitRate;    // downlink bit rate [bps]
    double dlMinInterval;  // downlink minimum inter-packet interval [ms]
    bool dlUseUdp;         // whether to use UDP protocol instead of TCP

    bool enableUlFlow;     // enable uplink data flow
    uint32_t ulTxRepeat;   // number of times to repeat data transmission
    uint32_t ulPacketSize; // uplink packet size [bytes]
    uint32_t ulBitRate;    // uplink bit rate [bps]
    double ulMinInterval;  // uplink minimum inter-packet interval [ms]
    bool ulUseUdp;         // whether to use UDP protocol instead of TCP

    DataFormat dataFormat; // data format (assumed similar for uplink and downlink)

    bool enableIbOranSchedCoreLog; // enable ib-ORAN MAC scheduler core logging
    bool enableRrcLog;             // enable RRC logging
};

// == LTE PARAMETERS ============================================================
struct LteParams
{
    uint32_t dlEarfcn;         // downlink EARFCN
    uint32_t ulEarfcn;         // uplink EARFCN
    uint32_t dlRbNum;          // no. downlink RBs
    uint32_t ulRbNum;          // no. uplink RBs
    uint32_t transmissionMode; // downlink transmission mode
    double p0NominalPusch;     // nominal PSD target for PUSCH [dBm]
};

// == NR PARAMETERS =============================================================
struct NrParams
{
    double centralFrequency;       // central frequency [Hz]
    double bandwidth;              // bandwidth [Hz]
    uint32_t numCc;                // number of component carriers (>= 1 & <= 16)
    uint32_t numBwp;               // number of bandwidth parts (1-4)
    uint32_t numerology;           // 3GPP 5G/NR numerology (0-6)
    std::string tddPattern;        // TDD pattern
    uint32_t gNbAntennaNumRows;    // number of rows in gNB antenna
    uint32_t gNbAntennaNumColumns; // number of columns in gNB antenna
    uint32_t ueAntennaNumRows;     // number of rows in UE antenna
    uint32_t ueAntennaNumColumns;  // number of columns in UE antenna
};

// == MAIN CLASS ================================================================
class SimParams
{
  public:
    static SimParams& GetInstance();

    SimParams(const SimParams&) = delete;
    SimParams& operator=(const SimParams&) = delete;

    void SetRatType(std::string_view type);

    const std::string& GetRatType() const
    {
        return ratType;
    }

    void LoadFromCommandLine(int argc, char* argv[]);
    void Print(std::ostream& os) const;

    // accessors for common parameters
    CommonRanParams& GetCommonRanParams()
    {
        return common;
    }

    const CommonRanParams& GetCommonRanParams() const
    {
        return common;
    }

    // accessors for LTE parameters
    LteParams& GetLteParams()
    {
        return lte;
    }

    const LteParams& GetLteParams() const
    {
        return lte;
    }

    // accessors for NR parameters
    NrParams& GetNrParams()
    {
        return nr;
    }

    const NrParams& GetNrParams() const
    {
        return nr;
    }

  private:
    SimParams(); // private constructor for singleton

    std::string ratType;
    CommonRanParams common;
    LteParams lte;
    NrParams nr;
};

std::ostream& operator<<(std::ostream& os, const SimParams& params);

} // namespace ns3

#endif
