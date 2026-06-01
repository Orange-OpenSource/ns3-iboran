# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from abc import ABC, abstractmethod
from model.iboran_interfaces.iboran_shm_structure import *
from model.iboran_interfaces.iboran_e2sm_dapp_structure import *
from model.utils.cpp2py_msg_validation import e2_kpm_report_validation
import time


class IbOranXApp(ABC):
    def __init__(self, intents, rat_type, control_period_f):
        self.intents = intents
        self.rat_type = rat_type
        self.control_period_f = control_period_f

        self.id = None

    def _reset_variables(self):
        self.e2_dapp_ = E2DappInterface()
        self._e2_rc_control = {DOWNLINK: E2RcControl(), UPLINK: E2RcControl()}
        for ctrl in self._e2_rc_control.values():
            ctrl.skipThisStep = True
            ctrl.maxUeSchedSize = 0

        self._e2_dapp_report = {DOWNLINK: E2DAppReport(), UPLINK: E2DAppReport()}
        self._e2_dapp_control = {DOWNLINK: E2DAppControl(), UPLINK: E2DAppControl()}

        self.step = 0
        self.last_control_f = {DOWNLINK: 0, UPLINK: 0}
        self.dl_decisions = 0
        self.ul_decisions = 0
        self.decision_time_ms = 0
        self.errors = []

    def run(self, cpp2Py: Cpp2Py, py2Cpp: Py2Cpp, e2_dapp_report: E2DAppReport):
        e2KpmReport = cpp2Py.e2KpmReport
        link = e2KpmReport.linkDirection
        self.step += 1

        if self._should_skip_step(py2Cpp, e2KpmReport, e2_dapp_report):
            return self.e2_dapp_.control

        decision_start = time.perf_counter()
        done = self.process(e2KpmReport=e2KpmReport, py2Cpp=py2Cpp)
        if not done:
            self._apply_skip_step(py2Cpp, e2KpmReport)
            return self.e2_dapp_.control
        self.decision_time_ms += time.perf_counter() - decision_start

        if link == DOWNLINK:
            self.dl_decisions += 1
        else:
            self.ul_decisions += 1

        self.write_e2_rc_control(py2Cpp, e2KpmReport)
        self.write_e2_dapp_control(link)
        return self.e2_dapp_.control

    @abstractmethod
    def process(self, e2KpmReport, py2Cpp) -> bool:
        "Execute the xApp control algorithm."
        ...

    @abstractmethod
    def write_e2_rc_control(self, py2Cpp, e2KpmReport) -> None:
        "Write E2SM-RC CONTROL message elements."
        ...

    @abstractmethod
    def write_e2_dapp_control(self, link) -> None:
        "Write E2SM-DAPP CONTROL message elements."
        ...

    @abstractmethod
    def store_e2_dapp_report(self, e2_dapp_report, link) -> None:
        "Store E2SM-DAPP REPORT message elements."
        ...

    def _should_skip_step(self, py2Cpp, e2KpmReport, e2_dapp_report):
        link = e2KpmReport.linkDirection
        frames_since_last = e2KpmReport.frameNo - self.last_control_f[link]
        if frames_since_last > 0 and frames_since_last % self.control_period_f == 0:
            self.last_control_f[link] = e2KpmReport.frameNo
        else:
            self.write_e2_rc_control(py2Cpp, e2KpmReport)
            self.write_e2_dapp_control(link)
            return True

        # verify E2SM-DAPP REPORT
        if e2_dapp_report.type != E2SM_DAPP_REPORT:
            print("ib-ORAN MAC Scheduler xApp: E2SM-DAPP REPORT verification failed.")
        else:
            self.store_e2_dapp_report(e2_dapp_report, link=link)

        # verify E2SM-KPM REPORT
        e2_kpm_report_errors = e2_kpm_report_validation(e2KpmReport, self.rat_type)
        if e2_kpm_report_errors:
            self._apply_skip_step(py2Cpp, e2KpmReport)
            self.errors.append(e2_kpm_report_errors)
            return True

        return False

    def _apply_skip_step(self, py2Cpp, e2KpmReport):
        link = e2KpmReport.linkDirection
        self._e2_rc_control[link].skipThisStep = True
        self._e2_dapp_control[link].skipThisStep = True
        self._e2_rc_control[link].maxUeSchedSize = 0
        self._e2_dapp_control[link].maxUeSchedSize = 0
        self.write_e2_rc_control(py2Cpp, e2KpmReport)
        self.write_e2_dapp_control(link)

    def print_stats(self):
        header = f"-- xApp {self.id}: Stats "
        print(f"\n{header:-<39}")
        print(f"{'Total control steps':<22}: {self.step}")
        total_decisions = self.dl_decisions + self.ul_decisions
        print(f"{'Total decisions':<22}: {total_decisions}")
        if total_decisions > 0:
            print(
                f"{'Downlink decisions':<22}: {self.dl_decisions}"
                f" ({100 * self.dl_decisions / (total_decisions):.2f} %)"
            )
            print(
                f"{'Uplink decisions':<22}: {self.ul_decisions}"
                f" ({100 * self.ul_decisions / (total_decisions):.2f} %)"
            )
        if self.step > 0:
            print(
                f"{'Avg decision time':<22}: "
                f"{1000*self.decision_time_ms / self.step:.2f} ms"
            )

        print(f"{'Errors encountered':<22}: {len(self.errors)}")
        if self.errors:
            print("\n error(s)")
            for i in range(len(self.errors)):
                print(f" -> {self.errors[i]}")
