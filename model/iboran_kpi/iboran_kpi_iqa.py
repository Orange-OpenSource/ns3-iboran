# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md


import os
import numpy as np
from dataclasses import replace

import torch
from PIL import Image
from skimage.metrics import structural_similarity as ssim

from model.iboran_kpi.iboran_kpi import IbOranKPI, KPI
from model.utils.embedding_store import CLIPBackend


class IbOranIQA(IbOranKPI):
    METRIC_TEMPLATES = {
        **IbOranKPI.METRIC_TEMPLATES,
        "SemanticFidelity": KPI("Semantic Fidelity", sort="desc"),
        "ContentFidelity": KPI("Content Fidelity", sort="desc"),
        "StructuralFidelity": KPI("Structural Fidelity", sort="desc"),
    }

    def __init__(self, verbose: bool = False):
        super().__init__(verbose)

        if torch.cuda.is_available():
            device = torch.device("cuda")
        elif torch.backends.mps.is_available():
            device = torch.device("mps")
        else:
            device = torch.device("cpu")

        abs_path = os.path.dirname(os.path.abspath(__file__))
        base_dir = os.path.abspath(os.path.join(abs_path, "../../notebook"))
        self._clip = CLIPBackend(device, base_dir=base_dir)

        self.resize_dim = (224, 224)

        self.metrics = {key: replace(kpi) for key, kpi in self.METRIC_TEMPLATES.items()}
        self.patch_map = np.zeros((0, 0))

    def _get_metrics(self, input_file: str, output_file: str, **kwargs) -> bool:
        try:
            input_image = Image.open(input_file).convert("RGB")
            output_image = Image.open(output_file).convert("RGB")
        except (FileNotFoundError, OSError):
            return False

        self._derive_semantic_fidelity(
            input_image=input_image,
            output_image=output_image,
            grid_size=4,
            min_similarity=0.8,
        )
        self._derive_structural_fidelity(
            input_image=input_image, output_image=output_image
        )
        self._derive_content_fidelity(
            input_image=input_image,
            output_image=output_image,
            block_size=16,
            var_threshold=10,
        )

        self.metrics["FidelityScore"].measure = (
            0.4 * self.metrics["SemanticFidelity"].measure
            + 0.3 * self.metrics["ContentFidelity"].measure
            + 0.3 * self.metrics["StructuralFidelity"].measure
        )

        return True

    def print_spatial_map(self) -> None:
        symbols = [
            (0.8, "██"),  # Excellent recovery
            (0.6, "▓▓"),  # Good recovery
            (0.4, "▒▒"),  # Partial recovery
            (0.2, "░░"),  # Poor recovery
        ]
        for row_count, row in enumerate(self.patch_map):
            label = "Spatial recovery map" if row_count == 0 else ""
            print(f"{label:<22}: " if row_count == 0 else f"{'':<24}", end="")
            print("│", end="")
            for val in row:
                symbol = next((s for thresh, s in symbols if val > thresh), "  ")
                print(f"{symbol} {val:.2f} │", end="")
            print()

    def _clip_embed(self, images: list[Image.Image]) -> np.ndarray:
        """Run the shared CLIP extraction path on a small in-memory image batch."""
        return self._clip.extract_images(images)

    def _derive_semantic_fidelity(
        self,
        input_image: Image.Image,
        output_image: Image.Image,
        grid_size: int = 4,
        min_similarity: float = 0.8,
    ) -> None:
        input_image_resized = input_image.resize(self.resize_dim)
        output_image_resized = output_image.resize(self.resize_dim)
        patch_h = self.resize_dim[0] // grid_size
        patch_w = self.resize_dim[1] // grid_size

        patch_fidelity = []
        self.patch_map = np.zeros((grid_size, grid_size))

        for i in range(grid_size):
            for j in range(grid_size):
                y1, y2 = i * patch_h, (i + 1) * patch_h
                x1, x2 = j * patch_w, (j + 1) * patch_w
                patch_input = input_image_resized.crop((x1, y1, x2, y2))
                patch_output = output_image_resized.crop((x1, y1, x2, y2))

                features = self._clip_embed([patch_input, patch_output])
                patch_sim = float(np.dot(features[0], features[1]))

                patch_fidelity.append(patch_sim)
                self.patch_map[i, j] = patch_sim

        patch_sem_fidelity = np.mean(patch_fidelity)
        spatial_coverage = np.sum(np.array(patch_fidelity) > min_similarity) / len(
            patch_fidelity
        )

        global_features = self._clip_embed([input_image, output_image])
        global_sem_fidelity = float(np.dot(global_features[0], global_features[1]))

        self.metrics["SemanticFidelity"].measure = (
            (patch_sem_fidelity + global_sem_fidelity) * spatial_coverage / 2
        )

    def _derive_structural_fidelity(
        self, input_image: Image.Image, output_image: Image.Image
    ) -> None:
        gray_input = np.array(input_image.convert("L").resize(self.resize_dim))
        gray_output = np.array(output_image.convert("L").resize(self.resize_dim))

        self.metrics["StructuralFidelity"].measure = ssim(
            gray_input,
            gray_output,
            data_range=255,
            gaussian_weights=True,
            sigma=1.5,
            use_sample_covariance=False,
            win_size=11,
        )

    def _derive_content_fidelity(
        self,
        input_image: Image.Image,
        output_image: Image.Image,
        block_size: int = 16,
        var_threshold: float = 10,
    ) -> None:
        gray_input = np.array(input_image.convert("L").resize(self.resize_dim))
        gray_output = np.array(output_image.convert("L").resize(self.resize_dim))
        n_blocks_h = self.resize_dim[0] // block_size
        n_blocks_w = self.resize_dim[1] // block_size

        active_blocks_input = 0
        matched_blocks = 0
        for i in range(n_blocks_h):
            for j in range(n_blocks_w):
                y1, y2 = i * block_size, (i + 1) * block_size
                x1, x2 = j * block_size, (j + 1) * block_size
                block_input = gray_input[y1:y2, x1:x2]
                block_output = gray_output[y1:y2, x1:x2]

                var_input = np.var(block_input)
                var_output = np.var(block_output)

                if var_input > var_threshold:
                    active_blocks_input += 1
                    if var_output > var_threshold:
                        matched_blocks += 1

        self.metrics["ContentFidelity"].measure = matched_blocks / max(
            active_blocks_input, 1
        )
