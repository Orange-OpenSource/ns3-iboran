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
from model.iboran_interfaces.iboran_shm_structure import DOWNLINK, UPLINK


class IntentProducer:
    def __init__(self):
        pass

    def do(self, id_range=None, seed=42):
        rng = np.random.default_rng(seed)
        contentId = rng.integers(
            id_range[0], id_range[1] + 1, size=2, dtype="int"
        )  # NOTE: update the apporoach to be more complex
        intents = {UPLINK: str(contentId[0]), DOWNLINK: str(contentId[1])}
        return intents
