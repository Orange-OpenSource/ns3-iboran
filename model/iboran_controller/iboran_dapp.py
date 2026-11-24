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
from abc import abstractmethod

import numpy as np

from model.iboran_interfaces.iboran_shm_structure import (
    Cpp2Py,
    Py2Cpp,
    E3Indication,
    E3ControlRequest,
)
from model.iboran_interfaces.iboran_e2sm_dapp_structure import (
    E2DAppControlRequest,
    E2DAppIndication,
)
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    RatType,
    MessageType,
)
from model.iboran_controller.iboran_controller import IbOranController
from model.utils.file_store import FileStore


class IbOranDApp(IbOranController):
    _KIND = "dApp"

    def __init__(
        self,
        intents: dict[LinkDirection, np.ndarray] | None,
        rat_type: RatType,
        control_period_sf: int,
        save_log: bool = False,
        app_id: str | None = None,
    ):
        super().__init__(
            intents=intents,
            rat_type=rat_type,
            control_period=control_period_sf,
            app_id=app_id,
        )
        self.save_log = save_log

    def run(
        self, cpp2Py: Cpp2Py, py2Cpp: Py2Cpp, e2_dapp_control: E2DAppControlRequest
    ) -> E2DAppIndication:
        return self._run(cpp2Py=cpp2Py, py2Cpp=py2Cpp, peer_message=e2_dapp_control)

    @abstractmethod
    def write_e3_control(self, py2Cpp: Py2Cpp, e3Indication: E3Indication) -> None:
        "Write E3SM CONTROL message elements."
        ...

    @abstractmethod
    def write_e2_dapp_indication(self, link_dir: LinkDirection) -> None:
        "Write E2SM-DAPP INDICATION message elements."
        ...

    @abstractmethod
    def store_e2_dapp_control(
        self, link_dir: LinkDirection, e2_dapp_control: E2DAppControlRequest
    ) -> None:
        "Store E2SM-DAPP CONTROL REQUEST message elements."
        ...

    def _extract_report(self, cpp2Py: Cpp2Py) -> E3Indication:
        return cpp2Py.e3Indication

    def _output_message(self) -> E2DAppIndication:
        return self.e2_dapp_.indication

    def write_primary_control(self, py2Cpp: Py2Cpp, report: E3Indication) -> None:
        self.write_e3_control(py2Cpp, report)

    def write_dapp_interface_message(self, link_dir: LinkDirection) -> None:
        self.write_e2_dapp_indication(link_dir)

    def store_dapp_interface_message(
        self, link_dir: LinkDirection, peer_message: E2DAppControlRequest
    ) -> None:
        self.store_e2_dapp_control(link_dir, peer_message)

    def _verify_peer_message(self, peer_message: E2DAppControlRequest) -> None:
        if peer_message.type != MessageType.E2SM_DAPP_CONTROL_REQUEST:
            raise ValueError(
                f"ib-ORAN {self._KIND}: E2SM-DAPP CONTROL REQUEST verification failed."
            )

    def _report_due(self, report: E3Indication) -> bool:
        link_dir = report.linkDirection
        current_ms = report.timestamp // 1_000_000
        if current_ms - self._last_control_ms[link_dir] >= self.control_period:
            self._last_control_ms[link_dir] = current_ms
            return True
        return False

    def _outcome_of(self, outcome) -> tuple[int, int, int]:
        return outcome.e3RequestSn, outcome.e3Cause, outcome.e3NumRejectedUe

    def _extra_skip_check(self, link_dir: LinkDirection) -> bool:
        return self._e2_dapp_control[link_dir].skipThisStep

    def _reset_primary_control_to_skip(self, link_dir: LinkDirection) -> None:
        self._e3_control[link_dir].skipThisStep = True
        self._e3_control[link_dir].ueSchedSize = 0

    def _reset_variables(self) -> None:
        self._reset_common_variables()
        self._last_control_ms = self._per_link(int)

        store = FileStore(
            script_dir=os.path.dirname(os.path.abspath(__file__)),
            folder_name="../../notebook/logs/dapp_logs",
            log_name=f"dApp{self.app_id}.log",
        )
        self.store = None
        if self.save_log:
            self.store = store
            self.store.overwrite(f"RAT Type: {self.rat_type}\n\n")
        else:
            # a run without the log removes the log of an earlier run
            store.remove(prune_dir=True)

        self._e3_control = self._per_link(E3ControlRequest)
        for ctrl in self._e3_control.values():
            ctrl.skipThisStep = True
            ctrl.ueSchedSize = 0
