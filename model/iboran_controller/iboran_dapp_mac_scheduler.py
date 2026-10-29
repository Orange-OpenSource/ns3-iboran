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
    UeE2DAppReport,
)
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    MessageType,
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
        link = e3Indication.linkDirection

        if e3Indication.numUe == 0 or e3Indication.numRbg == 0:
            return False

        # PRB-utilization denominator: resources offered at every decision step
        self.sum_available_rb[link] += e3Indication.numRbg * e3Indication.rbgSize

        self.selected_idx_k, self.selected_rnti_k = self.algo_ue_selection.execute(
            e3Indication=e3Indication,
            e2DAppControl=self._e2_dapp_control[link],
            skipped_rntis={r for r, b in self._rbg_bounds[link].items() if b <= 0},
            _hist_link_rnti_subframe=self._hist_link_rnti_subframe,
            _hist_link_rnti_tps=self._hist_link_rnti_tps,
        )
        k_step = len(self.selected_idx_k)

        if k_step > 0:
            self.assigned_mcs_k = self.mcs_assigner.assign(
                Cpp2PyReport=e3Indication, input_idx_vect=self.selected_idx_k
            )
            self.start_rbg_k, self.allocated_rbg_k = self._allocate_rbg(e3Indication)
            # apply the xApp bounds before the budget check: a UE whose demand
            # exceeds its bound is served up to the bound instead of being dropped
            k_step = self._apply_rbg_bounds(link)

        while k_step > 0 and (
            k_step > self._e2_dapp_control[link].maxUeSchedSize
            or sum(self.allocated_rbg_k[:k_step])
            > min(e3Indication.numRbg, self._e2_dapp_control[link].numAvailableRbg)
        ):
            k_step -= 1
        self.selected_rnti_k = self.selected_rnti_k[:k_step]
        self.selected_idx_k = self.selected_idx_k[:k_step]
        self.assigned_mcs_k = self.assigned_mcs_k[:k_step]
        self.allocated_rbg_k = self.allocated_rbg_k[:k_step]
        # contiguous RBG ranges for the kept (bounded) allocations
        self.start_rbg_k = list(
            accumulate(self.allocated_rbg_k, initial=e3Indication.initRbg)
        )[:-1]

        if k_step == 0:
            self._e3_control[link].skipThisStep = True
            self._e3_control[link].ueSchedSize = 0
            self._e2_dapp_report[link].ue = self._grant_report(link, {})
            self._e2_dapp_report[link].ueSchedSize = 0
            self._e2_dapp_report[link].totalNumAllocatedRbg = 0
            if self.save_log:
                self.store.append(
                    f"Control step: {self.stats.steps[link]} | "
                    f"{link} | "
                    f"No allocations made.\n"
                )
        else:
            self._e3_control[link].skipThisStep = False
            self._e3_control[link].ueSchedSize = k_step
            self._e2_dapp_report[link].ueSchedSize = k_step
            self._e2_dapp_report[link].rbgSize = e3Indication.rbgSize

            for i, rnti in enumerate(self.selected_rnti_k):
                # grant accounting for the xApp, per xApp decision window
                acc = self._grant_acc[link].setdefault(rnti, [0, 0])
                acc[0] += int(self.allocated_rbg_k[i])
                acc[1] += 1

                self._e3_control[link].ue[i].rnti = rnti
                self._e3_control[link].ue[i].initAllocatedRbg = self.start_rbg_k[i]
                self._e3_control[link].ue[i].numAllocatedRbg = self.allocated_rbg_k[i]
                self._e3_control[link].ue[i].mcs = self.assigned_mcs_k[i]

                if rnti in self._hist_link_rnti_tps[link].keys():
                    self._hist_link_rnti_tps[link][rnti] = (
                        1 - PROP_FAIRNESS_TUNING_FACTOR
                    ) * self._hist_link_rnti_tps[link][
                        rnti
                    ] + PROP_FAIRNESS_TUNING_FACTOR * e3Indication.ue[
                        self.selected_idx_k[i]
                    ].actualThroughput
                else:
                    self._hist_link_rnti_tps[link][rnti] = (
                        e3Indication.ue[self.selected_idx_k[i]].actualThroughput
                        + 1e-9  # to avoid zero throughput
                    )

                self._hist_link_rnti_subframe[link][rnti] = (
                    e3Indication.timestamp // 1_000_000
                )  # ms

                if self.save_log:
                    self.store.append(
                        f"Control step: {self.stats.steps[link]} | "
                        f"{link} | "
                        f"RNTI={rnti:1d}, "
                        f"{'PRBs' if e3Indication.rbgSize == 1 else 'RBGs'}="
                        f"[{self.start_rbg_k[i]}-{self.start_rbg_k[i]+self.allocated_rbg_k[i]-1}], "
                        f"MCS={self.assigned_mcs_k[i]}\n"
                    )

            self._e2_dapp_report[link].ue = self._grant_report(
                link, dict(zip(self.selected_rnti_k, self.allocated_rbg_k))
            )
            self._e2_dapp_report[link].totalNumAllocatedRbg = sum(self.allocated_rbg_k)
            self.sum_allocated_rb[link] += (
                sum(self.allocated_rbg_k) * e3Indication.rbgSize
            )
        return True

    def _grant_report(self, link: LinkDirection, last_alloc: dict) -> list:
        "Per-RNTI grants accumulated since the last xApp decision."
        return [
            UeE2DAppReport(
                rnti=rnti,
                numAllocatedRbg=int(last_alloc.get(rnti, 0)),
                sumAllocatedRbg=int(acc[0]),
                numScheduledTti=int(acc[1]),
            )
            for rnti, acc in self._grant_acc[link].items()
        ]

    def _allocate_rbg(self, e3Indication: E3Indication) -> tuple[list[int], np.ndarray]:
        allocated_rbg_k = self.rbg_demand_calculator.compute(
            Cpp2PyReport=e3Indication,
            input_idx_vect=self.selected_idx_k,
            assigned_mcs_k=self.assigned_mcs_k,
        )
        start_rbg_k = list(accumulate(allocated_rbg_k, initial=e3Indication.initRbg))[
            :-1
        ]
        return start_rbg_k, allocated_rbg_k

    def _apply_rbg_bounds(self, link: LinkDirection) -> int:
        # Bounds persist until the xApp replaces them; a UE with no bound is unbounded.
        # Same rule as the C++ schedulers (m_ueDl/UlMaxRbgAllocation).
        k_step = len(self.selected_idx_k)
        max_rbg_by_rnti = self._rbg_bounds[link]
        for j, rnti in enumerate(self.selected_rnti_k):
            max_rbg_allocation = max_rbg_by_rnti.get(rnti, self.allocated_rbg_k[j])
            self.allocated_rbg_k[j] = int(
                min(max_rbg_allocation, self.allocated_rbg_k[j])
            )

        keep_idx = [j for j in range(k_step) if self.allocated_rbg_k[j] > 0]

        self.selected_rnti_k = [self.selected_rnti_k[j] for j in keep_idx]
        self.selected_idx_k = [self.selected_idx_k[j] for j in keep_idx]
        self.assigned_mcs_k = [self.assigned_mcs_k[j] for j in keep_idx]
        self.allocated_rbg_k = [self.allocated_rbg_k[j] for j in keep_idx]
        self.start_rbg_k = [self.start_rbg_k[j] for j in keep_idx]
        return len(keep_idx)

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

    def write_e2_dapp_report(self, link: LinkDirection) -> None:
        self.e2_dapp_.report.type = MessageType.E2SM_DAPP_REPORT
        self.e2_dapp_.report.ueSchedSize = self._e2_dapp_report[link].ueSchedSize
        self.e2_dapp_.report.totalNumAllocatedRbg = self._e2_dapp_report[
            link
        ].totalNumAllocatedRbg
        self.e2_dapp_.report.rbgSize = self._e2_dapp_report[link].rbgSize
        self.e2_dapp_.report.linkDirection = link
        self.e2_dapp_.report.id = self.stats.steps[link]
        self.e2_dapp_.report.ue = list(self._e2_dapp_report[link].ue)

    def store_e2_dapp_control(
        self, link: LinkDirection, e2_dapp_control: E2DAppControl
    ) -> None:
        self._e2_dapp_control[link].maxUeSchedSize = e2_dapp_control.maxUeSchedSize
        self._e2_dapp_control[link].numAvailableRbg = e2_dapp_control.numAvailableRbg
        self._e2_dapp_control[link].skipThisStep = e2_dapp_control.skipThisStep
        self._e2_dapp_control[link].linkDirection = e2_dapp_control.linkDirection
        self._e2_dapp_control[link].id = e2_dapp_control.id
        self._e2_dapp_control[link].ue = list(e2_dapp_control.ue)
        if not e2_dapp_control.skipThisStep:
            for ue in e2_dapp_control.ue:
                self._rbg_bounds[link][ue.rnti] = int(ue.maxRbgAllocation)

        # a new xApp decision opens a new accounting window for the grant report
        if e2_dapp_control.decisionId != self._last_xapp_decision[link]:
            self._last_xapp_decision[link] = e2_dapp_control.decisionId
            self._grant_acc[link] = {}
            self._e2_dapp_report[link].ue = []

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
        self._rbg_bounds = self._per_link(
            dict
        )  # persistent xApp bounds: rnti -> max RBG
        self.sum_allocated_rb = self._per_link(int)
        self.sum_available_rb = self._per_link(int)
        self._grant_acc = self._per_link(dict)  # rnti -> [sum RBG, scheduled TTIs]
        self._last_xapp_decision = self._per_link(lambda: -1)

        self.selected_rnti_k = []
        self.selected_idx_k = []
        self.assigned_mcs_k = []
        self.allocated_rbg_k = []
        self.start_rbg_k = []
