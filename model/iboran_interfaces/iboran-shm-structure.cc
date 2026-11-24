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

#include <cstdio>
#include <cstring>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("IbOranShmStructure");

// == E2SM-KPM RIC INDICATION STRUCTURE
// =========================================================
void
InitE2KpmIndication(E2KpmIndication* e2KpmIndication)
{
    if (!e2KpmIndication)
    {
        return;
    }

    // zero out the entire structure, then set the non-zero defaults
    std::memset(e2KpmIndication, 0, sizeof(E2KpmIndication));
    e2KpmIndication->type = E2SM_KPM_INDICATION;
    e2KpmIndication->ranFunctionId = RAN_FUNCTION_ID_KPM;
    e2KpmIndication->rbgSize = 1;

    for (uint32_t i = 0; i < MAX_UE_NUM; ++i)
    {
        std::snprintf(e2KpmIndication->ue[i].contentId, MAX_ID_LEN, "%s", "null");
        e2KpmIndication->ue[i].qosId = 9;
    }
}

// == E2SM-RC RIC CONTROL REQUEST STRUCTURE
// =========================================================
void
InitE2RcControlRequest(E2RcControlRequest* e2RcControlRequest)
{
    if (!e2RcControlRequest)
    {
        return;
    }

    // zero out the entire structure, then set the non-zero defaults
    std::memset(e2RcControlRequest, 0, sizeof(E2RcControlRequest));
    e2RcControlRequest->type = E2SM_RC_CONTROL_REQUEST;
    e2RcControlRequest->ranFunctionId = RAN_FUNCTION_ID_RC;
}

// == E3SM INDICATION STRUCTURE
// =========================================================
void
InitE3Indication(E3Indication* e3Indication)
{
    if (!e3Indication)
    {
        return;
    }

    // zero out the entire structure, then set the non-zero defaults
    std::memset(e3Indication, 0, sizeof(E3Indication));
    e3Indication->type = E3SM_INDICATION;
    e3Indication->rbgSize = 1;

    for (uint32_t i = 0; i < MAX_UE_NUM; ++i)
    {
        e3Indication->ue[i].contentCriticality = 10;
        std::snprintf(e3Indication->ue[i].contentId, MAX_ID_LEN, "%s", "null");
        e3Indication->ue[i].qosId = 9;
    }
}

// == E3SM CONTROL REQUEST STRUCTURE
// =========================================================
void
InitE3ControlRequest(E3ControlRequest* e3ControlRequest)
{
    if (!e3ControlRequest)
    {
        return;
    }

    // zero out the entire structure, then set the non-zero defaults
    std::memset(e3ControlRequest, 0, sizeof(E3ControlRequest));
    e3ControlRequest->type = E3SM_CONTROL_REQUEST;
    e3ControlRequest->skipThisStep = true;
}

// == SHARED MEMORY INTERFACE STRUCTURE
// =========================================================
void
InitCpp2Py(Cpp2Py* cpp2Py)
{
    if (!cpp2Py)
    {
        return;
    }
    std::memset(cpp2Py, 0, sizeof(Cpp2Py));
    cpp2Py->phase = PHASE_RUN;
    InitE3Indication(&cpp2Py->e3Indication);
    InitE2KpmIndication(&cpp2Py->e2KpmIndication);
}

void
InitPy2Cpp(Py2Cpp* py2Cpp)
{
    if (!py2Cpp)
    {
        return;
    }
    std::memset(py2Cpp, 0, sizeof(Py2Cpp));
    py2Cpp->phase = PHASE_RUN;
    InitE3ControlRequest(&py2Cpp->e3ControlRequest);
    InitE2RcControlRequest(&py2Cpp->e2RcControlRequest);
}

} // namespace ns3
