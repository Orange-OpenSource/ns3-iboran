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
from pathlib import Path
from dataclasses import replace

from model.iboran_kpi.iboran_kpi import IbOranKPI, KPI
from model.utils.embedding_store import EmbeddingStore


class IbOranECS(IbOranKPI):
    METRIC_TEMPLATES = {
        **IbOranKPI.METRIC_TEMPLATES,
        "CosineSimilarity": KPI("Cosine Similarity", sort="desc"),
        "CompletenessRatio": KPI("Completeness Ratio", sort="desc"),
    }

    def __init__(self, verbose: bool = False, extraction_model: str = "TransReID"):
        super().__init__(verbose)

        self.metrics = {key: replace(kpi) for key, kpi in self.METRIC_TEMPLATES.items()}

        self.emb_store = EmbeddingStore(extraction_model=extraction_model)

    def _get_metrics(self, input_file: str, output_file: str, **kwargs) -> bool:
        input_path = Path(input_file)
        output_path = Path(output_file)

        try:
            input_emb = self.emb_store.read(
                dir=input_path.parent,
                pattern=input_path.name,
                extension=input_path.suffix,
            )[0]
            output_emb = self.emb_store.read(
                dir=output_path.parent,
                pattern=output_path.name,
                extension=output_path.suffix,
            )[0]
        except (FileNotFoundError, OSError):
            return False

        self.metrics["CosineSimilarity"].measure = self._derive_cosine_similarity(
            input_emb=input_emb, output_emb=output_emb
        )
        self.metrics["CompletenessRatio"].measure = self._derive_completeness_ratio(
            output_emb=output_emb
        )

        if np.isnan(self.metrics["CosineSimilarity"].measure):
            self.metrics["FidelityScore"].measure = 0.0
            return True

        self.metrics["FidelityScore"].measure = (
            self.metrics["CosineSimilarity"].measure
            * self.metrics["CompletenessRatio"].measure
        )
        return True

    def _derive_cosine_similarity(
        self, input_emb: np.ndarray, output_emb: np.ndarray
    ) -> float:
        mask_idx = ~np.isnan(input_emb) & ~np.isnan(output_emb)
        if not mask_idx.any():
            return np.nan
        valid_input_emb, valid_output_emb = input_emb[mask_idx], output_emb[mask_idx]

        denom = np.linalg.norm(valid_input_emb) * np.linalg.norm(valid_output_emb)
        if denom == 0:
            return np.nan

        return float(np.dot(valid_input_emb, valid_output_emb) / denom)

    def _derive_completeness_ratio(self, output_emb: np.ndarray) -> float:
        return float(np.mean(~np.isnan(output_emb)))
