# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from ctypes import *

MAX_UE_NUM = 32
MAX_PRB_NUM_LTE = 100
MAX_ID_LEN = 64

E3SM_INDICATION = 0x01
E3SM_CONTROL = 0x02

E2SM_KPM_REPORT = 0x11
E2SM_RC_CONTROL = 0x12

LTE = "LTE"

DOWNLINK = 0
UPLINK = 1


# == E2SM-KPM REPORT STRUCTURE =======================================
# make sure to keep in sync with 'iboran-e2-structure.h'


class UeE2KpmReport(Structure):
    _pack_ = 1  # ensure no padding
    _fields_ = [
        ("rnti", c_uint16),
        ("cqi", c_uint8),
        ("bufferLevel", c_uint32),
        ("contentId", c_char * MAX_ID_LEN),
        ("qci", c_uint8),
        ("achievableRate", c_double),
        ("actualThroughput", c_double),
        ("totalBytesServed", c_uint32),
        ("packetDelay", c_uint32),
    ]


class E2KpmReport(Structure):
    _pack_ = 1
    _fields_ = [
        ("type", c_uint16),
        ("timestamp", c_uint64),
        ("frameNo", c_uint32),
        ("numUe", c_uint32),
        ("totalBufferLevel", c_uint32),
        ("numRbg", c_uint16),
        ("rbgSize", c_int),
        ("linkDirection", c_uint8),
        ("id", c_uint32),
        ("ue", UeE2KpmReport * MAX_UE_NUM),
    ]


# == E2SM-RC CONTROL STRUCTURE =======================================
# make sure to keep in sync with 'iboran-e2-structure.h'
class UeE2RcControl(Structure):
    _pack_ = 1
    _fields_ = [
        ("rnti", c_uint16),
        ("maxRbgAllocation", c_uint16),
    ]


class E2RcControl(Structure):
    _pack_ = 1
    _fields_ = [
        ("type", c_uint16),
        ("maxUeSchedSize", c_uint32),
        ("skipThisStep", c_bool),
        ("linkDirection", c_uint8),
        ("id", c_uint32),
        ("ue", UeE2RcControl * MAX_UE_NUM),
    ]


# == E3SM INDICATION STRUCTURE ========================================
# make sure to keep in sync with 'iboran-shm-structure.h'


class UeE3Indication(Structure):
    _pack_ = 1  # ensure no padding
    _fields_ = [
        ("rnti", c_uint16),
        ("cqi", c_uint8),
        ("bufferLevel", c_uint32),
        ("contentCriticality", c_uint8),
        ("contentId", c_char * MAX_ID_LEN),
        ("qci", c_uint8),
        ("achievableRate", c_double),
        ("actualThroughput", c_double),
        ("totalBytesServed", c_uint32),
        ("packetDelay", c_uint32),
    ]


class E3Indication(Structure):
    _pack_ = 1
    _fields_ = [
        ("type", c_uint16),
        ("timestamp", c_uint64),
        ("subframeNo", c_uint32),
        ("frameNo", c_uint32),
        ("numUe", c_uint32),
        ("totalBufferLevel", c_uint32),
        ("initRbg", c_uint16),
        ("numRbg", c_uint16),
        ("rbgSize", c_int),
        ("linkDirection", c_uint8),
        ("id", c_uint32),
        ("ue", UeE3Indication * MAX_UE_NUM),
    ]


# == E3SM CONTROL STRUCTURE ==========================================
# make sure to keep in sync with 'iboran-shm-structure.h'
class UeE3Control(Structure):
    _pack_ = 1
    _fields_ = [
        ("rnti", c_uint16),
        ("initAllocatedRbg", c_uint16),
        ("numAllocatedRbg", c_uint16),
        ("mcs", c_uint8),
    ]


class E3Control(Structure):
    _pack_ = 1
    _fields_ = [
        ("type", c_uint16),
        ("ueSchedSize", c_uint32),
        ("skipThisStep", c_bool),
        ("linkDirection", c_uint8),
        ("id", c_uint32),
        ("ue", UeE3Control * MAX_UE_NUM),
    ]


# == SHARED MEMORY STRUCTURE =========================================
class Cpp2Py(Structure):
    _pack_ = 1
    _fields_ = [
        ("e3Indication", E3Indication),
        ("e2KpmReport", E2KpmReport),
    ]


class Py2Cpp(Structure):
    _pack_ = 1
    _fields_ = [
        ("e3Control", E3Control),
        ("e2RcControl", E2RcControl),
    ]


SHM_SIZE = (
    sizeof(Cpp2Py) + sizeof(Py2Cpp) + 512
)  # extra buffer for future extensions or metadata if needed

# == ALIASING ========================================================
# required by ns3ai_utils
Cpp2PyStruct = Cpp2Py
Py2CppStruct = Py2Cpp
