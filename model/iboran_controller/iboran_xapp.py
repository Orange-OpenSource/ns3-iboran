# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from abc import abstractmethod

import numpy as np

from model.iboran_interfaces.iboran_shm_structure import (
    Cpp2Py,
    Py2Cpp,
    E2KpmReport,
    E2RcControl,
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


class IbOranXApp(IbOranController):
    _KIND = "xApp"

    def __init__(
        self,
        intents: dict[LinkDirection, np.ndarray] | None,
        rat_type: RatType,
        control_period_f: int,
        app_id: str | None = None,
    ):
        super().__init__(
            intents=intents,
            rat_type=rat_type,
            control_period=control_period_f,
            app_id=app_id,
        )

    def run(
        self, cpp2Py: Cpp2Py, py2Cpp: Py2Cpp, e2_dapp_report: E2DAppReport
    ) -> E2DAppControl:
        return self._run(cpp2Py=cpp2Py, py2Cpp=py2Cpp, peer_message=e2_dapp_report)

    @abstractmethod
    def write_e2_rc_control(self, py2Cpp: Py2Cpp, e2KpmReport: E2KpmReport) -> None:
        "Write E2SM-RC CONTROL message elements."
        ...

    @abstractmethod
    def write_e2_dapp_control(self, link_dir: LinkDirection) -> None:
        "Write E2SM-DAPP CONTROL message elements."
        ...

    @abstractmethod
    def store_e2_dapp_report(
        self, link_dir: LinkDirection, e2_dapp_report: E2DAppReport
    ) -> None:
        "Store E2SM-DAPP REPORT message elements."
        ...

    def _extract_report(self, cpp2Py: Cpp2Py) -> E2KpmReport:
        return cpp2Py.e2KpmReport

    def _output_message(self) -> E2DAppControl:
        return self.e2_dapp_.control

    def write_primary_control(self, py2Cpp: Py2Cpp, report: E2KpmReport) -> None:
        self.write_e2_rc_control(py2Cpp, report)

    def write_dapp_interface_message(self, link_dir: LinkDirection) -> None:
        self.write_e2_dapp_control(link_dir)

    def store_dapp_interface_message(
        self, link_dir: LinkDirection, peer_message: E2DAppReport
    ) -> None:
        self.store_e2_dapp_report(link_dir, peer_message)

    def _verify_peer_message(self, peer_message: E2DAppReport) -> None:
        if peer_message.type != MessageType.E2SM_DAPP_REPORT:
            raise ValueError(
                f"ib-ORAN {self._KIND}: E2SM-DAPP REPORT verification failed."
            )

    def _time_unit(self, report: E2KpmReport) -> int:
        return report.timestamp // 10_000_000

    def _reset_primary_control_to_skip(self, link_dir: LinkDirection) -> None:
        self._e2_rc_control[link_dir].skipThisStep = True
        self._e2_dapp_control[link_dir].skipThisStep = True
        self._e2_rc_control[link_dir].maxUeSchedSize = 0
        self._e2_dapp_control[link_dir].maxUeSchedSize = 0

    def _reset_variables(self) -> None:
        self._reset_common_variables()

        self._e2_rc_control = self._per_link(E2RcControl)
        for ctrl in self._e2_rc_control.values():
            ctrl.skipThisStep = True
            ctrl.maxUeSchedSize = 0
