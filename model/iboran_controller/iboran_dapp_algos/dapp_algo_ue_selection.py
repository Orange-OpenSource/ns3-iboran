# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from abc import ABC, abstractmethod
import numpy as np
from model.iboran_interfaces.iboran_shm_structure import *
from model.utils.rbg_demand import RbgDemand
from model.utils.assign_mcs import assign_mcs
import time


class DAppAlgoUeSelection(ABC):
    def __init__(self, k, intents=None, rat_type=LTE):
        self.k = k
        self.intents = intents
        self.rat_type = rat_type

        self.id = None

        self.contentId = "0"

    def _reset_variables(self):
        self.selected_idx_k = []
        self.selected_rnti_k = []

        self.rbg_demand = RbgDemand(rat_type=self.rat_type)

        self.weight_matrix = {
            DOWNLINK: np.array(
                [
                    [1 / 1, 1 / 1, 3 / 1, 4 / 1],  # fairness
                    [1 / 1, 1 / 1, 3 / 1, 4 / 1],  # criticality
                    [1 / 3, 1 / 3, 1 / 1, 1 / 1],  # buffer size
                    [1 / 4, 1 / 4, 1 / 1, 1 / 1],  # cqi
                ]
            ),
            UPLINK: np.array(
                [
                    [1 / 1, 1 / 1, 3 / 1, 4 / 1],  # fairness
                    [1 / 1, 1 / 1, 3 / 1, 4 / 1],  # criticality
                    [1 / 3, 1 / 3, 1 / 1, 2 / 1],  # buffer size
                    [1 / 4, 1 / 4, 1 / 2, 1 / 1],  # cqi
                ]
            ),
        }
        self.weights = {DOWNLINK: None, UPLINK: None}

        self._metric_fn = None

    def execute(self, e3Indication: E3Indication, **kwargs):
        candidates = self._derive_candidates(e3Indication)
        self.k = min(self.k, len(candidates))
        if self.k == 0:
            self.selected_idx_k = []
            self.selected_rnti_k = []
            return [], []

        done = self.process(candidates, e3Indication, **kwargs)
        if not (done or self._selection_is_verified(e3Indication)):
            raise ValueError(f"{self.id} UE selection failed.")

        self.selected_rnti_k = [
            e3Indication.ue[idx].rnti for idx in self.selected_idx_k
        ]
        return self.selected_idx_k, self.selected_rnti_k

    @abstractmethod
    def process(self, candidates, e3Indication, **kwargs) -> bool:
        "Execute the UE selection."
        ...

    def _derive_candidates(self, e3Indication):
        "Derive candidate UEs to be scheduled."
        ues = e3Indication.ue
        candidates = [i for i in range(e3Indication.numUe) if ues[i].bufferLevel > 0]

        if self.intents is not None:  # intent-based pre-selection
            intent = self.intents[e3Indication.linkDirection]

            relevant_cands = [i for i in candidates if ues[i].contentId == intent]
            irrelevant_cands = [i for i in candidates if ues[i].contentId != intent]

            accepted = []
            for idx in relevant_cands + irrelevant_cands:
                trial = accepted + [idx]
                if (
                    sum(self._derive_rbg_needed(e3Indication, trial))
                    <= e3Indication.numRbg
                ):
                    accepted = trial
                elif idx not in relevant_cands:
                    break  # once irrelevant UEs start overflowing, stop trying them

            candidates = (
                accepted
                if accepted
                else relevant_cands if relevant_cands else irrelevant_cands
            )

        if not candidates:
            return []
        return candidates

    def _selection_is_verified(self, e3Indication):
        "Validate if the available resources can meet selection requirements."
        rbg_needed_k = self._derive_rbg_needed(e3Indication, self.selected_idx_k)
        while sum(rbg_needed_k) > e3Indication.numRbg:
            if not self.selected_idx_k:
                break
            self.selected_idx_k.pop()
            self.selected_rnti_k.pop()
            rbg_needed_k = self._derive_rbg_needed(e3Indication, self.selected_idx_k)
        return True

    def _derive_rbg_needed(self, e3Indication, input_idx_vect=None):
        provisional_mcs = assign_mcs(self.rat_type, e3Indication, input_idx_vect)
        rbg_needed = self.rbg_demand.compute(
            e3Indication, input_idx_vect, provisional_mcs
        )
        return rbg_needed

    # ── candidates metric calculation ──────────────────────────────────────────────

    def _bind_metric_fn(self):
        dispatch = {
            "Cqi": self._metric_cqi,
            "Criticality": self._metric_criticality,
            "BufferLevel": self._metric_buffer_size,
            "PropFairness": self._metric_prop_fairness,
            "MultiCriteria": self._metric_multi_criteria,
        }
        for prefix in ("TopK", "Greedy", "Dp"):
            if self.id.startswith(prefix):
                metric = self.id[len(prefix) :]
                break
        else:
            raise ValueError(f"Unknown algo id: {self.id}")

        if metric not in dispatch:
            raise ValueError(f"Unknown metric '{metric}' in id '{self.id}'")
        self._metric_fn = dispatch[metric]

    def _metric_cqi(self, e3Indication, candidates, **kwargs):
        return [e3Indication.ue[i].cqi for i in candidates]

    def _metric_criticality(self, e3Indication, candidates, **kwargs):
        return [e3Indication.ue[i].contentCriticality for i in candidates]

    def _metric_buffer_size(self, e3Indication, candidates, **kwargs):
        return [e3Indication.ue[i].bufferLevel for i in candidates]

    def _metric_prop_fairness(self, e3Indication, candidates, **kwargs):
        _hist_link_rnti_throughput = kwargs.get("_hist_link_rnti_throughput", {})
        return [
            e3Indication.ue[i].achievableRate
            / _hist_link_rnti_throughput.get(f"{e3Indication.linkDirection}", {}).get(
                e3Indication.ue[i].rnti,
                1e-9,  # to avoid division by zero for new UEs
            )
            for i in candidates
        ]

    def _metric_multi_criteria(self, e3Indication, candidates, **kwargs):
        _hist_link_rnti_throughput = kwargs.get("_hist_link_rnti_throughput", {})
        fairness = [
            e3Indication.ue[i].achievableRate
            / _hist_link_rnti_throughput.get(f"{e3Indication.linkDirection}", {}).get(
                e3Indication.ue[i].rnti,
                1e-9,  # to avoid division by zero for new UEs
            )
            for i in candidates
        ]

        criticality = [e3Indication.ue[i].contentCriticality for i in candidates]
        buffer_size = [e3Indication.ue[i].bufferLevel for i in candidates]
        cqi = [e3Indication.ue[i].cqi for i in candidates]

        self.weights[e3Indication.linkDirection] = self._get_weights(
            e3Indication.linkDirection
        )
        return (
            self.weights[e3Indication.linkDirection][0] * self._normalize(fairness)
            + self.weights[e3Indication.linkDirection][1] * self._normalize(criticality)
            + self.weights[e3Indication.linkDirection][2] * self._normalize(buffer_size)
            + self.weights[e3Indication.linkDirection][3] * self._normalize(cqi)
        )

    def _get_metric_of_candidates(self, e3Indication, candidates, **kwargs):
        return self._metric_fn(e3Indication, candidates, **kwargs)

    def _get_weights(self, link_direction):
        if self.weights[link_direction] is None:
            eigenvalues, eigenvectors = np.linalg.eig(
                self.weight_matrix[link_direction]
            )
            lambda_max = np.real(eigenvalues.max())
            n_criteria = self.weight_matrix[link_direction].shape[0]

            # Consistency Index (CI)
            consistency_index = (lambda_max - n_criteria) / (n_criteria - 1)
            # Random Index (RI) reference
            random_index_dict = {1: 0, 2: 0, 3: 0.58, 4: 0.90, 5: 1.12}
            random_index = random_index_dict.get(n_criteria)
            # Consistency Ratio (CR)
            consistency_ratio = consistency_index / random_index
            if consistency_ratio > 0.1:
                raise ValueError(
                    f"{self.id} Crtiteria weight matrix is not consistent."
                )

            # get the corresponding eigenvector and normalize it
            index = np.argmax(eigenvalues)
            weights = np.real(eigenvectors[:, index])
            self.weights[link_direction] = weights / weights.sum()
        return self.weights[link_direction]

    def _normalize(self, values):
        array = np.array(values)
        if array.max() == array.min():
            return np.ones_like(array) / len(array)
        return (array - array.min()) / (array.max() - array.min())
