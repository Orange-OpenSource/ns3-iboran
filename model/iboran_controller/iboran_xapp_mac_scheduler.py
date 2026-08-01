# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

import numpy as np

from model.utils.algo_resolver import resolve_ib_algo
from model.iboran_interfaces.iboran_shm_structure import (
    Py2Cpp,
    E2KpmReport,
)
from model.iboran_interfaces.iboran_e2sm_dapp_structure import E2DAppReport
from model.iboran_interfaces.iboran_shm_protocol import LinkDirection, MessageType
from model.iboran_controller.iboran_xapp import IbOranXApp
from model.iboran_controller.iboran_xapp_algos.xapp_algos_factory import (
    setup_xapp_algo_rbg_bounding,
    BASE_XAPP_ALGOS_KEYS,
)


class IbOranXAppMacScheduler(IbOranXApp):
    def __init__(self, intents, rat_type, control_period_f, algo_rbg_bounding):
        super().__init__(intents, rat_type, control_period_f, app_id="MacScheduler")
        self.algo_rbg_bounding = algo_rbg_bounding

        self._reset_variables()

    def decide(self, e2KpmReport: E2KpmReport) -> bool:
        link = e2KpmReport.linkDirection

        if e2KpmReport.numUe == 0 or e2KpmReport.numRbg == 0:
            return False

        self.candidate_idx, self.candidate_rnti, self.bounded_rbg = (
            self.algo_rbg_bounding.execute(
                e2KpmReport=e2KpmReport,
            )
        )
        num_candidate = len(self.candidate_idx)

        if num_candidate == 0:
            self._e2_rc_control[link].skipThisStep = True
            self._e2_dapp_control[link].skipThisStep = True
            self._e2_rc_control[link].maxUeSchedSize = 0
            self._e2_dapp_control[link].maxUeSchedSize = 0
            self._e2_dapp_control[link].numAvailableRbg = 0
        else:
            self._e2_rc_control[link].skipThisStep = False
            self._e2_dapp_control[link].skipThisStep = False
            self._e2_rc_control[link].maxUeSchedSize = num_candidate
            self._e2_dapp_control[link].maxUeSchedSize = num_candidate
            self._e2_dapp_control[link].numAvailableRbg = np.sum(self.bounded_rbg)
            for i, rnti in enumerate(self.candidate_rnti):
                self._e2_rc_control[link].ue[i].rnti = rnti
                self._e2_rc_control[link].ue[i].maxRbgAllocation = self.bounded_rbg[i]

        return True

    def write_e2_rc_control(self, py2Cpp: Py2Cpp, e2KpmReport: E2KpmReport) -> None:
        py2Cpp.e2RcControl.type = MessageType.E2SM_RC_CONTROL
        py2Cpp.e2RcControl.maxUeSchedSize = self._e2_rc_control[
            e2KpmReport.linkDirection
        ].maxUeSchedSize
        py2Cpp.e2RcControl.skipThisStep = self._e2_rc_control[
            e2KpmReport.linkDirection
        ].skipThisStep
        py2Cpp.e2RcControl.id = self.stats.steps[e2KpmReport.linkDirection]
        py2Cpp.e2RcControl.linkDirection = e2KpmReport.linkDirection
        for i in range(e2KpmReport.numUe):
            ue = self._e2_rc_control[e2KpmReport.linkDirection].ue[i]
            py2Cpp.e2RcControl.ue[i].rnti = ue.rnti
            py2Cpp.e2RcControl.ue[i].maxRbgAllocation = ue.maxRbgAllocation

    def write_e2_dapp_control(self, link_dir: LinkDirection) -> None:
        self.e2_dapp_.control.type = MessageType.E2SM_DAPP_CONTROL
        self.e2_dapp_.control.maxUeSchedSize = self._e2_dapp_control[
            link_dir
        ].maxUeSchedSize
        self.e2_dapp_.control.numAvailableRbg = self._e2_dapp_control[
            link_dir
        ].numAvailableRbg
        self.e2_dapp_.control.skipThisStep = self._e2_dapp_control[
            link_dir
        ].skipThisStep
        self.e2_dapp_.control.linkDirection = link_dir
        self.e2_dapp_.control.id = self.stats.steps[link_dir]

    def store_e2_dapp_report(
        self, link_dir: LinkDirection, e2_dapp_report: E2DAppReport
    ) -> None:
        self._e2_dapp_report[link_dir].ueSchedSize = e2_dapp_report.ueSchedSize
        self._e2_dapp_report[link_dir].numAllocatedRbg = e2_dapp_report.numAllocatedRbg
        self._e2_dapp_report[link_dir].rbgSize = e2_dapp_report.rbgSize
        self._e2_dapp_report[link_dir].linkDirection = e2_dapp_report.linkDirection
        self._e2_dapp_report[link_dir].id = e2_dapp_report.id

    def _reset_variables(self) -> None:
        super()._reset_variables()

        resolved_algo_rbg_bounding, is_ib = resolve_ib_algo(
            name=self.algo_rbg_bounding,
            base_algos=BASE_XAPP_ALGOS_KEYS,
            app_id=self.app_id,
        )
        if not is_ib:
            self.intents = None
        self.algo_rbg_bounding = setup_xapp_algo_rbg_bounding(
            resolved_algo_rbg_bounding, self.intents, self.rat_type
        )
