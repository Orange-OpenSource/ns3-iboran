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
from model.iboran_interfaces.iboran_shm_structure import *
from model.iboran_interfaces.iboran_e2sm_dapp_structure import *
from model.iboran_controller.iboran_xapp import *

from model.iboran_controller.iboran_xapp_algos.xapp_algos_factory import (
    setup_xapp_algo_rbg_bounding,
)


class IbOranXAppMacScheduler(IbOranXApp):
    def __init__(self, intents, rat_type, control_period_f, algo_rbg_bounding):
        super().__init__(intents, rat_type, control_period_f)
        self.algo_rbg_bounding = algo_rbg_bounding
        self.id = "MacScheduler"
        self._reset_variables()

        if self.algo_rbg_bounding.startswith("iB"):
            self.algo_rbg_bounding = self.algo_rbg_bounding.removeprefix("iB")
        else:
            self.intents = None
        self.algo_rbg_bounding = setup_xapp_algo_rbg_bounding(
            self.algo_rbg_bounding, self.intents, self.rat_type
        )

    def _reset_variables(self) -> None:
        super()._reset_variables()
        self.algo_rbg_bounding = getattr(self, "algo_rbg_bounding", None)

    def process(self, e2KpmReport, py2Cpp) -> bool:
        link = e2KpmReport.linkDirection

        if e2KpmReport.numUe == 0 or e2KpmReport.numRbg == 0:
            return False

        self.candidated_idx, self.candidated_rnti, self.bounded_rbg = (
            self.algo_rbg_bounding.execute(
                e2KpmReport=e2KpmReport,
            )
        )

        if len(self.candidated_idx) == 0:
            self._e2_rc_control[link].skipThisStep = True
            self._e2_dapp_control[link].skipThisStep = True
            self._e2_rc_control[link].maxUeSchedSize = 0
            self._e2_dapp_control[link].maxUeSchedSize = 0
            self._e2_dapp_control[link].numAvailableRbg = 0
        else:
            self._e2_rc_control[link].skipThisStep = False
            self._e2_dapp_control[link].skipThisStep = False
            self._e2_rc_control[link].maxUeSchedSize = len(self.candidated_idx)
            self._e2_dapp_control[link].maxUeSchedSize = len(self.candidated_idx)
            self._e2_dapp_control[link].numAvailableRbg = np.sum(self.bounded_rbg)
            for i, rnti in enumerate(self.candidated_rnti):
                self._e2_rc_control[link].ue[i].rnti = rnti
                self._e2_rc_control[link].ue[i].maxRbgAllocation = self.bounded_rbg[i]

        return True

    def write_e2_rc_control(self, py2Cpp, e2KpmReport) -> None:
        py2Cpp.e2RcControl.type = E2SM_RC_CONTROL
        py2Cpp.e2RcControl.maxUeSchedSize = self._e2_rc_control[
            e2KpmReport.linkDirection
        ].maxUeSchedSize
        py2Cpp.e2RcControl.skipThisStep = self._e2_rc_control[
            e2KpmReport.linkDirection
        ].skipThisStep
        py2Cpp.e2RcControl.id = self.step
        py2Cpp.e2RcControl.linkDirection = e2KpmReport.linkDirection
        for i in range(e2KpmReport.numUe):
            ue = self._e2_rc_control[e2KpmReport.linkDirection].ue[i]
            py2Cpp.e2RcControl.ue[i].rnti = ue.rnti
            py2Cpp.e2RcControl.ue[i].maxRbgAllocation = ue.maxRbgAllocation

    def write_e2_dapp_control(self, link) -> None:
        self.e2_dapp_.control.type = E2SM_DAPP_CONTROL
        self.e2_dapp_.control.maxUeSchedSize = self._e2_dapp_control[
            link
        ].maxUeSchedSize
        self.e2_dapp_.control.numAvailableRbg = self._e2_dapp_control[
            link
        ].numAvailableRbg
        self.e2_dapp_.control.skipThisStep = self._e2_dapp_control[link].skipThisStep
        self.e2_dapp_.control.linkDirection = link
        self.e2_dapp_.control.id = self.step

    def store_e2_dapp_report(self, e2_dapp_report, link) -> None:
        self._e2_dapp_report[link].ueSchedSize = e2_dapp_report.ueSchedSize
        self._e2_dapp_report[link].numAllocatedRbg = e2_dapp_report.numAllocatedRbg
        self._e2_dapp_report[link].rbgSize = e2_dapp_report.rbgSize
        self._e2_dapp_report[link].linkDirection = e2_dapp_report.linkDirection
        self._e2_dapp_report[link].id = e2_dapp_report.id
