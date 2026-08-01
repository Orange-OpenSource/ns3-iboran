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
from itertools import accumulate

from model.iboran_controller.iboran_dapp import IbOranDApp
from model.iboran_interfaces.iboran_shm_structure import (
    Py2Cpp,
    E3Indication,
)
from model.iboran_interfaces.iboran_e2sm_dapp_structure import (
    E2DAppControl,
)
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    MessageType,
    ProtocolLimits,
)
from model.utils.algo_resolver import resolve_ib_algo
from model.utils.mcs_assigner import McsAssigner
from model.utils.rbg_demand_calculator import RbgDemandCalculator
from model.iboran_controller.iboran_dapp_algos.dapp_algos_factory import (
    setup_dapp_algo_ue_selection,
    BASE_DAPP_ALGOS_KEYS,
    PROP_FAIRNESS_TUNING_FACTOR,
)


class IbOranDAppMacScheduler(IbOranDApp):
    def __init__(
        self, intents, rat_type, control_period_sf, save_log, k, algo_ue_selection
    ):
        super().__init__(
            intents, rat_type, control_period_sf, save_log, app_id="MacScheduler"
        )
        self.k = k
        self.algo_ue_selection = algo_ue_selection

        self._reset_variables()

    def decide(self, e3Indication: E3Indication) -> bool:
        link_dir = e3Indication.linkDirection

        if e3Indication.numUe == 0 or e3Indication.numRbg == 0:
            return False

        self.selected_idx_k, self.selected_rnti_k = self.algo_ue_selection.execute(
            e3Indication=e3Indication,
            _hist_link_rnti_subframe=self._hist_link_rnti_subframe,
            _hist_link_rnti_tps=self._hist_link_rnti_tps,
        )
        k_step = len(self.selected_idx_k)

        if k_step > 0:
            self.assigned_mcs_k = self.mcs_assigner.assign(
                e3Indication, self.selected_idx_k
            )
            self.start_rbg_k, self.allocated_rbg_k = self._allocate_rbg(e3Indication)

        while k_step > 0 and (
            k_step > self._e2_dapp_control[link_dir].maxUeSchedSize
            or sum(self.allocated_rbg_k[:k_step])
            > min(e3Indication.numRbg, self._e2_dapp_control[link_dir].numAvailableRbg)
        ):
            k_step -= 1
        self.selected_rnti_k = self.selected_rnti_k[:k_step]
        self.selected_idx_k = self.selected_idx_k[:k_step]
        self.assigned_mcs_k = self.assigned_mcs_k[:k_step]
        self.allocated_rbg_k = self.allocated_rbg_k[:k_step]
        self.start_rbg_k = self.start_rbg_k[:k_step]

        if k_step == 0:
            self._e3_control[link_dir].skipThisStep = True
            self._e3_control[link_dir].ueSchedSize = 0
            if self.save_log:
                self.store.append(
                    f"Control step: {self.stats.steps[link_dir]} | "
                    f"{link_dir} | "
                    f"No allocations made.\n"
                )
        else:
            self._e3_control[link_dir].skipThisStep = False
            self._e3_control[link_dir].ueSchedSize = k_step
            self._e2_dapp_report[link_dir].ueSchedSize = k_step
            self._e2_dapp_report[link_dir].numAllocatedRbg = sum(self.allocated_rbg_k)
            self._e2_dapp_report[link_dir].rbgSize = e3Indication.rbgSize

            self.sum_allocated_rb[link_dir] += (
                sum(self.allocated_rbg_k) * e3Indication.rbgSize
            )

            for i, rnti in enumerate(self.selected_rnti_k):
                self._e3_control[link_dir].ue[i].rnti = rnti
                self._e3_control[link_dir].ue[i].initAllocatedRbg = self.start_rbg_k[i]
                self._e3_control[link_dir].ue[i].numAllocatedRbg = self.allocated_rbg_k[
                    i
                ]
                self._e3_control[link_dir].ue[i].mcs = self.assigned_mcs_k[i]

                if rnti in self._hist_link_rnti_tps[link_dir].keys():
                    self._hist_link_rnti_tps[link_dir][rnti] = (
                        1 - PROP_FAIRNESS_TUNING_FACTOR
                    ) * self._hist_link_rnti_tps[link_dir][
                        rnti
                    ] + PROP_FAIRNESS_TUNING_FACTOR * e3Indication.ue[
                        self.selected_idx_k[i]
                    ].actualThroughput
                else:
                    self._hist_link_rnti_tps[link_dir][rnti] = (
                        e3Indication.ue[self.selected_idx_k[i]].actualThroughput
                        + 1e-9  # to avoid zero throughput
                    )

                self._hist_link_rnti_subframe[link_dir][rnti] = (
                    e3Indication.frameNo * ProtocolLimits.SUBFRAMES_PER_FRAME
                    + e3Indication.subframeNo
                )

                if self.save_log:
                    self.store.append(
                        f"Control step: {self.stats.steps[link_dir]} | "
                        f"{link_dir} | "
                        f"RNTI={rnti:1d}, "
                        f"{'PRBs' if e3Indication.rbgSize == 1 else 'RBGs'}="
                        f"[{self.start_rbg_k[i]}-{self.start_rbg_k[i]+self.allocated_rbg_k[i]-1}], "
                        f"MCS={self.assigned_mcs_k[i]}\n"
                    )

        return True

    def _allocate_rbg(self, e3Indication: E3Indication) -> tuple[list[int], np.ndarray]:
        allocated_rbg_k = self.rbg_demand_calculator.compute(
            e3Indication, self.selected_idx_k, self.assigned_mcs_k
        )
        start_rbg_k = list(accumulate(allocated_rbg_k, initial=e3Indication.initRbg))[
            :-1
        ]
        return start_rbg_k, allocated_rbg_k

    def write_e3_control(self, py2Cpp: Py2Cpp, e3Indication: E3Indication) -> None:
        py2Cpp.e3Control.type = MessageType.E3SM_CONTROL
        py2Cpp.e3Control.ueSchedSize = self._e3_control[
            e3Indication.linkDirection
        ].ueSchedSize
        py2Cpp.e3Control.skipThisStep = self._e3_control[
            e3Indication.linkDirection
        ].skipThisStep
        py2Cpp.e3Control.linkDirection = e3Indication.linkDirection
        py2Cpp.e3Control.id = self.stats.steps[e3Indication.linkDirection]
        for i in range(py2Cpp.e3Control.ueSchedSize):
            ue = self._e3_control[e3Indication.linkDirection].ue[i]
            py2Cpp.e3Control.ue[i].rnti = ue.rnti
            py2Cpp.e3Control.ue[i].initAllocatedRbg = ue.initAllocatedRbg
            py2Cpp.e3Control.ue[i].numAllocatedRbg = ue.numAllocatedRbg
            py2Cpp.e3Control.ue[i].mcs = ue.mcs

    def write_e2_dapp_report(self, link_dir: LinkDirection) -> None:
        self.e2_dapp_.report.type = MessageType.E2SM_DAPP_REPORT
        self.e2_dapp_.report.ueSchedSize = self._e2_dapp_report[link_dir].ueSchedSize
        self.e2_dapp_.report.numAllocatedRbg = self._e2_dapp_report[
            link_dir
        ].numAllocatedRbg
        self.e2_dapp_.report.rbgSize = self._e2_dapp_report[link_dir].rbgSize
        self.e2_dapp_.report.linkDirection = link_dir
        self.e2_dapp_.report.id = self.stats.steps[link_dir]

    def store_e2_dapp_control(
        self, link_dir: LinkDirection, e2_dapp_control: E2DAppControl
    ) -> None:
        self._e2_dapp_control[link_dir].maxUeSchedSize = e2_dapp_control.maxUeSchedSize
        self._e2_dapp_control[link_dir].numAvailableRbg = (
            e2_dapp_control.numAvailableRbg
        )
        self._e2_dapp_control[link_dir].skipThisStep = e2_dapp_control.skipThisStep
        self._e2_dapp_control[link_dir].linkDirection = e2_dapp_control.linkDirection
        self._e2_dapp_control[link_dir].id = e2_dapp_control.id

    def _reset_variables(self) -> None:
        super()._reset_variables()

        resolved_algo_ue_selection, is_ib = resolve_ib_algo(
            name=self.algo_ue_selection,
            base_algos=BASE_DAPP_ALGOS_KEYS,
            app_id=self.app_id,
        )
        if not is_ib:
            self.intents = None
        self.algo_ue_selection = setup_dapp_algo_ue_selection(
            resolved_algo_ue_selection, self.k, self.intents, self.rat_type
        )

        self.rbg_demand_calculator = RbgDemandCalculator(rat_type=self.rat_type)
        self.mcs_assigner = McsAssigner(rat_type=self.rat_type)

        self._hist_link_rnti_tps = self._per_link(dict)  # for 'PropFairness' algos
        self._hist_link_rnti_subframe = self._per_link(dict)  # for 'RoundRobin' algos
        self.sum_allocated_rb = self._per_link(int)

        self.selected_rnti_k = []
        self.selected_idx_k = []
        self.assigned_mcs_k = []
        self.allocated_rbg_k = []
        self.start_rbg_k = []
