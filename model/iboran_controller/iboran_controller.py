# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

import time
from abc import ABC, abstractmethod

import numpy as np

from model.iboran_interfaces.iboran_shm_structure import Cpp2Py, Py2Cpp
from model.iboran_interfaces.iboran_e2sm_dapp_structure import (
    E2DappInterface,
    E2DAppControl,
    E2DAppReport,
)
from model.iboran_interfaces.iboran_shm_protocol import LinkDirection, RatType
from model.iboran_controller.iboran_controller_stats import IbOranControllerStats
from model.utils.cpp2py_message_checker import Cpp2PyMessageChecker


class IbOranController(ABC):
    _KIND: str = "Controller"

    def __init__(
        self,
        intents: dict[LinkDirection, np.ndarray] | None,
        rat_type: RatType,
        control_period: int,
        app_id: str | None = None,
    ):
        self.intents = intents
        self.rat_type = rat_type
        self.control_period = control_period
        self.app_id = app_id

        self.stats = IbOranControllerStats()

    def _run(self, cpp2Py: Cpp2Py, py2Cpp: Py2Cpp, peer_message) -> object:
        report = self._extract_report(cpp2Py)
        link_dir = report.linkDirection
        self.stats.steps[link_dir] += 1

        if self._should_skip_step(py2Cpp, report, peer_message):
            return self._output_message()

        decision_start = time.perf_counter()

        done = self.decide(report)
        if not done:
            self._apply_skip_step(py2Cpp, report)
            return self._output_message()

        self.stats.decision_time[link_dir] += time.perf_counter() - decision_start
        self.stats.decisions[link_dir] += 1

        self.write_primary_control(py2Cpp, report)
        self.write_dapp_interface_message(link_dir)
        return self._output_message()

    @abstractmethod
    def decide(self, report) -> bool:
        "Execute the control algorithm for this step."
        ...

    @abstractmethod
    def _extract_report(self, cpp2Py: Cpp2Py):
        "Pull this controller's primary report out of the shared-memory struct."
        ...

    @abstractmethod
    def _output_message(self) -> object:
        "Return the E2SM-DAPP message this controller hands back to its caller."
        ...

    @abstractmethod
    def write_primary_control(self, py2Cpp: Py2Cpp, report) -> None:
        "Write the primary control message consumed by the ns-3 RAN environment."
        ...

    @abstractmethod
    def write_dapp_interface_message(self, link_dir: LinkDirection) -> None:
        "Write this controller's outward E2SM-DAPP message."
        ...

    @abstractmethod
    def store_dapp_interface_message(
        self, link_dir: LinkDirection, peer_message
    ) -> None:
        "Store the inbound E2SM-DAPP message received from the peer controller."
        ...

    @abstractmethod
    def _verify_peer_message(self, peer_message) -> None:
        "Raise ValueError if peer message is not of the expected E2SM-DAPP type."
        ...

    @abstractmethod
    def _time_unit(self, report) -> int:
        "Return the absolute control-period unit (subframe or frame) for report."
        ...

    @abstractmethod
    def _reset_primary_control_to_skip(self, link_dir: LinkDirection) -> None:
        "Reset this controller's primary control struct to a skip state."
        ...

    def _extra_skip_check(self, link_dir: LinkDirection) -> bool:
        "Optional additional condition that forces a skip."
        return False

    def _reset_common_variables(self) -> None:
        self.e2_dapp_ = E2DappInterface()
        self._e2_dapp_report = self._per_link(E2DAppReport)
        self._e2_dapp_control = self._per_link(E2DAppControl)
        self.cpp2py_msg_checker = Cpp2PyMessageChecker(rat_type=self.rat_type)
        self.last_control_unit = self._per_link(int)

    def _per_link(self, factory):
        return {LinkDirection.DOWNLINK: factory(), LinkDirection.UPLINK: factory()}

    def _should_skip_step(self, py2Cpp: Py2Cpp, report, peer_message) -> bool:
        link_dir = report.linkDirection
        current_unit = self._time_unit(report)
        elapsed = current_unit - self.last_control_unit[link_dir]

        if elapsed > 0 and elapsed % self.control_period == 0:
            self.last_control_unit[link_dir] = current_unit
        else:
            self.write_primary_control(py2Cpp, report)
            self.write_dapp_interface_message(link_dir)
            return True

        self._verify_peer_message(peer_message)
        self.store_dapp_interface_message(link_dir, peer_message)

        if self._extra_skip_check(link_dir):
            self._apply_skip_step(py2Cpp, report)
            return True

        report_errors = self.cpp2py_msg_checker.detect_errors(report)
        if report_errors:
            self._apply_skip_step(py2Cpp, report)
            self.stats.errors.append(report_errors)
            return True

        return False

    def _apply_skip_step(self, py2Cpp: Py2Cpp, report) -> None:
        link_dir = report.linkDirection
        self._reset_primary_control_to_skip(link_dir)
        self.write_primary_control(py2Cpp, report)
        self.write_dapp_interface_message(link_dir)

    def print_stats(self) -> None:
        self.stats.print_stats(kind=self._KIND, app_id=self.app_id)
