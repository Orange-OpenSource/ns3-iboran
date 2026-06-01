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
import os
import torch
from huggingface_hub import snapshot_download
from PIL import Image, ImageFile
from transformers import CLIPModel, CLIPProcessor
from skimage.metrics import structural_similarity as ssim


class IQAMetric:
    def __init__(self):
        ImageFile.LOAD_TRUNCATED_IMAGES = True
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        base_dir = os.path.dirname(os.path.abspath(__file__))
        loaded_model_folder = os.path.abspath(
            os.path.join(base_dir, "../../notebook/clip_model")
        )
        self.clip_model, self.clip_processor = self._load_clip_model_processor(
            model_name="openai/clip-vit-base-patch16",
            local_dir=os.path.join(loaded_model_folder, "clip-vit-base-patch16"),
        )
        self.input_path = None
        self.output_path = None
        self.fidelity_score = None
        self.semantic_fidelity = None
        self.content_fidelity = None
        self.structural_fidelity = None

        self.patch_map = None

    def compute(self, input_path, output_path, diagnosis_needed):
        try:
            self.input_path = Image.open(input_path).convert("RGB")
            self.output_path = Image.open(output_path).convert("RGB")
        except (FileNotFoundError, OSError):
            return False

        patch_semantic_fid, global_semantic_fid, spatial_coverage = (
            self._get_semantic_fidelity(grid_size=4, min_similarity=0.8)
        )
        self._get_structural_fidelity(scales=[224])
        self._get_content_recovery(block_size=16, var_threshold=10)

        # get global and patch semantic fidelity plus spatial coverage
        self.semantic_fidelity = (
            patch_semantic_fid + global_semantic_fid
        ) * spatial_coverage

        # compute final fidelity score
        self.fidelity_score = (
            0.2 * self.semantic_fidelity
            + 0.3 * self.content_fidelity
            + 0.3 * self.structural_fidelity
        )

        if diagnosis_needed:
            self._diagnose_degradation(global_semantic_fid=global_semantic_fid)
        return True

    def _get_semantic_fidelity(self, grid_size=4, min_similarity=0.8):
        size = (224, 224)
        input_path_resized = self.input_path.resize(size)
        output_path_resized = self.output_path.resize(size)

        patch_h = size[0] // grid_size
        patch_w = size[1] // grid_size

        patch_fidelity = []
        self.patch_map = np.zeros((grid_size, grid_size))

        for i in range(grid_size):
            for j in range(grid_size):
                # extract patch
                y1, y2 = i * patch_h, (i + 1) * patch_h
                x1, x2 = j * patch_w, (j + 1) * patch_w
                patch_input = input_path_resized.crop((x1, y1, x2, y2))
                patch_output = output_path_resized.crop((x1, y1, x2, y2))

                # compute CLIP similarity for this patch
                inputs = self.clip_processor(
                    images=[patch_input, patch_output],
                    return_tensors="pt",
                    padding=True,
                )
                inputs = {k: v.to(self.device) for k, v in inputs.items()}
                with torch.no_grad():
                    features = self.clip_model.get_image_features(**inputs)
                    if hasattr(features, "pooler_output"):
                        features = features.pooler_output
                    features = features / features.norm(p=2, dim=-1, keepdim=True)
                    patch_sim = torch.mm(features[0:1], features[1:2].t()).item()

                patch_fidelity.append(patch_sim)
                self.patch_map[i, j] = patch_sim

        patch_semantic_fid = np.mean(patch_fidelity)
        spatial_coverage = np.sum(np.array(patch_fidelity) > min_similarity) / len(
            patch_fidelity
        )

        # global fidelity
        inputs = self.clip_processor(
            images=[self.input_path, self.output_path],
            return_tensors="pt",
            padding=True,
        )
        inputs = {k: v.to(self.device) for k, v in inputs.items()}
        with torch.no_grad():
            features = self.clip_model.get_image_features(**inputs)
            if hasattr(features, "pooler_output"):
                features = features.pooler_output
            features = features / features.norm(p=2, dim=-1, keepdim=True)
            global_semantic_fid = torch.mm(features[0:1], features[1:2].t()).item()

        return patch_semantic_fid, global_semantic_fid, spatial_coverage

    def _get_structural_fidelity(self, scales=[224]):
        size = scales[0]
        gray_input = np.array(self.input_path.convert("L").resize((size, size)))
        gray_output = np.array(self.output_path.convert("L").resize((size, size)))
        self.structural_fidelity = ssim(
            gray_input,
            gray_output,
            data_range=255,
            gaussian_weights=True,
            sigma=1.5,
            use_sample_covariance=False,
            win_size=11,
        )

    def _get_content_recovery(self, block_size=16, var_threshold=10):
        size = (224, 224)
        gray_input = np.array(self.input_path.convert("L").resize(size))
        gray_output = np.array(self.output_path.convert("L").resize(size))

        # divide into blocks and compute variance
        n_blocks_h = size[0] // block_size
        n_blocks_w = size[1] // block_size

        active_blocks_input = 0
        active_blocks_output = 0
        matched_blocks = 0
        for i in range(n_blocks_h):
            for j in range(n_blocks_w):
                y1, y2 = i * block_size, (i + 1) * block_size
                x1, x2 = j * block_size, (j + 1) * block_size
                block_input = gray_input[y1:y2, x1:x2]
                block_output = gray_output[y1:y2, x1:x2]

                var_input = np.var(block_input)
                var_output = np.var(block_output)

                # block is "active" if it has content
                if var_input > var_threshold:
                    active_blocks_input += 1
                    if var_output > var_threshold:
                        matched_blocks += 1

                if var_output > var_threshold:
                    active_blocks_output += 1

        self.content_fidelity = matched_blocks / max(active_blocks_input, 1)

    def _diagnose_degradation(self, global_semantic_fid):
        print(f"{'Semantic fidelity':<22}: {self.semantic_fidelity:.2f}")
        optimism_gap = global_semantic_fid - self.fidelity_score
        print(f"{'CLIP optimism gap':<22}: {optimism_gap:.2f}")
        print(f"{'Content fidelity':<22}: {self.content_fidelity:.2f}")
        print(f"{'Structural fidelity':<22}: {self.structural_fidelity:.2f}")
        print(f"{'Fidelity score':<22}: {self.fidelity_score:.2f}")

        if self.fidelity_score > 0.80:
            perception_quality = "EXCELLENT"
        elif self.fidelity_score > 0.60:
            perception_quality = "GOOD"
        elif self.fidelity_score > 0.40:
            perception_quality = "FAIR"
        else:
            perception_quality = "POOR"
        print(f"{'Perception quality':<22}: {perception_quality}")

    def _load_clip_model_processor(self, model_name, local_dir):
        device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        # download model as a backup if not exists locally
        if not os.path.exists(local_dir):
            snapshot_download(
                repo_id=model_name, local_dir=local_dir, local_dir_use_symlinks=False
            )
        try:
            model = CLIPModel.from_pretrained(local_dir, local_files_only=True)
            model.to(device)
            processor = CLIPProcessor.from_pretrained(
                local_dir, use_fast=True, local_files_only=True
            )
            print("CLIP model: Loaded from local directory.")
            return model, processor

        except Exception as e:
            try:
                model = CLIPModel.from_pretrained(model_name)
                model.to(device)
                processor = CLIPProcessor.from_pretrained(model_name, use_fast=True)
                print("CLIP model: Loaded online.")
                return model, processor

            except Exception as local_e:
                raise Exception(
                    "Could not load CLIP model from online or local. "
                    "Please download the model manually."
                )

    def display_recovery_map(self):
        print()
        for row_count, row in enumerate(self.patch_map):
            if row_count == 0:
                print(f"{'Spacial recovery map':<22}: ", end="")
            else:
                print(f"{'':<24}", end="")
            print("│", end="")
            for val in row:
                if val > 0.8:
                    symbol = "██"  # Excellent recovery
                elif val > 0.6:
                    symbol = "▓▓"  # Good recovery
                elif val > 0.4:
                    symbol = "▒▒"  # Partial recovery
                elif val > 0.2:
                    symbol = "░░"  # Poor recovery
                else:
                    symbol = "  "  # Missing/lost

                print(f"{symbol} {val:.2f} │", end="")
            print()
