# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from model.iboran_controller.iboran_dapp_algos.dapp_algo_ue_selection import (
    DAppAlgoUeSelection,
)
from model.iboran_controller.iboran_dapp_algos.dapp_algo_ue_selection_random import (
    DAppAlgoUeSelectionRandom,
)
from model.iboran_controller.iboran_dapp_algos.dapp_algo_ue_selection_round_robin import (
    DAppAlgoUeSelectionRoundRobin,
)

from model.iboran_controller.iboran_dapp_algos.dapp_algo_ue_selection_top_k import (
    DAppAlgoUeSelectionTopK,
)
from model.iboran_controller.iboran_dapp_algos.dapp_algo_ue_selection_greedy import (
    DAppAlgoUeSelectionGreedy,
)
from model.iboran_controller.iboran_dapp_algos.dapp_algo_ue_selection_dp import (
    DAppAlgoUeSelectionDp,
)

_STANDALONE = {
    "Random": DAppAlgoUeSelectionRandom,
    "RoundRobin": DAppAlgoUeSelectionRoundRobin,
}

_FAMILIES = {
    "TopK": DAppAlgoUeSelectionTopK,
    "Greedy": DAppAlgoUeSelectionGreedy,
    "Dp": DAppAlgoUeSelectionDp,
}

_VALID_METRICS = {"Cqi", "BufferLevel", "PropFairness", "Criticality", "MultiCriteria"}

BASE_DAPP_ALGOS_KEYS = set(_STANDALONE) | {
    f"{family}{metric}" for family in _FAMILIES for metric in _VALID_METRICS
}

PROP_FAIRNESS_TUNING_FACTOR = 0.1


def setup_dapp_algo_ue_selection(
    algo_name: str, k: int, intents, rat_type
) -> DAppAlgoUeSelection:
    if algo_name in _STANDALONE:
        return _STANDALONE[algo_name](k, intents, rat_type)

    for prefix, cls in _FAMILIES.items():
        if algo_name.startswith(prefix):
            metric = algo_name[len(prefix) :]
            if metric not in _VALID_METRICS:
                raise ValueError(
                    f"Unknown metric '{metric}' for family '{prefix}'. "
                    f"Valid metrics: {sorted(_VALID_METRICS)}"
                )
            return cls(k, intents, rat_type, metric=metric)

    raise ValueError(
        f"Invalid dApp UE selection algo: '{algo_name}'. "
        f"Valid standalones: {list(_STANDALONE)}. "
        f"Valid families: {list(_FAMILIES)} with metrics {sorted(_VALID_METRICS)}."
    )
