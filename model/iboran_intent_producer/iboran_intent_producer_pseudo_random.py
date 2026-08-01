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

from model.iboran_interfaces.iboran_shm_protocol import LinkDirection
from model.iboran_intent_producer.iboran_intent_producer import IbOranIntentProducer


class IbOranIntentProducerPseudoRandom(IbOranIntentProducer):
    _ID_PREFIX = "PseudoRandom"

    def __init__(self, oid_range: tuple[int, int]):
        super().__init__(oid_range, prod_algo=self._ID_PREFIX)

    def _generate(self, num_intended_oids: int = 1, seed: int = 42, **kwargs) -> None:
        rng = np.random.default_rng(seed)
        intended_oids = rng.integers(
            self.oid_range[0],
            self.oid_range[1] + 1,
            size=(2, num_intended_oids),
            dtype="int",
        ).astype(str)

        self._intents = {
            LinkDirection.UPLINK: intended_oids[0],
            LinkDirection.DOWNLINK: intended_oids[1],
        }
