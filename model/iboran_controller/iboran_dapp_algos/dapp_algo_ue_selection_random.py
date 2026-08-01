# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

import random
from .dapp_algo_ue_selection import DAppAlgoUeSelection


class DAppAlgoUeSelectionRandom(DAppAlgoUeSelection):
    _ID_PREFIX = "Random"

    def __init__(self, k, intents, rat_type):
        super().__init__(k, intents, rat_type, algo_id=self._ID_PREFIX)

        self._reset_variables()

    def process(self, candidates, e3Indication, **kwargs) -> bool:
        "Execute the UE selection."
        self.selected_idx_k = random.sample(candidates, self.k)

        return True
