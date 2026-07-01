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

#ifndef IBORAN_SHM_STRUCTURE_H
#define IBORAN_SHM_STRUCTURE_H

#include <cstdint>

namespace ns3
{

#define MAX_UE_NUM 32
#define MAX_PRB_NUM_LTE 100
#define MAX_PRB_NUM_NR 264
#define MAX_ID_LEN 32

#define E3SM_INDICATION 0x01
#define E3SM_CONTROL 0x02

#define E2SM_KPM_REPORT 0x11
#define E2SM_RC_CONTROL 0x12

struct RatType
{
    static constexpr const char* LTE = "LTE";
    static constexpr const char* NR = "NR";
};

enum LinkDirection
{
    DOWNLINK = 0,
    UPLINK = 1
};

// == E2SM-KPM REPORT STRUCTURE =================================================
struct UeE2KpmReport
{
    uint16_t rnti;              // Radio Network Temporary Identifier
    uint8_t cqi;                // Channel Quality Indicator (0-15)
    uint32_t bufferLevel;       // bytes waiting in buffer
    char contentId[MAX_ID_LEN]; // object ID the UE observes
    uint8_t qci;                // QoS Class Identifier (1-9)
    double achievableRate;      // achievable rate [Mbps]
    double actualThroughput;    // actual throughput [Mbps]
    uint32_t totalBytesServed;  // total bytes served so far
    uint32_t packetDelay;       // current packet delay [ms]
} __attribute__((packed));

struct E2KpmReport
{
    uint16_t type;                // E2SM message type
    uint64_t timestamp;           // current simulation time [ns]
    uint32_t frameNo;             // current frame number
    uint32_t numUe;               // number of active UEs
    uint32_t totalBufferLevel;    // sum of all UE buffers
    uint16_t numRbg;              // number of available RBGs
    int rbgSize;                  // RBG size
    uint8_t linkDirection;        // DL: 0, UL: 1
    uint32_t id;                  // unique ID for tracking in logs
    UeE2KpmReport ue[MAX_UE_NUM]; // array of UE states
} __attribute__((packed));

// == E2SM-RC CONTROL STRUCTURE =================================================
struct UeE2RcControl
{
    uint16_t rnti;             // which UE to serve
    uint16_t maxRbgAllocation; // maximum RBG allocation
} __attribute__((packed));

struct E2RcControl
{
    uint16_t type;           // E2SM message type
    uint32_t maxUeSchedSize; // maximum number of UEs scheduled
    bool skipThisStep;       // don't schedule anything
    uint8_t linkDirection;   // DL: 0, UL: 1
    uint32_t id;             // unique ID for tracking in logs
    UeE2RcControl ue[MAX_UE_NUM];
} __attribute__((packed));

// == E3SM INDICATION STRUCTURE ==================================================
struct UeE3Indication
{
    uint16_t rnti;              // Radio Network Temporary Identifier
    uint8_t cqi;                // Channel Quality Indicator (0-15)
    uint32_t bufferLevel;       // bytes waiting in buffer
    uint8_t contentCriticality; // criticality of data in buffer (1-10)
    char contentId[MAX_ID_LEN]; // object ID the UE observes
    uint8_t qci;                // QoS Class Identifier (1-9)
    double achievableRate;      // achievable rate [Mbps]
    double actualThroughput;    // throughput [Mbps]
    uint32_t totalBytesServed;  // total bytes served so far
    uint32_t packetDelay;       // current packet delay [ms]
} __attribute__((packed));

struct E3Indication
{
    uint16_t type;                 // E3SM message type
    uint64_t timestamp;            // current simulation time [ns]
    uint32_t subframeNo;           // current subframe number (0-9)
    uint32_t frameNo;              // current frame number
    uint32_t numUe;                // number of active UEs
    uint32_t totalBufferLevel;     // sum of all UE buffers
    uint16_t initRbg;              // first available RBG index
    uint16_t numRbg;               // number of available RBGs
    int rbgSize;                   // RBG size
    uint8_t linkDirection;         // DL: 0, UL: 1
    uint32_t id;                   // unique ID for tracking in logs
    UeE3Indication ue[MAX_UE_NUM]; // array of UE states
} __attribute__((packed));

// == E3SM CONTROL STRUCTURE ====================================================
struct UeE3Control
{
    uint16_t rnti;             // which UE to serve
    uint16_t initAllocatedRbg; // starting RBG index
    uint16_t numAllocatedRbg;  // number of consecutive RBG
    uint8_t mcs;               // Modulation and Coding Scheme (0-28)
} __attribute__((packed));

struct E3Control
{
    uint16_t type;         // E3SM message type
    uint32_t ueSchedSize;  // number of UEs scheduled
    bool skipThisStep;     // don't schedule anything
    uint8_t linkDirection; // DL: 0, UL: 1
    uint32_t id;           // unique ID for tracking in logs
    UeE3Control ue[MAX_UE_NUM];
} __attribute__((packed));

// == SHARED MEMORY INTERFACE STRUCTURE =========================================
struct Cpp2Py
{
    E3Indication e3Indication;
    E2KpmReport e2KpmReport;
} __attribute__((packed));

struct Py2Cpp
{
    E3Control e3Control;
    E2RcControl e2RcControl;
} __attribute__((packed));

static constexpr uint32_t SHM_SIZE =
    sizeof(Cpp2Py) + sizeof(Py2Cpp) +
    512; // extra buffer for future extensions or metadata if needed

} // namespace ns3

#endif // IBORAN_SHM_STRUCTURE_H
