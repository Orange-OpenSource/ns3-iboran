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

#include <pybind11/pybind11.h>

using namespace ns3;

namespace py = pybind11;

PYBIND11_MODULE(iboran_shm_structure_py_binding, m)
{
    // == E2SM-KPM REPORT STRUCTURE ==============================================
    py::class_<ns3::UeE2KpmReport>(m, "UeE2KpmReport")
        .def(py::init<>())
        .def_readwrite("rnti", &ns3::UeE2KpmReport::rnti)
        .def_readwrite("cqi", &ns3::UeE2KpmReport::cqi)
        .def_readwrite("bufferLevel", &ns3::UeE2KpmReport::bufferLevel)
        .def_property(
            "contentId",
            [](const ns3::UeE2KpmReport& ue) { return std::string(ue.contentId); },
            [](ns3::UeE2KpmReport& ue, const std::string& value) {
                std::snprintf(ue.contentId, MAX_ID_LEN, "%s", value.c_str());
            })
        .def_readwrite("qci", &ns3::UeE2KpmReport::qci)
        .def_readwrite("achievableRate", &ns3::UeE2KpmReport::achievableRate)
        .def_readwrite("actualThroughput", &ns3::UeE2KpmReport::actualThroughput)
        .def_readwrite("totalBytesServed", &ns3::UeE2KpmReport::totalBytesServed)
        .def_readwrite("packetDelay", &ns3::UeE2KpmReport::packetDelay);

    py::class_<ns3::E2KpmReport>(m, "E2KpmReport")
        .def(py::init<>())
        .def_readwrite("type", &ns3::E2KpmReport::type)
        .def_readwrite("timestamp", &ns3::E2KpmReport::timestamp)
        .def_readwrite("numUe", &ns3::E2KpmReport::numUe)
        .def_readwrite("numRbg", &ns3::E2KpmReport::numRbg)
        .def_readwrite("rbgSize", &ns3::E2KpmReport::rbgSize)
        .def_readwrite("frameNo", &ns3::E2KpmReport::frameNo)
        .def_readwrite("totalBufferLevel", &ns3::E2KpmReport::totalBufferLevel)
        .def_readwrite("linkDirection", &ns3::E2KpmReport::linkDirection)
        .def_readwrite("id", &ns3::E2KpmReport::id)
        .def_property(
            "ue",
            [](ns3::E2KpmReport& self) {
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
            [](ns3::E2KpmReport& self, py::list ueE2List) {
                for (size_t i = 0; i < std::min<size_t>(ueE2List.size(), MAX_UE_NUM); ++i)
                {
                    self.ue[i] = ueE2List[i].cast<ns3::UeE2KpmReport>();
                }
            });

    // == E2SM-RC CONTROL STRUCTURE ===============================================
    py::class_<ns3::UeE2RcControl>(m, "UeE2RcControl")
        .def(py::init<>())
        .def_readwrite("rnti", &ns3::UeE2RcControl::rnti)
        .def_readwrite("maxRbgAllocation", &ns3::UeE2RcControl::maxRbgAllocation);

    py::class_<ns3::E2RcControl>(m, "E2RcControl")
        .def(py::init<>())
        .def_readwrite("type", &ns3::E2RcControl::type)
        .def_readwrite("maxUeSchedSize", &ns3::E2RcControl::maxUeSchedSize)
        .def_readwrite("skipThisStep", &ns3::E2RcControl::skipThisStep)
        .def_readwrite("linkDirection", &ns3::E2RcControl::linkDirection)
        .def_readwrite("id", &ns3::E2RcControl::id)
        .def_property(
            "ue",
            [](ns3::E2RcControl& self) {
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
            [](ns3::E2RcControl& self, py::list ueRcList) {
                for (size_t i = 0; i < std::min<size_t>(ueRcList.size(), MAX_UE_NUM); ++i)
                {
                    self.ue[i] = ueRcList[i].cast<ns3::UeE2RcControl>();
                }
            });

    // == E3SM INDICATION STRUCTURE ===============================================
    py::class_<ns3::UeE3Indication>(m, "UeE3Indication")
        .def(py::init<>())
        .def_readwrite("rnti", &ns3::UeE3Indication::rnti)
        .def_readwrite("cqi", &ns3::UeE3Indication::cqi)
        .def_readwrite("bufferLevel", &ns3::UeE3Indication::bufferLevel)
        .def_readwrite("contentCriticality", &ns3::UeE3Indication::contentCriticality)
        .def_property(
            "contentId",
            [](const ns3::UeE3Indication& ue) { return std::string(ue.contentId); },
            [](ns3::UeE3Indication& ue, const std::string& value) {
                std::snprintf(ue.contentId, MAX_ID_LEN, "%s", value.c_str());
            })
        .def_readwrite("qci", &ns3::UeE3Indication::qci)
        .def_readwrite("achievableRate", &ns3::UeE3Indication::achievableRate)
        .def_readwrite("actualThroughput", &ns3::UeE3Indication::actualThroughput)
        .def_readwrite("totalBytesServed", &ns3::UeE3Indication::totalBytesServed)
        .def_readwrite("packetDelay", &ns3::UeE3Indication::packetDelay);

    py::class_<ns3::E3Indication>(m, "E3Indication")
        .def(py::init<>())
        .def_readwrite("type", &ns3::E3Indication::type)
        .def_readwrite("timestamp", &ns3::E3Indication::timestamp)
        .def_readwrite("subframeNo", &ns3::E3Indication::subframeNo)
        .def_readwrite("frameNo", &ns3::E3Indication::frameNo)
        .def_readwrite("numUe", &ns3::E3Indication::numUe)
        .def_readwrite("totalBufferLevel", &ns3::E3Indication::totalBufferLevel)
        .def_readwrite("initRbg", &ns3::E3Indication::initRbg)
        .def_readwrite("numRbg", &ns3::E3Indication::numRbg)
        .def_readwrite("rbgSize", &ns3::E3Indication::rbgSize)
        .def_readwrite("linkDirection", &ns3::E3Indication::linkDirection)
        .def_readwrite("id", &ns3::E3Indication::id)
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

    // == E3SM CONTROL STRUCTURE ==================================================
    py::class_<ns3::UeE3Control>(m, "UeE3Control")
        .def(py::init<>())
        .def_readwrite("rnti", &ns3::UeE3Control::rnti)
        .def_readwrite("initAllocatedRbg", &ns3::UeE3Control::initAllocatedRbg)
        .def_readwrite("numAllocatedRbg", &ns3::UeE3Control::numAllocatedRbg)
        .def_readwrite("mcs", &ns3::UeE3Control::mcs);

    py::class_<ns3::E3Control>(m, "E3Control")
        .def(py::init<>())
        .def_readwrite("type", &ns3::E3Control::type)
        .def_readwrite("ueSchedSize", &ns3::E3Control::ueSchedSize)
        .def_readwrite("skipThisStep", &ns3::E3Control::skipThisStep)
        .def_readwrite("linkDirection", &ns3::E3Control::linkDirection)
        .def_readwrite("id", &ns3::E3Control::id)
        .def_property(
            "ue",
            [](ns3::E3Control& self) {
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
            [](ns3::E3Control& self, py::list ueRcList) {
                for (size_t i = 0; i < std::min<size_t>(ueRcList.size(), MAX_UE_NUM); ++i)
                {
                    self.ue[i] = ueRcList[i].cast<ns3::UeE3Control>();
                }
            });

    // == SHARED MEMORY INTERFACE STRUCTURE =======================================
    py::class_<ns3::Cpp2Py>(m, "Cpp2Py")
        .def(py::init<>())
        .def_property(
            "e3Indication",
            [](ns3::Cpp2Py& self) -> ns3::E3Indication& { return self.e3Indication; },
            [](ns3::Cpp2Py& self, const ns3::E3Indication& v) { self.e3Indication = v; },
            py::return_value_policy::reference_internal)
        .def_property(
            "e2KpmReport",
            [](ns3::Cpp2Py& self) -> ns3::E2KpmReport& { return self.e2KpmReport; },
            [](ns3::Cpp2Py& self, const ns3::E2KpmReport& v) { self.e2KpmReport = v; },
            py::return_value_policy::reference_internal);

    py::class_<ns3::Py2Cpp>(m, "Py2Cpp")
        .def(py::init<>())
        .def_property(
            "e3Control",
            [](ns3::Py2Cpp& self) -> ns3::E3Control& { return self.e3Control; },
            [](ns3::Py2Cpp& self, const ns3::E3Control& v) { self.e3Control = v; },
            py::return_value_policy::reference_internal)
        .def_property(
            "e2RcControl",
            [](ns3::Py2Cpp& self) -> ns3::E2RcControl& { return self.e2RcControl; },
            [](ns3::Py2Cpp& self, const ns3::E2RcControl& v) { self.e2RcControl = v; },
            py::return_value_policy::reference_internal);

    // == MESSAGE INTERFACE STRUCTURE =============================================
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

    // == CONSTANTS ===============================================================
    m.attr("MAX_UE_NUM") = MAX_UE_NUM;
    m.attr("MAX_PRB_NUM_LTE") = MAX_PRB_NUM_LTE;
    m.attr("MAX_ID_LEN") = MAX_ID_LEN;
    m.attr("E3SM_INDICATION") = E3SM_INDICATION;
    m.attr("E3SM_CONTROL") = E3SM_CONTROL;
    m.attr("E2SM_KPM_REPORT") = E2SM_KPM_REPORT;
    m.attr("E2SM_RC_CONTROL") = E2SM_RC_CONTROL;
    m.attr("SHM_SIZE") = SHM_SIZE;
    m.attr("LTE") = "LTE";
    m.attr("DOWNLINK") = 0;
    m.attr("UPLINK") = 1;
}
