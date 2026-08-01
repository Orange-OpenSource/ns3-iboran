# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md


from pathlib import Path
from abc import ABC, abstractmethod
from dataclasses import dataclass


@dataclass
class KPI:
    label: str
    measure: float = 0.0
    unit: str = ""
    sort: str = "desc"  # or whatever sensible default fits "asc"/"desc"


class IbOranKPI(ABC):
    METRIC_TEMPLATES: dict[str, KPI] = {
        "FidelityScore": KPI("Fidelity Score", sort="desc"),
    }

    def __init__(self, verbose: bool = False):
        self.verbose = verbose

        self.metrics: dict[str, KPI] = {}

    def compute(
        self, input_file: str | Path, output_file: str | Path, **kwargs
    ) -> bool:
        done = self._get_metrics(input_file, output_file, **kwargs)
        if done and not self.metrics:
            raise RuntimeError(
                f"{self.__class__.__name__}._get_metrics() must populate self.metrics"
            )

        if self.verbose and done:
            self._print_metrics_summary()
        return done

    @abstractmethod
    def _get_metrics(
        self, input_file: str | Path, output_file: str | Path, **kwargs
    ) -> bool:
        "Compute KPIs and populate self.metrics. Return True if successful, False otherwise."
        ...

    def _print_metrics_summary(self) -> None:
        for metric in self.metrics.values():
            print(f"{metric.label:<22}: {metric.measure:.2f}")
