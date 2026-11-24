# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from dataclasses import dataclass, field
from typing import List
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    MessageType,
    RanFunctionId,
)


# == E2SM-DAPP INDICATION STRUCTURE
# ===============================================================
@dataclass
class UeE2DAppIndication:
    rnti: int = 0
    ueId: int = 0  # IMSI, 0 if not known yet
    numAllocatedRbg: int = 0  # RBGs granted in the last TTI or slot
    sumAllocatedRbg: int = 0  # RBGs granted since the last xApp decision
    numScheduledTti: int = 0  # TTIs or slots scheduled since the last xApp decision


@dataclass
class E2DAppIndication:  # from dApp to xApp
    type: int = MessageType.E2SM_DAPP_INDICATION
    dAppId: int = 0
    ranFunctionId: int = RanFunctionId.DAPP
    indicationSn: int = 0
    ueSchedSize: int = 0
    totalNumAllocatedRbg: int = 0
    rbgSize: int = 1
    linkDirection: int = LinkDirection.DOWNLINK
    ue: List[UeE2DAppIndication] = field(default_factory=list)


# == E2SM-DAPP CONTROL REQUEST STRUCTURE
# ===============================================================
@dataclass
class UeE2DAppControlRequest:
    rnti: int = 0  # last known RNTI, for the dApp learning state
    ueId: int = 0  # IMSI the bound applies to
    maxRbgAllocation: int = 0


@dataclass
class E2DAppControlRequest:  # from xApp to dApp
    type: int = MessageType.E2SM_DAPP_CONTROL_REQUEST
    ricRequestorId: int = 0
    ricInstanceId: int = 0
    ranFunctionId: int = RanFunctionId.DAPP
    requestSn: int = 0
    maxUeSchedSize: int = 0
    numAvailableRbg: int = 0
    skipThisStep: bool = False
    linkDirection: int = LinkDirection.DOWNLINK
    decisionId: int = 0  # incremented at every xApp decision, including open-link
    ue: List[UeE2DAppControlRequest] = field(default_factory=list)


@dataclass
class E2DappInterface:
    indication: E2DAppIndication = field(default_factory=E2DAppIndication)
    control: E2DAppControlRequest = field(default_factory=E2DAppControlRequest)
