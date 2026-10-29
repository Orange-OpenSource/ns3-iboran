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
    E3Control,
)
from model.iboran_interfaces.iboran_e2sm_dapp_structure import (
    E2DAppControl,
    E2DAppReport,
)
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    RatType,
    MessageType,
    ProtocolLimits,
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
        self, cpp2Py: Cpp2Py, py2Cpp: Py2Cpp, e2_dapp_control: E2DAppControl
    ) -> E2DAppReport:
        return self._run(cpp2Py=cpp2Py, py2Cpp=py2Cpp, peer_message=e2_dapp_control)

    @abstractmethod
    def write_e3_control(self, py2Cpp: Py2Cpp, e3Indication: E3Indication) -> None:
        "Write E3SM CONTROL message elements."
        ...

    @abstractmethod
    def write_e2_dapp_report(self, link_dir: LinkDirection) -> None:
        "Write E2SM-DAPP REPORT message elements."
        ...

    @abstractmethod
    def store_e2_dapp_control(
        self, link_dir: LinkDirection, e2_dapp_control: E2DAppControl
    ) -> None:
        "Store E2SM-DAPP CONTROL message elements."
        ...

    def _extract_report(self, cpp2Py: Cpp2Py) -> E3Indication:
        return cpp2Py.e3Indication

    def _output_message(self) -> E2DAppReport:
        return self.e2_dapp_.report

    def write_primary_control(self, py2Cpp: Py2Cpp, report: E3Indication) -> None:
        self.write_e3_control(py2Cpp, report)

    def write_dapp_interface_message(self, link_dir: LinkDirection) -> None:
        self.write_e2_dapp_report(link_dir)

    def store_dapp_interface_message(
        self, link_dir: LinkDirection, peer_message: E2DAppControl
    ) -> None:
        self.store_e2_dapp_control(link_dir, peer_message)

    def _verify_peer_message(self, peer_message: E2DAppControl) -> None:
        if peer_message.type != MessageType.E2SM_DAPP_CONTROL:
            raise ValueError(
                f"ib-ORAN {self._KIND}: E2SM-DAPP CONTROL verification failed."
            )

    def _time_unit(self, report: E3Indication) -> int:
        return report.timestamp // 1_000_000

    def _extra_skip_check(self, link_dir: LinkDirection) -> bool:
        return self._e2_dapp_control[link_dir].skipThisStep

    def _reset_primary_control_to_skip(self, link_dir: LinkDirection) -> None:
        self._e3_control[link_dir].skipThisStep = True
        self._e3_control[link_dir].ueSchedSize = 0

    def _reset_variables(self) -> None:
        self._reset_common_variables()

        self.store = FileStore(
            script_dir=os.path.dirname(os.path.abspath(__file__)),
            folder_name="../../notebook/logs/dapp_logs",
            log_name=f"dApp{self.app_id}.log",
        )
        if self.save_log:
            self.store.overwrite(f"RAT Type: {self.rat_type}\n\n")
        else:
            self.store.remove()

        self._e3_control = self._per_link(E3Control)
        for ctrl in self._e3_control.values():
            ctrl.skipThisStep = True
            ctrl.ueSchedSize = 0
