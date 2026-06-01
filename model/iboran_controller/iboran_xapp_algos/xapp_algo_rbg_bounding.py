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


class XAppAlgoRbgBounding(ABC):
    def __init__(self, intents=None, rat_type=LTE):
        self.intents = intents
        self.rat_type = rat_type

        self.id = None

        self.contentId = "0"

    def _reset_variables(self):
        self.candidated_idx = []
        self.candidated_rnti = []
        self.bounded_rbg = []

        self.rbg_demand = RbgDemand(rat_type=self.rat_type)

    def execute(self, e2KpmReport: E2KpmReport, **kwargs):
        candidates = self._derive_candidates(e2KpmReport)
        if len(candidates) == 0:
            self.candidated_idx = []
            self.candidated_rnti = []
            self.bounded_rbg = []
            return [], [], []

        done = self.process(candidates, e2KpmReport, **kwargs)
        if not (done or self._selection_is_verified(e2KpmReport)):
            raise ValueError(f"{self.id} RBG bounding failed.")

        self.candidated_rnti = [e2KpmReport.ue[idx].rnti for idx in self.candidated_idx]
        return self.candidated_idx, self.candidated_rnti, self.bounded_rbg

    @abstractmethod
    def process(self, candidates, e2KpmReport, **kwargs) -> bool:
        "Execute the RBG bounding."
        ...

    def _derive_candidates(self, e2KpmReport):
        "Derive candidate UEs to be scheduled."
        ues = e2KpmReport.ue
        candidates = [i for i in range(e2KpmReport.numUe) if ues[i].bufferLevel > 0]

        if self.intents is not None:  # intent-based pre-selection
            intent = self.intents[e2KpmReport.linkDirection]

            relevant_cands = [i for i in candidates if ues[i].contentId == intent]
            irrelevant_cands = [i for i in candidates if ues[i].contentId != intent]

            accepted = []
            for idx in relevant_cands + irrelevant_cands:
                trial = accepted + [idx]
                if (
                    sum(self._derive_rbg_needed(e2KpmReport, trial))
                    <= e2KpmReport.numRbg
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

    def _selection_is_verified(self, e2KpmReport):
        "Validate if the available resources can meet selection requirements."
        while sum(self.bounded_rbg) > e2KpmReport.numRbg:
            if not self.candidated_idx:
                break
            self.candidated_idx.pop()
            self.candidated_rnti.pop()
            self.bounded_rbg.pop()
        return True

    def _derive_rbg_needed(self, e2KpmReport, input_idx_vect=None):
        provisional_mcs = assign_mcs(self.rat_type, e2KpmReport, input_idx_vect)
        rbg_needed = self.rbg_demand.compute(
            e2KpmReport, input_idx_vect, provisional_mcs
        )
        return rbg_needed
