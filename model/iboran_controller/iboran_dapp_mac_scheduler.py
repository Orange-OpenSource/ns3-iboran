# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from itertools import accumulate
from model.utils.assign_mcs import assign_mcs
from model.iboran_controller.iboran_dapp import *
from model.utils.rbg_demand import RbgDemand

from model.iboran_controller.iboran_dapp_algos.dapp_algos_factory import (
    setup_dapp_algo_ue_selection,
)


class IbOranDAppMacScheduler(IbOranDApp):
    def __init__(
        self, intents, rat_type, control_period_sf, save_log, k, algo_ue_selection
    ):
        super().__init__(intents, rat_type, control_period_sf, save_log)
        self.k = k
        self.algo_ue_selection = algo_ue_selection
        self.id = "MacScheduler"
        self._reset_variables()

        if self.algo_ue_selection.startswith("iB"):
            self.algo_ue_selection = self.algo_ue_selection.removeprefix("iB")
        else:
            self.intents = None
        self.algo_ue_selection = setup_dapp_algo_ue_selection(
            self.algo_ue_selection, self.k, self.intents, self.rat_type
        )

        self.rbg_demand = RbgDemand(rat_type=self.rat_type)

        self._hist_link_rnti_throughput = {
            f"{DOWNLINK}": {},
            f"{UPLINK}": {},
        }  # required by Prop Fairness algos
        self._hist_link_rnti_subframe = {
            f"{DOWNLINK}": {},
            f"{UPLINK}": {},
        }  # required by Round-Robin algos

        self.sum_allocated_rb = 0
        self.selected_rnti_k = []
        self.selected_idx_k = []
        self.assigned_mcs_k = []
        self.allocated_rbg_k = []
        self.start_rbg_k = []

    def _reset_variables(self) -> None:
        super()._reset_variables()
        self.k = getattr(self, "k", None)
        self.algo_ue_selection = getattr(self, "algo_ue_selection", None)

    def process(self, e3Indication, py2Cpp) -> bool:
        link = e3Indication.linkDirection

        # handle no UEs or no available RBs
        if e3Indication.numUe == 0 or e3Indication.numRbg == 0:
            return False

        self.selected_idx_k, self.selected_rnti_k = self.algo_ue_selection.execute(
            e3Indication=e3Indication,
            _hist_link_rnti_subframe=self._hist_link_rnti_subframe,
            _hist_link_rnti_throughput=self._hist_link_rnti_throughput,
        )
        k = len(self.selected_idx_k)

        if k > 0:
            self.assigned_mcs_k = assign_mcs(
                self.rat_type, e3Indication, self.selected_idx_k
            )
            self.start_rbg_k, self.allocated_rbg_k = self._allocate_rbg(e3Indication)

        while k > 0 and (
            k > self._e2_dapp_control[link].maxUeSchedSize
            or sum(self.allocated_rbg_k[:k])
            > min(e3Indication.numRbg, self._e2_dapp_control[link].numAvailableRbg)
        ):
            k -= 1
        self.selected_rnti_k = self.selected_rnti_k[:k]
        self.selected_idx_k = self.selected_idx_k[:k]
        self.assigned_mcs_k = self.assigned_mcs_k[:k]
        self.allocated_rbg_k = self.allocated_rbg_k[:k]
        self.start_rbg_k = self.start_rbg_k[:k]

        if k == 0:
            self._e3_control[link].skipThisStep = True
            self._e3_control[link].ueSchedSize = 0
            if self.save_log:
                self.logger.append(
                    f"Control step: {self.step} | "
                    f"{'DOWNLINK' if link == DOWNLINK else 'UPLINK'} | "
                    f"No allocations made.\n"
                )
        else:
            self._e3_control[link].skipThisStep = False
            self._e3_control[link].ueSchedSize = k
            self._e2_dapp_report[link].ueSchedSize = k
            self._e2_dapp_report[link].numAllocatedRbg = sum(self.allocated_rbg_k)
            self._e2_dapp_report[link].rbgSize = e3Indication.rbgSize
            for i, rnti in enumerate(self.selected_rnti_k):
                self._e3_control[link].ue[i].rnti = rnti
                self._e3_control[link].ue[i].initAllocatedRbg = self.start_rbg_k[i]
                self._e3_control[link].ue[i].numAllocatedRbg = self.allocated_rbg_k[i]
                self._e3_control[link].ue[i].mcs = self.assigned_mcs_k[i]

                pf_tuning_factor = 0.01
                if rnti in self._hist_link_rnti_throughput[f"{link}"].keys():
                    self._hist_link_rnti_throughput[f"{link}"][rnti] = (
                        1 - pf_tuning_factor
                    ) * self._hist_link_rnti_throughput[f"{link}"][
                        rnti
                    ] + pf_tuning_factor * e3Indication.ue[
                        self.selected_idx_k[i]
                    ].actualThroughput
                else:
                    self._hist_link_rnti_throughput[f"{link}"][rnti] = (
                        e3Indication.ue[self.selected_idx_k[i]].actualThroughput
                        + 1e-9  # to avoid zero throughput
                    )

                self._hist_link_rnti_subframe[f"{link}"][rnti] = (
                    e3Indication.frameNo * SUBFRAMES_PER_FRAME + e3Indication.subframeNo
                )

                if self.save_log:
                    self.logger.append(
                        f"Control step: {self.step} | "
                        f"{'DOWNLINK' if link == DOWNLINK else 'UPLINK'} | "
                        f"RNTI={rnti:1d}, "
                        f"{'PRBs' if e3Indication.rbgSize == 1 else 'RBGs'}="
                        f"[{self.start_rbg_k[i]}-{self.start_rbg_k[i]+self.allocated_rbg_k[i]-1}], "
                        f"MCS={self.assigned_mcs_k[i]}\n"
                    )

                self.sum_allocated_rb += self.allocated_rbg_k[i] * e3Indication.rbgSize

        return True

    def write_e3_control(self, py2Cpp, e3Indication) -> None:
        py2Cpp.e3Control.type = E3SM_CONTROL
        py2Cpp.e3Control.ueSchedSize = self._e3_control[
            e3Indication.linkDirection
        ].ueSchedSize
        py2Cpp.e3Control.skipThisStep = self._e3_control[
            e3Indication.linkDirection
        ].skipThisStep
        py2Cpp.e3Control.linkDirection = e3Indication.linkDirection
        py2Cpp.e3Control.id = self.step
        for i in range(py2Cpp.e3Control.ueSchedSize):
            ue = self._e3_control[e3Indication.linkDirection].ue[i]
            py2Cpp.e3Control.ue[i].rnti = ue.rnti
            py2Cpp.e3Control.ue[i].initAllocatedRbg = ue.initAllocatedRbg
            py2Cpp.e3Control.ue[i].numAllocatedRbg = ue.numAllocatedRbg
            py2Cpp.e3Control.ue[i].mcs = ue.mcs

    def write_e2_dapp_report(self, link) -> None:
        self.e2_dapp_.report.type = E2SM_DAPP_REPORT
        self.e2_dapp_.report.ueSchedSize = self._e2_dapp_report[link].ueSchedSize
        self.e2_dapp_.report.numAllocatedRbg = self._e2_dapp_report[
            link
        ].numAllocatedRbg
        self.e2_dapp_.report.rbgSize = self._e2_dapp_report[link].rbgSize
        self.e2_dapp_.report.linkDirection = link
        self.e2_dapp_.report.id = self.step

    def store_e2_dapp_control(self, e2_dapp_control, link) -> None:
        self._e2_dapp_control[link].maxUeSchedSize = e2_dapp_control.maxUeSchedSize
        self._e2_dapp_control[link].numAvailableRbg = e2_dapp_control.numAvailableRbg
        self._e2_dapp_control[link].skipThisStep = e2_dapp_control.skipThisStep
        self._e2_dapp_control[link].linkDirection = e2_dapp_control.linkDirection
        self._e2_dapp_control[link].id = e2_dapp_control.id

    def _allocate_rbg(self, e3Indication):
        allocated_rbg_k = self.rbg_demand.compute(
            e3Indication, self.selected_idx_k, self.assigned_mcs_k
        )
        start_rbg_k = list(accumulate(allocated_rbg_k, initial=e3Indication.initRbg))[
            :-1
        ]
        return start_rbg_k, allocated_rbg_k
