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
from abc import ABC, abstractmethod

from model.iboran_interfaces.iboran_shm_protocol import LinkDirection


class IbOranIntentProducer(ABC):
    def __init__(self, oid_range: tuple[int, int], prod_algo: str = None):
        if oid_range[0] > oid_range[1]:
            raise ValueError(f"oid_range must be (low, high), got {oid_range}")
        self.oid_range = oid_range
        self.prod_algo = prod_algo

        self._intents: dict[LinkDirection, np.ndarray] | None = None

    @property
    def intents(self) -> dict[LinkDirection, np.ndarray]:
        if self._intents is None:
            raise RuntimeError(f"{type(self).__name__}: intents not yet produced.")
        return self._intents

    def produce(self, **kwargs) -> dict[LinkDirection, np.ndarray]:
        self._generate(**kwargs)
        return self._intents

    @abstractmethod
    def _generate(self, **kwargs) -> None:
        """Generate intents per link direction."""
        ...
