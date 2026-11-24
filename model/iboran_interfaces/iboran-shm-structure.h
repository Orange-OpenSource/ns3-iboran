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

// constants for shared memory structure sizes
#define MAX_UE_NUM 32
#define MAX_PRB_NUM_LTE 100
#define MAX_PRB_NUM_NR 264
#define MAX_ID_LEN 32

// message types (E2SM-DAPP codes 0x21 and 0x22 never cross the shared memory)
#define E3SM_INDICATION 0x01
#define E3SM_CONTROL_REQUEST 0x02
#define E2SM_KPM_INDICATION 0x11
#define E2SM_RC_CONTROL_REQUEST 0x12

// RAN function IDs announced in E2 SETUP
#define RAN_FUNCTION_ID_KPM 1
#define RAN_FUNCTION_ID_RC 2
#define RAN_FUNCTION_ID_DAPP 3

// exchange phase of Cpp2Py and Py2Cpp
#define PHASE_SETUP 0
#define PHASE_RUN 1

// control outcome causes (CAUSE_NONE: acknowledged)
#define CAUSE_NONE 0
#define CAUSE_INVALID_TYPE 1
#define CAUSE_WRONG_DIRECTION 2
#define CAUSE_INVALID_RNTI 3
#define CAUSE_INVALID_MCS 4
#define CAUSE_RBG_OUT_OF_RANGE 5
#define CAUSE_INVALID_RAN_FUNCTION 6

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

// == E2SM-KPM RIC INDICATION STRUCTURE
// =========================================================
struct UeE2KpmIndication
{
    uint16_t rnti;              // Radio Network Temporary Identifier
    uint64_t ueId;              // UE ID (IMSI), 0 if not known yet
    uint8_t qosId;              // measurement label: QCI (LTE, 1-9) or 5QI (NR)
    double drbUeThp;            // actual throughput [kbps], 100 ms moving average
    uint32_t drbRlcSduVolume;   // Data volume served [kbit] in the period
    uint8_t cqi;                // extension: Channel Quality Indicator (0-15)
    uint32_t bufferSize;        // extension: bytes waiting in buffer
    char contentId[MAX_ID_LEN]; // extension: object ID the UE observes
    uint32_t packetDelay;       // extension: head-of-line delay [ms]
} __attribute__((packed));

struct E2KpmIndication
{
    uint16_t type;                    // E2SM message type
    uint16_t ricRequestorId;          // RIC request ID: requestor (xApp)
    uint16_t ricInstanceId;           // RIC request ID: subscription instance
    uint16_t ranFunctionId;           // RAN_FUNCTION_ID_KPM
    uint32_t indicationSn;            // RIC indication sequence number
    uint16_t cellId;                  // cell, stand-in for the NR CGI or ECGI
    uint8_t present;                  // 1: reporting period elapsed, 0: no report
    uint64_t collectionStartTime;     // start of the collection period [ns]
    uint32_t granularityPeriod;       // collection period [ms]
    uint32_t numUe;                   // number of active UEs
    uint16_t numRbg;                  // number of available RBGs
    int rbgSize;                      // RBG size [PRB]
    uint8_t linkDirection;            // DL: 0, UL: 1
    UeE2KpmIndication ue[MAX_UE_NUM]; // array of UE states
} __attribute__((packed));

// == E2SM-RC RIC CONTROL REQUEST STRUCTURE
// =========================================================
struct UeE2RcControlRequest
{
    uint64_t ueId;             // UE ID (IMSI) the bound applies to, 0: inert
    uint16_t maxRbgAllocation; // maximum RBG allocation
} __attribute__((packed));

struct E2RcControlRequest
{
    uint16_t type;           // E2SM message type
    uint16_t ricRequestorId; // RIC request ID: requestor (xApp)
    uint16_t ricInstanceId;  // RIC request ID: instance
    uint16_t ranFunctionId;  // RAN_FUNCTION_ID_RC
    uint32_t requestSn;      // ib-ORAN request counter (logs, control outcome)
    uint32_t maxUeSchedSize; // maximum number of UEs scheduled
    bool skipThisStep;       // don't schedule anything
    uint8_t linkDirection;   // DL: 0, UL: 1
    UeE2RcControlRequest ue[MAX_UE_NUM];
} __attribute__((packed));

// == E3SM INDICATION STRUCTURE
// =========================================================
struct UeE3Indication
{
    uint16_t rnti;              // Radio Network Temporary Identifier
    uint64_t ueId;              // UE ID (IMSI), 0 if not known yet
    uint8_t qosId;              // QCI (LTE, 1-9) or 5QI (NR)
    double drbUeThp;            // Actual throughput [kbps], 100 ms moving average
    uint8_t cqi;                // Channel Quality Indicator (0-15)
    uint32_t bufferSize;        // bytes waiting in buffer
    char contentId[MAX_ID_LEN]; // object ID the UE observes
    uint32_t packetDelay;       // head-of-line delay [ms]
    uint8_t contentCriticality; // E3 only: criticality of data in buffer (1-10)
    double achievableRate;      // E3 only: full-band rate at the current MCS [kbps]
} __attribute__((packed));

struct E3Indication
{
    uint16_t type;                 // E3SM message type
    uint16_t dAppId;               // dApp ID from E3 SETUP
    uint32_t indicationSn;         // indication sequence number
    uint16_t cellId;               // cell, stand-in for the NR CGI or ECGI
    uint64_t timestamp;            // current simulation time [ns]
    uint32_t subframeNo;           // current subframe number (0-9)
    uint32_t frameNo;              // current frame number
    uint32_t numUe;                // number of active UEs
    uint32_t totalBufferSize;      // sum of all UE buffers
    uint16_t initRbg;              // first available RBG index
    uint16_t numRbg;               // number of available RBGs
    int rbgSize;                   // RBG size
    uint8_t linkDirection;         // DL: 0, UL: 1
    UeE3Indication ue[MAX_UE_NUM]; // array of UE states
} __attribute__((packed));

// == E3SM CONTROL REQUEST STRUCTURE
// =========================================================
struct UeE3ControlRequest
{
    uint16_t rnti;             // which UE to serve
    uint16_t initAllocatedRbg; // starting RBG index
    uint16_t numAllocatedRbg;  // number of consecutive RBG
    uint8_t mcs;               // Modulation and Coding Scheme (0-28)
} __attribute__((packed));

struct E3ControlRequest
{
    uint16_t type;         // E3SM message type
    uint16_t dAppId;       // dApp ID from E3 SETUP
    uint32_t requestSn;    // request sequence number
    uint32_t ueSchedSize;  // number of UEs scheduled
    bool skipThisStep;     // don't schedule anything
    uint8_t linkDirection; // DL: 0, UL: 1
    UeE3ControlRequest ue[MAX_UE_NUM];
} __attribute__((packed));

// == SETUP AND CONTROL OUTCOME STRUCTURES
// =========================================================
struct SetupRequest // E2 SETUP and E3 SETUP
{
    uint16_t cellId;          // cell of the sending scheduler
    uint32_t ranFunctionMask; // bit (ID - 1) set for each RAN function offered
} __attribute__((packed));

struct SetupResponse // RIC SUBSCRIPTION and E3 SETUP RESPONSE
{
    uint16_t ricRequestorId;    // requestor (xApp)
    uint16_t ricInstanceId;     // subscription instance
    uint32_t kpmReportPeriodMs; // E2SM-KPM event trigger: reporting period
    uint16_t dAppId;            // dApp ID
} __attribute__((packed));

struct ControlOutcome // previous control requests of the same link
{
    uint32_t e3RequestSn;     // E3SM CONTROL REQUEST answered
    uint8_t e3Cause;          // CAUSE_NONE: acknowledged, else the last failure
    uint16_t e3NumRejectedUe; // per-UE allocations not applied
    uint32_t e2RcRequestSn;   // E2SM-RC CONTROL REQUEST answered
    uint8_t e2RcCause;        // CAUSE_NONE: acknowledged
} __attribute__((packed));

// == SHARED MEMORY INTERFACE STRUCTURE
// =========================================================
struct Cpp2Py
{
    uint8_t phase;                 // PHASE_SETUP or PHASE_RUN
    SetupRequest setupRequest;     // valid in PHASE_SETUP
    ControlOutcome controlOutcome; // valid in PHASE_RUN
    E3Indication e3Indication;
    E2KpmIndication e2KpmIndication;
} __attribute__((packed));

struct Py2Cpp
{
    uint8_t phase;               // PHASE_SETUP or PHASE_RUN
    SetupResponse setupResponse; // valid in PHASE_SETUP
    E3ControlRequest e3ControlRequest;
    E2RcControlRequest e2RcControlRequest;
} __attribute__((packed));

static constexpr uint32_t SHM_SIZE =
    sizeof(Cpp2Py) + sizeof(Py2Cpp) +
    512; // extra buffer for future extensions or metadata if needed

// == INITIALIZATION HELPERS
// =========================================================
void InitE2KpmIndication(E2KpmIndication* e2KpmIndication);
void InitE2RcControlRequest(E2RcControlRequest* e2RcControlRequest);
void InitE3Indication(E3Indication* e3Indication);
void InitE3ControlRequest(E3ControlRequest* e3ControlRequest);
void InitCpp2Py(Cpp2Py* cpp2Py);
void InitPy2Cpp(Py2Cpp* py2Cpp);

} // namespace ns3

#endif // IBORAN_SHM_STRUCTURE_H
