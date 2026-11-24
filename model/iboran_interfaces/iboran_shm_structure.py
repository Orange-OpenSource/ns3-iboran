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
from model.iboran_interfaces.iboran_shm_protocol import ProtocolLimits


# == E2SM-KPM RIC INDICATION STRUCTURE
# =========================================================
# make sure to keep in sync with 'iboran-shm-structure.h'
class UeE2KpmIndication(Structure):
    _pack_ = 1  # ensure no padding
    _fields_ = [
        ("rnti", c_uint16),
        ("ueId", c_uint64),
        ("qosId", c_uint8),
        ("drbUeThp", c_double),
        ("drbRlcSduVolume", c_uint32),
        ("cqi", c_uint8),
        ("bufferSize", c_uint32),
        ("contentId", c_char * ProtocolLimits.MAX_ID_LEN),
        ("packetDelay", c_uint32),
    ]


class E2KpmIndication(Structure):
    _pack_ = 1
    _fields_ = [
        ("type", c_uint16),
        ("ricRequestorId", c_uint16),
        ("ricInstanceId", c_uint16),
        ("ranFunctionId", c_uint16),
        ("indicationSn", c_uint32),
        ("cellId", c_uint16),
        ("present", c_uint8),
        ("collectionStartTime", c_uint64),
        ("granularityPeriod", c_uint32),
        ("numUe", c_uint32),
        ("numRbg", c_uint16),
        ("rbgSize", c_int),
        ("linkDirection", c_uint8),
        ("ue", UeE2KpmIndication * ProtocolLimits.MAX_UE_NUM),
    ]


# == E2SM-RC RIC CONTROL REQUEST STRUCTURE
# =========================================================
# make sure to keep in sync with 'iboran-shm-structure.h'
class UeE2RcControlRequest(Structure):
    _pack_ = 1
    _fields_ = [
        ("ueId", c_uint64),
        ("maxRbgAllocation", c_uint16),
    ]


class E2RcControlRequest(Structure):
    _pack_ = 1
    _fields_ = [
        ("type", c_uint16),
        ("ricRequestorId", c_uint16),
        ("ricInstanceId", c_uint16),
        ("ranFunctionId", c_uint16),
        ("requestSn", c_uint32),
        ("maxUeSchedSize", c_uint32),
        ("skipThisStep", c_bool),
        ("linkDirection", c_uint8),
        ("ue", UeE2RcControlRequest * ProtocolLimits.MAX_UE_NUM),
    ]


# == E3SM INDICATION STRUCTURE
# =========================================================
# make sure to keep in sync with 'iboran-shm-structure.h'
class UeE3Indication(Structure):
    _pack_ = 1  # ensure no padding
    _fields_ = [
        ("rnti", c_uint16),
        ("ueId", c_uint64),
        ("qosId", c_uint8),
        ("drbUeThp", c_double),
        ("cqi", c_uint8),
        ("bufferSize", c_uint32),
        ("contentId", c_char * ProtocolLimits.MAX_ID_LEN),
        ("packetDelay", c_uint32),
        ("contentCriticality", c_uint8),
        ("achievableRate", c_double),
    ]


class E3Indication(Structure):
    _pack_ = 1
    _fields_ = [
        ("type", c_uint16),
        ("dAppId", c_uint16),
        ("indicationSn", c_uint32),
        ("cellId", c_uint16),
        ("timestamp", c_uint64),
        ("subframeNo", c_uint32),
        ("frameNo", c_uint32),
        ("numUe", c_uint32),
        ("totalBufferSize", c_uint32),
        ("initRbg", c_uint16),
        ("numRbg", c_uint16),
        ("rbgSize", c_int),
        ("linkDirection", c_uint8),
        ("ue", UeE3Indication * ProtocolLimits.MAX_UE_NUM),
    ]


# == E3SM CONTROL REQUEST STRUCTURE
# =========================================================
# make sure to keep in sync with 'iboran-shm-structure.h'
class UeE3ControlRequest(Structure):
    _pack_ = 1
    _fields_ = [
        ("rnti", c_uint16),
        ("initAllocatedRbg", c_uint16),
        ("numAllocatedRbg", c_uint16),
        ("mcs", c_uint8),
    ]


class E3ControlRequest(Structure):
    _pack_ = 1
    _fields_ = [
        ("type", c_uint16),
        ("dAppId", c_uint16),
        ("requestSn", c_uint32),
        ("ueSchedSize", c_uint32),
        ("skipThisStep", c_bool),
        ("linkDirection", c_uint8),
        ("ue", UeE3ControlRequest * ProtocolLimits.MAX_UE_NUM),
    ]


# == SETUP AND CONTROL OUTCOME STRUCTURES
# =========================================================
# make sure to keep in sync with 'iboran-shm-structure.h'
class SetupRequest(Structure):
    _pack_ = 1
    _fields_ = [
        ("cellId", c_uint16),
        ("ranFunctionMask", c_uint32),
    ]


class SetupResponse(Structure):
    _pack_ = 1
    _fields_ = [
        ("ricRequestorId", c_uint16),
        ("ricInstanceId", c_uint16),
        ("kpmReportPeriodMs", c_uint32),
        ("dAppId", c_uint16),
    ]


class ControlOutcome(Structure):
    _pack_ = 1
    _fields_ = [
        ("e3RequestSn", c_uint32),
        ("e3Cause", c_uint8),
        ("e3NumRejectedUe", c_uint16),
        ("e2RcRequestSn", c_uint32),
        ("e2RcCause", c_uint8),
    ]


# == SHARED MEMORY STRUCTURE =========================================
class Cpp2Py(Structure):
    _pack_ = 1
    _fields_ = [
        ("phase", c_uint8),
        ("setupRequest", SetupRequest),
        ("controlOutcome", ControlOutcome),
        ("e3Indication", E3Indication),
        ("e2KpmIndication", E2KpmIndication),
    ]


class Py2Cpp(Structure):
    _pack_ = 1
    _fields_ = [
        ("phase", c_uint8),
        ("setupResponse", SetupResponse),
        ("e3ControlRequest", E3ControlRequest),
        ("e2RcControlRequest", E2RcControlRequest),
    ]


SHM_SIZE = (
    sizeof(Cpp2Py) + sizeof(Py2Cpp) + 512
)  # extra buffer for future extensions or metadata if needed


# == ALIASING ========================================================
# required by ns3ai_utils

Cpp2PyStruct = Cpp2Py
Py2CppStruct = Py2Cpp
