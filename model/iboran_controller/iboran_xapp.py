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
    E2KpmIndication,
    E2RcControlRequest,
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


class IbOranXApp(IbOranController):
    _KIND = "xApp"

    def __init__(
        self,
        intents: dict[LinkDirection, np.ndarray] | None,
        rat_type: RatType,
        control_period_f: int,
        save_log: bool = False,
        app_id: str | None = None,
    ):
        super().__init__(
            intents=intents,
            rat_type=rat_type,
            control_period=control_period_f,
            app_id=app_id,
        )
        self.save_log = save_log

    def run(
        self, cpp2Py: Cpp2Py, py2Cpp: Py2Cpp, e2_dapp_indication: E2DAppIndication
    ) -> E2DAppControlRequest:
        return self._run(cpp2Py=cpp2Py, py2Cpp=py2Cpp, peer_message=e2_dapp_indication)

    @abstractmethod
    def write_e2_rc_control(
        self, py2Cpp: Py2Cpp, e2KpmIndication: E2KpmIndication
    ) -> None:
        "Write E2SM-RC CONTROL REQUEST message elements."
        ...

    @abstractmethod
    def write_e2_dapp_control(self, link_dir: LinkDirection) -> None:
        "Write E2SM-DAPP CONTROL REQUEST message elements."
        ...

    @abstractmethod
    def store_e2_dapp_indication(
        self, link_dir: LinkDirection, e2_dapp_indication: E2DAppIndication
    ) -> None:
        "Store E2SM-DAPP INDICATION message elements."
        ...

    def _extract_report(self, cpp2Py: Cpp2Py) -> E2KpmIndication:
        return cpp2Py.e2KpmIndication

    def _output_message(self) -> E2DAppControlRequest:
        return self.e2_dapp_.control

    def write_primary_control(self, py2Cpp: Py2Cpp, report: E2KpmIndication) -> None:
        self.write_e2_rc_control(py2Cpp, report)

    def write_dapp_interface_message(self, link_dir: LinkDirection) -> None:
        self.write_e2_dapp_control(link_dir)

    def store_dapp_interface_message(
        self, link_dir: LinkDirection, peer_message: E2DAppIndication
    ) -> None:
        self.store_e2_dapp_indication(link_dir, peer_message)

    def _verify_peer_message(self, peer_message: E2DAppIndication) -> None:
        if peer_message.type != MessageType.E2SM_DAPP_INDICATION:
            raise ValueError(
                f"ib-ORAN {self._KIND}: E2SM-DAPP INDICATION verification failed."
            )

    def _report_due(self, report: E2KpmIndication) -> bool:
        # the RAN reports at the subscribed period (E2SM-KPM event trigger)
        return bool(report.present)

    def _outcome_of(self, outcome) -> tuple[int, int, int]:
        return outcome.e2RcRequestSn, outcome.e2RcCause, 0

    def _reset_primary_control_to_skip(self, link_dir: LinkDirection) -> None:
        self._e2_rc_control[link_dir].skipThisStep = True
        self._e2_dapp_control[link_dir].skipThisStep = True
        self._e2_rc_control[link_dir].maxUeSchedSize = 0
        self._e2_dapp_control[link_dir].maxUeSchedSize = 0

    def _reset_variables(self) -> None:
        self._reset_common_variables()

        store = FileStore(
            script_dir=os.path.dirname(os.path.abspath(__file__)),
            folder_name="../../notebook/logs/xapp_logs",
            log_name=f"xApp{self.app_id}.log",
        )
        self.store = None
        if self.save_log:
            self.store = store
            self.store.overwrite(f"RAT Type: {self.rat_type}\n\n")
        else:
            # a run without the log removes the log of an earlier run
            store.remove(prune_dir=True)

        self._e2_rc_control = self._per_link(E2RcControlRequest)
        for ctrl in self._e2_rc_control.values():
            ctrl.skipThisStep = True
            ctrl.maxUeSchedSize = 0
