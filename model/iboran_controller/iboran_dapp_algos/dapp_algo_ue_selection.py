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
from model.iboran_interfaces.iboran_shm_structure import E3Indication
from model.iboran_interfaces.iboran_e2sm_dapp_structure import E2DAppControl
from model.iboran_interfaces.iboran_shm_protocol import LinkDirection, RatType
from model.utils.rbg_demand_calculator import RbgDemandCalculator
from model.utils.mcs_assigner import McsAssigner


class DAppAlgoUeSelection(ABC):
    _ID_PREFIX: str = ""

    def __init__(self, k, intents=None, rat_type=RatType.LTE, algo_id=None):
        self.k = k
        self.k_max = k  # configured k; self.k is the effective value of each step
        self.intents = intents
        self.rat_type = rat_type
        self.id = algo_id

    def _reset_variables(self) -> None:
        self.selected_idx_k = []
        self.selected_rnti_k = []

        self.rbg_demand_calculator = RbgDemandCalculator(rat_type=self.rat_type)
        self.mcs_assigner = McsAssigner(rat_type=self.rat_type)

        self.weight_matrix = {
            LinkDirection.DOWNLINK: np.array(
                [
                    [1 / 1, 1 / 1, 3 / 1, 4 / 1],  # fairness
                    [1 / 1, 1 / 1, 3 / 1, 4 / 1],  # criticality
                    [1 / 3, 1 / 3, 1 / 1, 1 / 1],  # buffer size
                    [1 / 4, 1 / 4, 1 / 1, 1 / 1],  # cqi
                ]
            ),
            LinkDirection.UPLINK: np.array(
                [
                    [1 / 1, 1 / 1, 3 / 1, 4 / 1],  # fairness
                    [1 / 1, 1 / 1, 3 / 1, 4 / 1],  # criticality
                    [1 / 3, 1 / 3, 1 / 1, 2 / 1],  # buffer size
                    [1 / 4, 1 / 4, 1 / 2, 1 / 1],  # cqi
                ]
            ),
        }
        self.weights = {LinkDirection.DOWNLINK: None, LinkDirection.UPLINK: None}

        self._metric_fn = None

    def execute(
        self, e3Indication: E3Indication, e2DAppControl: E2DAppControl, **kwargs
    ) -> tuple[list, list]:
        candidates = self._derive_candidates(e3Indication)
        skipped = kwargs.get("skipped_rntis", set())
        if skipped:
            candidates = [
                i for i in candidates if e3Indication.ue[i].rnti not in skipped
            ]

        self.k = min(self.k_max, len(candidates))
        if self.k == 0:
            self.selected_idx_k = []
            self.selected_rnti_k = []
            return [], []

        done = self.process(candidates, e3Indication, e2DAppControl, **kwargs)
        if not (done or self._selection_is_verified(e3Indication)):
            raise ValueError(f"{self.id} UE selection failed.")

        self.selected_rnti_k = [
            e3Indication.ue[idx].rnti for idx in self.selected_idx_k
        ]
        return self.selected_idx_k, self.selected_rnti_k

    @abstractmethod
    def process(self, candidates, e3Indication, e2DAppControl, **kwargs) -> bool:
        "Execute the UE selection."
        ...

    def _derive_candidates(self, e3Indication) -> list[int]:
        "Derive candidate UEs to be scheduled."
        ues = e3Indication.ue
        candidates = [i for i in range(e3Indication.numUe) if ues[i].bufferLevel > 0]

        if self.intents is not None:  # intent-based pre-selection
            intents = self.intents[e3Indication.linkDirection]

            relevant_cands = [i for i in candidates if str(ues[i].contentId) in intents]
            irrelevant_cands = [
                i for i in candidates if str(ues[i].contentId) not in intents
            ]

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

    def _selection_is_verified(self, e3Indication) -> bool:
        "Validate if the available resources can meet selection requirements."
        rbg_needed_k = self._derive_rbg_needed(e3Indication, self.selected_idx_k)
        while sum(rbg_needed_k) > e3Indication.numRbg:
            if not self.selected_idx_k:
                break
            self.selected_idx_k.pop()
            self.selected_rnti_k.pop()
            rbg_needed_k = self._derive_rbg_needed(e3Indication, self.selected_idx_k)

        if sum(rbg_needed_k) > e3Indication.numRbg:
            return False

        return True

    def _derive_rbg_needed(self, e3Indication, input_idx_vect=None) -> np.ndarray:
        provisional_mcs = self.mcs_assigner.assign(
            Cpp2PyReport=e3Indication, input_idx_vect=input_idx_vect
        )
        rbg_needed = self.rbg_demand_calculator.compute(
            Cpp2PyReport=e3Indication,
            input_idx_vect=input_idx_vect,
            assigned_mcs_k=provisional_mcs,
        )
        return rbg_needed

    def _bind_metric_fn(self) -> None:
        dispatch = {
            "Cqi": self._metric_cqi,
            "Criticality": self._metric_criticality,
            "BufferLevel": self._metric_buffer_size,
            "PropFairness": self._metric_prop_fairness,
            "MultiCriteria": self._metric_multi_criteria,
        }
        metric = self.id[len(self._ID_PREFIX) :]
        if metric not in dispatch:
            raise ValueError(f"Unknown metric '{metric}' in id '{self.id}'")
        self._metric_fn = dispatch[metric]

    def _metric_cqi(self, e3Indication, candidates, **kwargs) -> list[float]:
        return [e3Indication.ue[i].cqi for i in candidates]

    def _metric_criticality(self, e3Indication, candidates, **kwargs) -> list[float]:
        return [e3Indication.ue[i].contentCriticality for i in candidates]

    def _metric_buffer_size(self, e3Indication, candidates, **kwargs) -> list[float]:
        return [e3Indication.ue[i].bufferLevel for i in candidates]

    def _metric_prop_fairness(self, e3Indication, candidates, **kwargs) -> list[float]:
        _hist_link_rnti_tps = kwargs.get("_hist_link_rnti_tps", {})
        return [
            e3Indication.ue[i].achievableRate
            / _hist_link_rnti_tps.get(e3Indication.linkDirection, {}).get(
                e3Indication.ue[i].rnti,
                1e-9,  # to avoid division by zero for new UEs
            )
            for i in candidates
        ]

    def _metric_multi_criteria(self, e3Indication, candidates, **kwargs) -> np.ndarray:
        _hist_link_rnti_tps = kwargs.get("_hist_link_rnti_tps", {})
        fairness = [
            e3Indication.ue[i].achievableRate
            / _hist_link_rnti_tps.get(e3Indication.linkDirection, {}).get(
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

    def _get_metric_of_candidates(
        self, e3Indication, candidates, **kwargs
    ) -> list[float]:
        return self._metric_fn(e3Indication, candidates, **kwargs)

    def _get_weights(self, link_direction) -> np.ndarray:
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
            if random_index is None:
                raise ValueError(
                    f"No AHP random index defined for {n_criteria} criteria."
                )

            # Consistency Ratio (CR)
            if n_criteria <= 2:
                # AHP consistency check is undefined/moot for fewer than 3 criteria
                pass  # skip the CR check
            else:
                consistency_ratio = consistency_index / random_index
                if consistency_ratio > 0.1:
                    raise ValueError(
                        f"{self.id} Criteria weight matrix is not consistent."
                    )

            # get the corresponding eigenvector and normalize it
            index = np.argmax(eigenvalues)
            weights = np.real(eigenvectors[:, index])
            self.weights[link_direction] = weights / weights.sum()
        return self.weights[link_direction]

    def _normalize(self, values) -> np.ndarray:
        array = np.array(values)
        if array.max() == array.min():
            return np.ones_like(array) / len(array)
        return (array - array.min()) / (array.max() - array.min())
