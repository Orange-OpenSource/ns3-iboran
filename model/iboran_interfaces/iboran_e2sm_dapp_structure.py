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

E2SM_DAPP_REPORT = 0x21
E2SM_DAPP_CONTROL = 0x22

DOWNLINK = 0
UPLINK = 1


@dataclass
class E2DAppReport:  # from dApp to xApp
    type: int = E2SM_DAPP_REPORT
    ueSchedSize: int = 0
    numAllocatedRbg: int = 0
    rbgSize: int = 1
    linkDirection: int = DOWNLINK
    id: int = 0


@dataclass
class E2DAppControl:  # from xApp to dApp
    type: int = E2SM_DAPP_CONTROL
    maxUeSchedSize: int = 0
    numAvailableRbg: int = 0
    skipThisStep: bool = False
    linkDirection: int = DOWNLINK
    id: int = 0


@dataclass
class E2DappInterface:
    report: E2DAppReport = field(default_factory=E2DAppReport)
    control: E2DAppControl = field(default_factory=E2DAppControl)
