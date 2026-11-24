# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from .dapp_algo_ue_selection import DAppAlgoUeSelection


class DAppAlgoUeSelectionRoundRobin(DAppAlgoUeSelection):
    _ID_PREFIX = "RoundRobin"

    def __init__(self, k, intents, rat_type):
        super().__init__(k, intents, rat_type, algo_id=self._ID_PREFIX)

        self._reset_variables()

    def process(self, candidates, e3Indication, e2DAppControlRequest, **kwargs) -> bool:
        "Execute the UE selection."
        _hist_link_rnti_subframe = kwargs.get("_hist_link_rnti_subframe", {})
        last_ms = _hist_link_rnti_subframe.get(e3Indication.linkDirection, {})
        # last scheduling time [ms]; a UE never scheduled comes first (-1)
        subframe_candidates = [
            last_ms.get(e3Indication.ue[i].rnti, -1) for i in candidates
        ]

        sorted_indices = sorted(
            range(len(candidates)),
            key=lambda i: subframe_candidates[i],
            reverse=False,  # the earlier a UE is scheduled, the higher priority it gets.
        )
        self.selected_idx_k = [candidates[i] for i in sorted_indices[: self.k]]

        return True
