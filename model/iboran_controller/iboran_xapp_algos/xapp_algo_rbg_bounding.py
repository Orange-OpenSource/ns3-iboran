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

from model.iboran_interfaces.iboran_shm_structure import E2KpmIndication
from model.iboran_interfaces.iboran_e2sm_dapp_structure import E2DAppIndication
from model.iboran_interfaces.iboran_shm_protocol import RatType
from model.utils.rbg_demand_calculator import RbgDemandCalculator
from model.utils.mcs_assigner import McsAssigner


class XAppAlgoRbgBounding(ABC):
    _ID_PREFIX: str = ""

    def __init__(self, intents=None, rat_type=RatType.LTE, algo_id=None):
        self.intents = intents
        self.rat_type = rat_type
        self.id = algo_id

    def _reset_variables(self) -> None:
        self.candidate_idx = []
        self.candidate_rnti = []
        self.bounded_rbg = []

        self.rbg_demand_calculator = RbgDemandCalculator(rat_type=self.rat_type)
        self.mcs_assigner = McsAssigner(rat_type=self.rat_type)

    def execute(
        self,
        e2KpmIndication: E2KpmIndication,
        e2DAppIndication: E2DAppIndication,
        **kwargs,
    ) -> tuple[list, list, list]:
        candidates = self._derive_candidates(e2KpmIndication)
        if len(candidates) == 0:
            self.candidate_idx = []
            self.candidate_rnti = []
            self.bounded_rbg = []
            return [], [], []

        done = self.process(candidates, e2KpmIndication, e2DAppIndication, **kwargs)
        if not (done or self._selection_is_verified(e2KpmIndication)):
            raise ValueError(f"{self.id} RBG bounding failed.")

        self.candidate_rnti = [
            e2KpmIndication.ue[idx].rnti for idx in self.candidate_idx
        ]
        return self.candidate_idx, self.candidate_rnti, self.bounded_rbg

    @abstractmethod
    def process(self, candidates, e2KpmIndication, e2DAppIndication, **kwargs) -> bool:
        "Execute the RBG bounding."
        ...

    def _derive_candidates(self, e2KpmIndication) -> list[int]:
        "Derive candidate UEs to be scheduled."
        ues = e2KpmIndication.ue
        candidates = [i for i in range(e2KpmIndication.numUe) if ues[i].bufferSize > 0]

        if self.intents is not None:  # intent-based pre-selection
            intents = self.intents[e2KpmIndication.linkDirection]

            relevant_cands = [i for i in candidates if str(ues[i].contentId) in intents]
            irrelevant_cands = [
                i for i in candidates if str(ues[i].contentId) not in intents
            ]

            accepted = []
            for idx in relevant_cands + irrelevant_cands:
                trial = accepted + [idx]
                if (
                    sum(self._derive_rbg_needed(e2KpmIndication, trial))
                    <= e2KpmIndication.numRbg
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

    def _selection_is_verified(self, e2KpmIndication) -> bool:
        "Validate if the available resources can meet selection requirements."
        while sum(self.bounded_rbg) > e2KpmIndication.numRbg:
            if not self.candidate_idx:
                break
            self.candidate_idx.pop()
            self.candidate_rnti.pop()
            self.bounded_rbg.pop()

        if sum(self.bounded_rbg) > e2KpmIndication.numRbg:
            return False

        return True

    def _derive_rbg_needed(self, e2KpmIndication, input_idx_vect=None) -> np.ndarray:
        provisional_mcs = self.mcs_assigner.assign(
            Cpp2PyReport=e2KpmIndication, input_idx_vect=input_idx_vect
        )
        rbg_needed = self.rbg_demand_calculator.compute(
            Cpp2PyReport=e2KpmIndication,
            input_idx_vect=input_idx_vect,
            assigned_mcs_k=provisional_mcs,
        )
        return rbg_needed
