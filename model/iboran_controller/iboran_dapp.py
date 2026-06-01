# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

import os
import time
from abc import ABC, abstractmethod
from model.iboran_interfaces.iboran_shm_structure import *
from model.iboran_interfaces.iboran_e2sm_dapp_structure import *
from model.utils.do_log import DoLog
from model.utils.cpp2py_msg_validation import e3_indication_validation

SUBFRAMES_PER_FRAME = 10


class IbOranDApp(ABC):
    def __init__(
        self,
        intents,
        rat_type,
        control_period_sf,
        save_log=False,
    ):
        self.intents = intents
        self.rat_type = rat_type
        self.control_period_sf = control_period_sf
        self.save_log = save_log

        self.id = None

    def _reset_variables(self):
        self.script_dir = os.path.dirname(os.path.abspath(__file__))
        self.logger = DoLog(
            script_dir=self.script_dir,
            folder_name="../../notebook/logs/dapp_logs",
            log_name=f"dApp{self.id}.log",
        )
        if self.save_log:
            self.logger.overwrite(f"RAT Type: {self.rat_type}\n\n")
        else:
            self.logger.remove()

        self.e2_dapp_ = E2DappInterface()
        self._e3_control = {DOWNLINK: E3Control(), UPLINK: E3Control()}
        for ctrl in self._e3_control.values():
            ctrl.skipThisStep = True
            ctrl.ueSchedSize = 0
        self._e2_dapp_report = {DOWNLINK: E2DAppReport(), UPLINK: E2DAppReport()}
        self._e2_dapp_control = {DOWNLINK: E2DAppControl(), UPLINK: E2DAppControl()}

        self.step = 0
        self.last_control_sf = {DOWNLINK: 0, UPLINK: 0}
        self.dl_decisions = 0
        self.ul_decisions = 0
        self.decision_time_ms = 0
        self.errors = []

    def run(self, cpp2Py: Cpp2Py, py2Cpp: Py2Cpp, e2_dapp_control: E2DAppControl):
        e3Indication = cpp2Py.e3Indication
        link = e3Indication.linkDirection
        self.step += 1

        if self._should_skip_step(py2Cpp, e3Indication, e2_dapp_control):
            return self.e2_dapp_.report

        decision_start = time.perf_counter()
        done = self.process(e3Indication=e3Indication, py2Cpp=py2Cpp)
        if not done:
            self._apply_skip_step(py2Cpp, e3Indication)
            return self.e2_dapp_.report
        self.decision_time_ms += time.perf_counter() - decision_start

        if link == DOWNLINK:
            self.dl_decisions += 1
        else:
            self.ul_decisions += 1

        self.write_e3_control(py2Cpp, e3Indication)
        self.write_e2_dapp_report(link=link)
        return self.e2_dapp_.report

    @abstractmethod
    def process(self, e3Indication, py2Cpp) -> bool:
        "Execute the dApp control algorithm."
        ...

    @abstractmethod
    def write_e3_control(self, py2Cpp, e3Indication) -> None:
        "Write E3SM CONTROL message elements."
        ...

    @abstractmethod
    def write_e2_dapp_report(self, link) -> None:
        "Write E2SM-DAPP REPORT message elements."
        ...

    @abstractmethod
    def store_e2_dapp_control(self, e2_dapp_control, link) -> None:
        "Store E2SM-DAPP CONTROL message elements."
        ...

    def _should_skip_step(self, py2Cpp, e3Indication, e2_dapp_control):
        link = e3Indication.linkDirection
        abs_subframe_number = (
            e3Indication.frameNo * SUBFRAMES_PER_FRAME + e3Indication.subframeNo
        )
        subframes_since_last = abs_subframe_number - self.last_control_sf[link]
        if (
            subframes_since_last > 0
            and subframes_since_last % self.control_period_sf == 0
        ):
            self.last_control_sf[link] = abs_subframe_number
        else:
            self.write_e3_control(py2Cpp, e3Indication)
            self.write_e2_dapp_report(link=link)
            return True

        # verify E2SM-DAPP CONTROL
        if e2_dapp_control.type != E2SM_DAPP_CONTROL:
            print("ib-ORAN MAC Scheduler dApp: E2SM-DAPP CONTROL verification failed.")
        else:
            self.store_e2_dapp_control(e2_dapp_control, link=link)

        if self._e2_dapp_control[link].skipThisStep:
            self._apply_skip_step(py2Cpp, e3Indication)
            return True

        # verify E3SM INDICATION
        e3_indication_errors = e3_indication_validation(e3Indication, self.rat_type)
        if e3_indication_errors:
            self._apply_skip_step(py2Cpp, e3Indication)
            self.errors.append(e3_indication_errors)
            return True

        return False

    def _apply_skip_step(self, py2Cpp, e3Indication):
        link = e3Indication.linkDirection
        self._e3_control[link].skipThisStep = True
        self._e3_control[link].ueSchedSize = 0
        self.write_e3_control(py2Cpp, e3Indication)
        self.write_e2_dapp_report(link=link)

    def print_stats(self):
        header = f"-- dApp {self.id}: Stats "
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
                f"{1000 * self.decision_time_ms / self.step:.2f} ms"
            )

        print(f"{'Errors encountered':<22}: {len(self.errors)}")
        if self.errors:
            print("\n error(s)")
            for i in range(len(self.errors)):
                print(f" -> {self.errors[i]}")
