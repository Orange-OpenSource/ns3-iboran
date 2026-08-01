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
from .dapp_algo_ue_selection import DAppAlgoUeSelection


class DAppAlgoUeSelectionDp(DAppAlgoUeSelection):
    _ID_PREFIX = "Dp"

    def __init__(self, k, intents, rat_type, metric: str):
        super().__init__(k, intents, rat_type, algo_id=f"{self._ID_PREFIX}{metric}")

        self._reset_variables()
        self._bind_metric_fn()

    def process(self, candidates, e3Indication, **kwargs) -> bool:
        "Execute the UE selection."
        metric_of_candidates = self._get_metric_of_candidates(
            e3Indication, candidates, **kwargs
        )
        rbg_needed_candidates = self._derive_rbg_needed(e3Indication, candidates)

        # derive DP table
        max_rbg = e3Indication.numRbg
        dp = np.zeros((len(candidates) + 1, max_rbg + 1), dtype=float)
        for i in range(len(candidates)):
            for rbg_counter in range(max_rbg + 1):
                if rbg_needed_candidates[i] <= rbg_counter:
                    dp[i + 1][rbg_counter] = max(
                        dp[i][rbg_counter],
                        dp[i][rbg_counter - rbg_needed_candidates[i]]
                        + metric_of_candidates[i],
                    )
                else:
                    dp[i + 1][rbg_counter] = dp[i][rbg_counter]

        # derive selection from DP table
        selection_candidates = np.zeros(len(candidates), dtype=bool)
        rbg_counter = max_rbg
        for i in range(len(candidates) - 1, -1, -1):
            if (
                rbg_needed_candidates[i] <= rbg_counter
                and dp[i + 1][rbg_counter] != dp[i][rbg_counter]
            ):
                selection_candidates[i] = True
                rbg_counter -= rbg_needed_candidates[i]

        self.selected_idx_k = [
            candidates[i] for i in range(len(candidates)) if selection_candidates[i]
        ]

        return True
