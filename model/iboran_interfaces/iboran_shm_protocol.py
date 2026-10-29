# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from enum import IntEnum


class LinkDirection(IntEnum):
    DOWNLINK = 0
    UPLINK = 1

    def __str__(self) -> str:
        return self.name

    def __format__(self, format_spec: str) -> str:
        return self.name


class RatType:
    LTE = "LTE"
    NR = "NR"


class MessageType(IntEnum):
    E3SM_INDICATION = 0x01
    E3SM_CONTROL = 0x02
    E2SM_KPM_REPORT = 0x11
    E2SM_RC_CONTROL = 0x12
    E2SM_DAPP_REPORT = 0x21
    E2SM_DAPP_CONTROL = 0x22


class ProtocolLimits:
    MAX_UE_NUM = 32
    MAX_PRB_NUM_LTE = 100
    MAX_PRB_NUM_NR = 264
    MAX_ID_LEN = 64
    SUBFRAMES_PER_FRAME = 10
    FRAME_NO_WRAP = 1024
