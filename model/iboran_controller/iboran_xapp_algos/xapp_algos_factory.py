# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from model.iboran_controller.iboran_xapp_algos.xapp_algo_rbg_bounding import (
    XAppAlgoRbgBounding,
)
from model.iboran_controller.iboran_xapp_algos.xapp_algo_rbg_bounding_bypass import (
    XAppAlgoRbgBoundingBypass,
)

# Standalone algos with no metric variation
_STANDALONE = {
    "Bypass": XAppAlgoRbgBoundingBypass,
}

# Parametric families: prefix → class
_FAMILIES = {}

_VALID_METRICS = {}

BASE_XAPP_ALGOS_KEYS = set(_STANDALONE) | {
    f"{family}{metric}" for family in _FAMILIES for metric in _VALID_METRICS
}


def setup_xapp_algo_rbg_bounding(
    algo_name: str, intents, rat_type
) -> XAppAlgoRbgBounding:
    if algo_name in _STANDALONE:
        return _STANDALONE[algo_name](intents, rat_type)

    for prefix, cls in _FAMILIES.items():
        if algo_name.startswith(prefix):
            metric = algo_name[len(prefix) :]
            if metric not in _VALID_METRICS:
                raise ValueError(
                    f"Unknown metric '{metric}' for family '{prefix}'. "
                    f"Valid metrics: {sorted(_VALID_METRICS)}"
                )
            return cls(intents, rat_type, metric=metric)

    raise ValueError(
        f"Invalid xApp RBG bounding algo: '{algo_name}'. "
        f"Valid standalones: {list(_STANDALONE)}. "
        f"Valid families: {list(_FAMILIES)} with metrics {sorted(_VALID_METRICS)}."
    )
