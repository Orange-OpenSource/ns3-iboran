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

#include <ns3/ai-module.h>

#include <algorithm>
#include <cstdio>
#include <pybind11/pybind11.h>
#include <string>

using namespace ns3;

namespace py = pybind11;

PYBIND11_MODULE(iboran_shm_structure_py_binding, m)
{
    // == E2SM-KPM RIC INDICATION STRUCTURE
    // ===============================================================
    py::class_<ns3::UeE2KpmIndication>(m, "UeE2KpmIndication")
        .def(py::init<>())
        .def_readwrite("rnti", &ns3::UeE2KpmIndication::rnti)
        .def_readwrite("ueId", &ns3::UeE2KpmIndication::ueId)
        .def_readwrite("qosId", &ns3::UeE2KpmIndication::qosId)
        .def_readwrite("drbUeThp", &ns3::UeE2KpmIndication::drbUeThp)
        .def_readwrite("drbRlcSduVolume", &ns3::UeE2KpmIndication::drbRlcSduVolume)
        .def_readwrite("cqi", &ns3::UeE2KpmIndication::cqi)
        .def_readwrite("bufferSize", &ns3::UeE2KpmIndication::bufferSize)
        .def_property(
            "contentId",
            [](const ns3::UeE2KpmIndication& ue) { return std::string(ue.contentId); },
            [](ns3::UeE2KpmIndication& ue, const std::string& value) {
                std::snprintf(ue.contentId, MAX_ID_LEN, "%s", value.c_str());
            })
        .def_readwrite("packetDelay", &ns3::UeE2KpmIndication::packetDelay);

    py::class_<ns3::E2KpmIndication>(m, "E2KpmIndication")
        .def(py::init<>())
        .def_readwrite("type", &ns3::E2KpmIndication::type)
        .def_readwrite("ricRequestorId", &ns3::E2KpmIndication::ricRequestorId)
        .def_readwrite("ricInstanceId", &ns3::E2KpmIndication::ricInstanceId)
        .def_readwrite("ranFunctionId", &ns3::E2KpmIndication::ranFunctionId)
        .def_readwrite("indicationSn", &ns3::E2KpmIndication::indicationSn)
        .def_readwrite("cellId", &ns3::E2KpmIndication::cellId)
        .def_readwrite("present", &ns3::E2KpmIndication::present)
        .def_readwrite("collectionStartTime", &ns3::E2KpmIndication::collectionStartTime)
        .def_readwrite("granularityPeriod", &ns3::E2KpmIndication::granularityPeriod)
        .def_readwrite("numUe", &ns3::E2KpmIndication::numUe)
        .def_readwrite("numRbg", &ns3::E2KpmIndication::numRbg)
        .def_readwrite("rbgSize", &ns3::E2KpmIndication::rbgSize)
        .def_readwrite("linkDirection", &ns3::E2KpmIndication::linkDirection)
        .def_property(
            "ue",
            [](ns3::E2KpmIndication& self) {
                py::list ue_e2_list;
                for (size_t i = 0; i < MAX_UE_NUM; ++i)
                {
                    ue_e2_list.append(py::cast(self.ue[i],
                                               py::return_value_policy::reference_internal,
                                               py::cast(&self)) // parent
                    );
                }
                return ue_e2_list;
            },
            [](ns3::E2KpmIndication& self, py::list ueE2List) {
                for (size_t i = 0; i < std::min<size_t>(ueE2List.size(), MAX_UE_NUM); ++i)
                {
                    self.ue[i] = ueE2List[i].cast<ns3::UeE2KpmIndication>();
                }
            });

    // == E2SM-RC RIC CONTROL REQUEST STRUCTURE
    // ===============================================================
    py::class_<ns3::UeE2RcControlRequest>(m, "UeE2RcControlRequest")
        .def(py::init<>())
        .def_readwrite("ueId", &ns3::UeE2RcControlRequest::ueId)
        .def_readwrite("maxRbgAllocation", &ns3::UeE2RcControlRequest::maxRbgAllocation);

    py::class_<ns3::E2RcControlRequest>(m, "E2RcControlRequest")
        .def(py::init<>())
        .def_readwrite("type", &ns3::E2RcControlRequest::type)
        .def_readwrite("ricRequestorId", &ns3::E2RcControlRequest::ricRequestorId)
        .def_readwrite("ricInstanceId", &ns3::E2RcControlRequest::ricInstanceId)
        .def_readwrite("ranFunctionId", &ns3::E2RcControlRequest::ranFunctionId)
        .def_readwrite("requestSn", &ns3::E2RcControlRequest::requestSn)
        .def_readwrite("maxUeSchedSize", &ns3::E2RcControlRequest::maxUeSchedSize)
        .def_readwrite("skipThisStep", &ns3::E2RcControlRequest::skipThisStep)
        .def_readwrite("linkDirection", &ns3::E2RcControlRequest::linkDirection)
        .def_property(
            "ue",
            [](ns3::E2RcControlRequest& self) {
                py::list ue_rc_list;
                for (size_t i = 0; i < MAX_UE_NUM; ++i)
                {
                    // Pass `self` as the owner
                    ue_rc_list.append(py::cast(self.ue[i],
                                               py::return_value_policy::reference_internal,
                                               py::cast(&self)));
                }
                return ue_rc_list;
            },
            [](ns3::E2RcControlRequest& self, py::list ueRcList) {
                for (size_t i = 0; i < std::min<size_t>(ueRcList.size(), MAX_UE_NUM); ++i)
                {
                    self.ue[i] = ueRcList[i].cast<ns3::UeE2RcControlRequest>();
                }
            });

    // == E3SM INDICATION STRUCTURE
    // ===============================================================
    py::class_<ns3::UeE3Indication>(m, "UeE3Indication")
        .def(py::init<>())
        .def_readwrite("rnti", &ns3::UeE3Indication::rnti)
        .def_readwrite("ueId", &ns3::UeE3Indication::ueId)
        .def_readwrite("qosId", &ns3::UeE3Indication::qosId)
        .def_readwrite("drbUeThp", &ns3::UeE3Indication::drbUeThp)
        .def_readwrite("cqi", &ns3::UeE3Indication::cqi)
        .def_readwrite("bufferSize", &ns3::UeE3Indication::bufferSize)
        .def_property(
            "contentId",
            [](const ns3::UeE3Indication& ue) { return std::string(ue.contentId); },
            [](ns3::UeE3Indication& ue, const std::string& value) {
                std::snprintf(ue.contentId, MAX_ID_LEN, "%s", value.c_str());
            })
        .def_readwrite("packetDelay", &ns3::UeE3Indication::packetDelay)
        .def_readwrite("contentCriticality", &ns3::UeE3Indication::contentCriticality)
        .def_readwrite("achievableRate", &ns3::UeE3Indication::achievableRate);

    py::class_<ns3::E3Indication>(m, "E3Indication")
        .def(py::init<>())
        .def_readwrite("type", &ns3::E3Indication::type)
        .def_readwrite("dAppId", &ns3::E3Indication::dAppId)
        .def_readwrite("indicationSn", &ns3::E3Indication::indicationSn)
        .def_readwrite("cellId", &ns3::E3Indication::cellId)
        .def_readwrite("timestamp", &ns3::E3Indication::timestamp)
        .def_readwrite("subframeNo", &ns3::E3Indication::subframeNo)
        .def_readwrite("frameNo", &ns3::E3Indication::frameNo)
        .def_readwrite("numUe", &ns3::E3Indication::numUe)
        .def_readwrite("totalBufferSize", &ns3::E3Indication::totalBufferSize)
        .def_readwrite("initRbg", &ns3::E3Indication::initRbg)
        .def_readwrite("numRbg", &ns3::E3Indication::numRbg)
        .def_readwrite("rbgSize", &ns3::E3Indication::rbgSize)
        .def_readwrite("linkDirection", &ns3::E3Indication::linkDirection)
        .def_property(
            "ue",
            [](ns3::E3Indication& self) {
                py::list ue_e3_list;
                for (size_t i = 0; i < MAX_UE_NUM; ++i)
                {
                    ue_e3_list.append(py::cast(self.ue[i],
                                               py::return_value_policy::reference_internal,
                                               py::cast(&self)) // parent
                    );
                }
                return ue_e3_list;
            },
            [](ns3::E3Indication& self, py::list ueE3List) {
                for (size_t i = 0; i < std::min<size_t>(ueE3List.size(), MAX_UE_NUM); ++i)
                {
                    self.ue[i] = ueE3List[i].cast<ns3::UeE3Indication>();
                }
            });

    // == E3SM CONTROL REQUEST STRUCTURE
    // ===============================================================
    py::class_<ns3::UeE3ControlRequest>(m, "UeE3ControlRequest")
        .def(py::init<>())
        .def_readwrite("rnti", &ns3::UeE3ControlRequest::rnti)
        .def_readwrite("initAllocatedRbg", &ns3::UeE3ControlRequest::initAllocatedRbg)
        .def_readwrite("numAllocatedRbg", &ns3::UeE3ControlRequest::numAllocatedRbg)
        .def_readwrite("mcs", &ns3::UeE3ControlRequest::mcs);

    py::class_<ns3::E3ControlRequest>(m, "E3ControlRequest")
        .def(py::init<>())
        .def_readwrite("type", &ns3::E3ControlRequest::type)
        .def_readwrite("dAppId", &ns3::E3ControlRequest::dAppId)
        .def_readwrite("requestSn", &ns3::E3ControlRequest::requestSn)
        .def_readwrite("ueSchedSize", &ns3::E3ControlRequest::ueSchedSize)
        .def_readwrite("skipThisStep", &ns3::E3ControlRequest::skipThisStep)
        .def_readwrite("linkDirection", &ns3::E3ControlRequest::linkDirection)
        .def_property(
            "ue",
            [](ns3::E3ControlRequest& self) {
                py::list ue_rc_list;
                for (size_t i = 0; i < MAX_UE_NUM; ++i)
                {
                    // Pass `self` as the owner
                    ue_rc_list.append(py::cast(self.ue[i],
                                               py::return_value_policy::reference_internal,
                                               py::cast(&self)));
                }
                return ue_rc_list;
            },
            [](ns3::E3ControlRequest& self, py::list ueRcList) {
                for (size_t i = 0; i < std::min<size_t>(ueRcList.size(), MAX_UE_NUM); ++i)
                {
                    self.ue[i] = ueRcList[i].cast<ns3::UeE3ControlRequest>();
                }
            });

    // == SETUP AND CONTROL OUTCOME STRUCTURES
    // ===============================================================
    py::class_<ns3::SetupRequest>(m, "SetupRequest")
        .def(py::init<>())
        .def_readwrite("cellId", &ns3::SetupRequest::cellId)
        .def_readwrite("ranFunctionMask", &ns3::SetupRequest::ranFunctionMask);

    py::class_<ns3::SetupResponse>(m, "SetupResponse")
        .def(py::init<>())
        .def_readwrite("ricRequestorId", &ns3::SetupResponse::ricRequestorId)
        .def_readwrite("ricInstanceId", &ns3::SetupResponse::ricInstanceId)
        .def_readwrite("kpmReportPeriodMs", &ns3::SetupResponse::kpmReportPeriodMs)
        .def_readwrite("dAppId", &ns3::SetupResponse::dAppId);

    py::class_<ns3::ControlOutcome>(m, "ControlOutcome")
        .def(py::init<>())
        .def_readwrite("e3RequestSn", &ns3::ControlOutcome::e3RequestSn)
        .def_readwrite("e3Cause", &ns3::ControlOutcome::e3Cause)
        .def_readwrite("e3NumRejectedUe", &ns3::ControlOutcome::e3NumRejectedUe)
        .def_readwrite("e2RcRequestSn", &ns3::ControlOutcome::e2RcRequestSn)
        .def_readwrite("e2RcCause", &ns3::ControlOutcome::e2RcCause);

    // == SHARED MEMORY INTERFACE STRUCTURE
    // ===============================================================
    py::class_<ns3::Cpp2Py>(m, "Cpp2Py")
        .def(py::init<>())
        .def_readwrite("phase", &ns3::Cpp2Py::phase)
        .def_property(
            "setupRequest",
            [](ns3::Cpp2Py& self) -> ns3::SetupRequest& { return self.setupRequest; },
            [](ns3::Cpp2Py& self, const ns3::SetupRequest& v) { self.setupRequest = v; },
            py::return_value_policy::reference_internal)
        .def_property(
            "controlOutcome",
            [](ns3::Cpp2Py& self) -> ns3::ControlOutcome& { return self.controlOutcome; },
            [](ns3::Cpp2Py& self, const ns3::ControlOutcome& v) { self.controlOutcome = v; },
            py::return_value_policy::reference_internal)
        .def_property(
            "e3Indication",
            [](ns3::Cpp2Py& self) -> ns3::E3Indication& { return self.e3Indication; },
            [](ns3::Cpp2Py& self, const ns3::E3Indication& v) { self.e3Indication = v; },
            py::return_value_policy::reference_internal)
        .def_property(
            "e2KpmIndication",
            [](ns3::Cpp2Py& self) -> ns3::E2KpmIndication& { return self.e2KpmIndication; },
            [](ns3::Cpp2Py& self, const ns3::E2KpmIndication& v) { self.e2KpmIndication = v; },
            py::return_value_policy::reference_internal);

    py::class_<ns3::Py2Cpp>(m, "Py2Cpp")
        .def(py::init<>())
        .def_readwrite("phase", &ns3::Py2Cpp::phase)
        .def_property(
            "setupResponse",
            [](ns3::Py2Cpp& self) -> ns3::SetupResponse& { return self.setupResponse; },
            [](ns3::Py2Cpp& self, const ns3::SetupResponse& v) { self.setupResponse = v; },
            py::return_value_policy::reference_internal)
        .def_property(
            "e3ControlRequest",
            [](ns3::Py2Cpp& self) -> ns3::E3ControlRequest& { return self.e3ControlRequest; },
            [](ns3::Py2Cpp& self, const ns3::E3ControlRequest& v) { self.e3ControlRequest = v; },
            py::return_value_policy::reference_internal)
        .def_property(
            "e2RcControlRequest",
            [](ns3::Py2Cpp& self) -> ns3::E2RcControlRequest& { return self.e2RcControlRequest; },
            [](ns3::Py2Cpp& self, const ns3::E2RcControlRequest& v) {
                self.e2RcControlRequest = v;
            },
            py::return_value_policy::reference_internal);

    // == MESSAGE INTERFACE STRUCTURE
    // ===============================================================
    using Ns3AiMsgInterfaceType = ns3::Ns3AiMsgInterfaceImpl<ns3::Cpp2Py, ns3::Py2Cpp>;

    py::class_<Ns3AiMsgInterfaceType>(m, "Ns3AiMsgInterfaceImpl")
        .def(py::init<bool,
                      bool,
                      bool,
                      uint32_t,
                      const char*,
                      const char*,
                      const char*,
                      const char*>(),
             py::arg("handleFinish") = true,
             py::arg("useVector") = false,
             py::arg("anotherBool") = false, // placeholder for 3rd bool arg
             py::arg("shmSize") = SHM_SIZE,
             py::arg("segName") = "iboranSegment",
             py::arg("cpp2pyMsgName") = "iboranCpp2Py",
             py::arg("py2cppMsgName") = "iboranPy2Cpp",
             py::arg("lockableName") = "iboran")
        .def("PyRecvBegin", &Ns3AiMsgInterfaceType::PyRecvBegin)
        .def("PyRecvEnd", &Ns3AiMsgInterfaceType::PyRecvEnd)
        .def("PySendBegin", &Ns3AiMsgInterfaceType::PySendBegin)
        .def("PySendEnd", &Ns3AiMsgInterfaceType::PySendEnd)
        .def("GetCpp2PyStruct",
             &Ns3AiMsgInterfaceType::GetCpp2PyStruct,
             py::return_value_policy::reference)
        .def("GetPy2CppStruct",
             &Ns3AiMsgInterfaceType::GetPy2CppStruct,
             py::return_value_policy::reference)
        .def("PyGetFinished", &Ns3AiMsgInterfaceType::PyGetFinished);

    // == CONSTANTS
    // ===============================================================
    m.attr("MAX_UE_NUM") = MAX_UE_NUM;
    m.attr("MAX_PRB_NUM_LTE") = MAX_PRB_NUM_LTE;
    m.attr("MAX_PRB_NUM_NR") = MAX_PRB_NUM_NR;
    m.attr("MAX_ID_LEN") = MAX_ID_LEN;
    m.attr("E3SM_INDICATION") = E3SM_INDICATION;
    m.attr("E3SM_CONTROL_REQUEST") = E3SM_CONTROL_REQUEST;
    m.attr("E2SM_KPM_INDICATION") = E2SM_KPM_INDICATION;
    m.attr("E2SM_RC_CONTROL_REQUEST") = E2SM_RC_CONTROL_REQUEST;
    m.attr("SHM_SIZE") = SHM_SIZE;
    m.attr("LTE") = "LTE";
    m.attr("NR") = "NR";
    m.attr("DOWNLINK") = 0;
    m.attr("UPLINK") = 1;
}
