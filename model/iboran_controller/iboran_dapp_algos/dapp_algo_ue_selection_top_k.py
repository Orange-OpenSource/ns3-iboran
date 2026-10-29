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


class DAppAlgoUeSelectionTopK(DAppAlgoUeSelection):
    _ID_PREFIX = "TopK"

    def __init__(self, k, intents, rat_type, metric: str):
        super().__init__(k, intents, rat_type, algo_id=f"{self._ID_PREFIX}{metric}")

        self._reset_variables()
        self._bind_metric_fn()

    def process(self, candidates, e3Indication, e2DAppControl, **kwargs) -> bool:
        "Execute the UE selection."
        metric_of_candidates = self._get_metric_of_candidates(
            e3Indication, candidates, **kwargs
        )
        sorted_indices = sorted(
            range(len(candidates)), key=lambda i: metric_of_candidates[i], reverse=True
        )
        self.selected_idx_k = [candidates[i] for i in sorted_indices[: self.k]]

        return True
