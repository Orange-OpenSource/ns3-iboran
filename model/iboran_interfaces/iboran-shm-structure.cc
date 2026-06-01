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

#include "iboran-shm-structure.h"

#include "ns3/log.h"

#include <cstring>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("IbOranShmStructure");

// == HELPER FUNCTION FOR E2SM-KPM REPORT STRUCTURE =============================
void
InitE2KpmReport(E2KpmReport* e2KpmReport)
{
    if (!e2KpmReport)
    {
        return;
    }

    // zero out the entire structure
    std::memset(e2KpmReport, 0, sizeof(E2KpmReport));

    // set default values
    e2KpmReport->type = E2SM_KPM_REPORT;
    e2KpmReport->timestamp = 0;
    e2KpmReport->frameNo = 0;
    e2KpmReport->numUe = 0;
    e2KpmReport->totalBufferLevel = 0;
    e2KpmReport->numRbg = 0;
    e2KpmReport->rbgSize = 1;
    e2KpmReport->linkDirection = DOWNLINK;
    e2KpmReport->id = 0;

    for (uint32_t i = 0; i < MAX_UE_NUM; ++i)
    {
        e2KpmReport->ue[i].rnti = 0;
        e2KpmReport->ue[i].cqi = 0;
        e2KpmReport->ue[i].bufferLevel = 0;
        std::snprintf(e2KpmReport->ue[i].contentId, MAX_ID_LEN, "%s", "null");
        e2KpmReport->ue[i].qci = 9;
        e2KpmReport->ue[i].achievableRate = 0.0;
        e2KpmReport->ue[i].actualThroughput = 0.0;
        e2KpmReport->ue[i].totalBytesServed = 0;
        e2KpmReport->ue[i].packetDelay = 0;
    }
}

// == HELPER FUNCTION FOR E2SM CONTROL STRUCTURE ================================
void
InitE2RcControl(E2RcControl* e2RcControl)
{
    if (!e2RcControl)
    {
        return;
    }

    // zero out the entire structure
    std::memset(e2RcControl, 0, sizeof(E2RcControl));

    // set default values
    e2RcControl->type = E2SM_RC_CONTROL;
    e2RcControl->maxUeSchedSize = 0;
    e2RcControl->skipThisStep = false;
    e2RcControl->linkDirection = DOWNLINK;
    e2RcControl->id = 0;

    for (uint32_t i = 0; i < MAX_UE_NUM; ++i)
    {
        e2RcControl->ue[i].rnti = 0;
        e2RcControl->ue[i].maxRbgAllocation = 0;
    }
};

// == HELPER FUNCTION FOR E3SM INDICATION STRUCTURE ==============================
void
InitE3Indication(E3Indication* e3Indication)
{
    if (!e3Indication)
    {
        return;
    }

    // zero out the entire structure
    std::memset(e3Indication, 0, sizeof(E3Indication));

    // set default values
    e3Indication->type = E3SM_INDICATION;
    e3Indication->timestamp = 0;
    e3Indication->subframeNo = 0;
    e3Indication->frameNo = 0;
    e3Indication->numUe = 0;
    e3Indication->totalBufferLevel = 0;
    e3Indication->initRbg = 0;
    e3Indication->numRbg = 0;
    e3Indication->rbgSize = 1;
    e3Indication->linkDirection = DOWNLINK;
    e3Indication->id = 0;

    for (uint32_t i = 0; i < MAX_UE_NUM; ++i)
    {
        e3Indication->ue[i].rnti = 0;
        e3Indication->ue[i].cqi = 0;
        e3Indication->ue[i].bufferLevel = 0;
        e3Indication->ue[i].contentCriticality = 10;
        std::snprintf(e3Indication->ue[i].contentId, MAX_ID_LEN, "%s", "null");
        e3Indication->ue[i].qci = 9;
        e3Indication->ue[i].achievableRate = 0.0;
        e3Indication->ue[i].actualThroughput = 0.0;
        e3Indication->ue[i].totalBytesServed = 0;
        e3Indication->ue[i].packetDelay = 0;
    }
}

// == HELPER FUNCTION FOR E3SM CONTROL STRUCTURE ================================
void
InitE3Control(E3Control* e3Control)
{
    if (!e3Control)
    {
        return;
    }

    // zero out the entire structure
    std::memset(e3Control, 0, sizeof(E3Control));

    // set default values
    e3Control->type = E3SM_CONTROL;
    e3Control->ueSchedSize = 0;
    e3Control->skipThisStep = true;
    e3Control->linkDirection = DOWNLINK;
    e3Control->id = 0;

    for (uint32_t i = 0; i < MAX_UE_NUM; ++i)
    {
        e3Control->ue[i].rnti = 0;
        e3Control->ue[i].initAllocatedRbg = 0;
        e3Control->ue[i].numAllocatedRbg = 0;
        e3Control->ue[i].mcs = 0;
    }
};

// == HELPER FUNCTION FOR SHARED MEMORY INTERFACE STRUCTURE =====================
void
InitCpp2Py(Cpp2Py* cpp2Py)
{
    if (!cpp2Py)
    {
        return;
    }
    InitE3Indication(&cpp2Py->e3Indication);
    InitE2KpmReport(&cpp2Py->e2KpmReport);
}

void
InitPy2Cpp(Py2Cpp* py2Cpp)
{
    if (!py2Cpp)
    {
        return;
    }
    InitE3Control(&py2Cpp->e3Control);
    InitE2RcControl(&py2Cpp->e2RcControl);
}

} // namespace ns3
