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
from .xapp_algo_rbg_bounding import XAppAlgoRbgBounding


class XAppAlgoRbgBoundingClamp(XAppAlgoRbgBounding):
    _ID_PREFIX = "Clamp"

    def __init__(self, intents, rat_type):
        super().__init__(intents, rat_type, algo_id=self._ID_PREFIX)

        self._reset_variables()

    def process(self, candidates, e2KpmReport, e2DAppReport, **kwargs) -> bool:
        "Execute the RBG bounding."
        self.candidate_idx = candidates
        self.bounded_rbg = self._derive_rbg_needed(
            e2KpmReport, input_idx_vect=self.candidate_idx
        )

        return True
