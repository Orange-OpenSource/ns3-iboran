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
import sys
import io
import contextlib
import importlib
import subprocess
import types
import collections.abc
from abc import ABC, abstractmethod
from pathlib import Path

import numpy as np
import torch
from PIL import Image, ImageFile
from huggingface_hub import snapshot_download
from transformers import CLIPModel, CLIPProcessor
from torchvision import transforms


class EmbeddingStore:
    def __init__(self, extraction_model: str = "CLIP"):

        if torch.cuda.is_available():
            self.device = torch.device("cuda")
        elif torch.backends.mps.is_available():
            self.device = torch.device("mps")
        else:
            self.device = torch.device("cpu")

        base_dir = os.path.dirname(os.path.abspath(__file__))
        self._backend = BackendRegistry.create(
            extraction_model, self.device, base_dir=base_dir
        )

    @property
    def embedding_dim(self) -> int:
        return self._backend.embedding_dim

    def extract(
        self, dir: Path, pattern: str, extension: str = ".png", batch_size: int = 32
    ) -> bool:
        paths = self._find_file(dir, pattern=pattern, extension=extension)
        if not paths:
            raise FileNotFoundError(f"No files matching '{pattern}' in {dir}.")

        all_embs = [
            self._backend.extract(paths[i : i + batch_size])
            for i in range(0, len(paths), batch_size)
        ]
        embs = np.concatenate(all_embs, axis=0)
        self._write_embs_to_f32(embs, paths, dir)
        return True

    def read(
        self,
        dir: Path,
        pattern: str,
        extension: str = ".f32",
        expected_dim: int | None = None,
    ) -> np.ndarray:
        output_files = self._find_file(dir, pattern=pattern, extension=extension)
        if not output_files:
            raise FileNotFoundError(f"No files matching '{pattern}' in {dir}.")

        arrays = [
            np.fromfile(dir / f"{p.stem}.f32", dtype=np.float32) for p in output_files
        ]

        dim = expected_dim or self.embedding_dim
        padded = np.full((len(arrays), dim), np.nan, dtype=np.float32)
        for i, a in enumerate(arrays):
            n = min(a.shape[0], dim)
            padded[i, :n] = a[:n]

        return padded

    def _find_file(
        self, dataset: Path, pattern: str = "input_*", extension: str = ".png"
    ) -> list[Path]:
        extensions = (extension,) if isinstance(extension, str) else tuple(extension)
        return sorted(
            p
            for p in dataset.glob(pattern)
            if p.is_file() and p.suffix.lower() in extensions
        )

    def _write_embs_to_f32(
        self, embeddings: np.ndarray, image_paths: list[Path], write_dir: Path
    ) -> None:
        write_dir = Path(write_dir)
        write_dir.mkdir(parents=True, exist_ok=True)
        for i in range(embeddings.shape[0]):
            embeddings[i].astype("<f4").tofile(write_dir / f"{image_paths[i].stem}.f32")


class EmbeddingBackend(ABC):
    "Common interface every extraction backend must implement."

    def __init__(self, device: torch.device):
        self.device = device

    @property
    @abstractmethod
    def embedding_dim(self) -> int:
        "The fixed output dimension of this backend's embeddings."
        raise NotImplementedError

    @abstractmethod
    def extract(self, paths: list[Path]) -> np.ndarray:
        "Given a list of image paths, return an (N, D) float32 array of embeddings."
        raise NotImplementedError


class BackendRegistry:
    _registry: dict[str, type[EmbeddingBackend]] = {}

    @classmethod
    def register(cls, name: str):
        def decorator(backend_cls: type[EmbeddingBackend]):
            cls._registry[name] = backend_cls
            return backend_cls

        return decorator

    @classmethod
    def create(cls, name: str, device: torch.device, **kwargs) -> EmbeddingBackend:
        if name not in cls._registry:
            available = ", ".join(sorted(cls._registry)) or "none registered"
            raise ValueError(
                f"Unsupported extraction model: {name}. Available: {available}"
            )
        return cls._registry[name](device, **kwargs)


@BackendRegistry.register("CLIP")
class CLIPBackend(EmbeddingBackend):
    def __init__(self, device: torch.device, base_dir: str):
        super().__init__(device)
        ImageFile.LOAD_TRUNCATED_IMAGES = True
        offline_model_dir = os.path.abspath(
            os.path.join(base_dir, "../../notebook/models/emb/CLIP")
        )
        self.model, self.processor = self._load(
            model_name="openai/clip-vit-base-patch16",
            local_dir=os.path.join(offline_model_dir, "clip-vit-base-patch16"),
        )

    @property
    def embedding_dim(self) -> int:
        return self.model.config.projection_dim

    def extract(self, paths: list[Path]) -> np.ndarray:
        imgs = [Image.open(p).convert("RGB") for p in paths]
        return self.extract_images(imgs)

    def extract_images(self, images: list[Image.Image]) -> np.ndarray:
        inputs = self.processor(images=images, return_tensors="pt").to(self.device)
        with torch.no_grad():
            vision_outputs = self.model.vision_model(
                pixel_values=inputs["pixel_values"]
            )
            feats = self.model.visual_projection(vision_outputs.pooler_output)
        feats = torch.nn.functional.normalize(feats, dim=-1)
        return feats.cpu().to(torch.float32).numpy()

    def _load(self, model_name: str, local_dir: str):
        if not os.path.exists(local_dir):
            snapshot_download(
                repo_id=model_name, local_dir=local_dir, local_dir_use_symlinks=False
            )
        try:
            model = CLIPModel.from_pretrained(local_dir, local_files_only=True)
            model.to(self.device)
            processor = CLIPProcessor.from_pretrained(
                local_dir, backend="torchvision", local_files_only=True
            )
            print("CLIP model: Loaded from local directory.")
            return model, processor
        except Exception as first_exc:
            try:
                model = CLIPModel.from_pretrained(model_name)
                model.to(self.device)
                processor = CLIPProcessor.from_pretrained(
                    model_name, backend="torchvision"
                )
                print("CLIP model: Loaded online.")
                return model, processor
            except Exception as local_e:
                raise RuntimeError(
                    "Could not load CLIP model from online or local. "
                    "Please download the model manually."
                ) from local_e


@BackendRegistry.register("TransReID")
class TransReIDBackend(EmbeddingBackend):
    def __init__(self, device: torch.device, base_dir: str):
        super().__init__(device)
        local_dir = os.path.abspath(
            os.path.join(base_dir, "../../notebook/models/emb/TransReID")
        )
        self.model, self.transform = self._load(
            model_name="vit_transreid_stride",
            patch_name="vit_transreid_market.pth",
            local_dir=local_dir,
        )
        self._embedding_dim = None

    @property
    def embedding_dim(self) -> int:
        if self._embedding_dim is None:
            dummy = torch.zeros(1, 3, *self.transform.transforms[0].size).to(
                self.device
            )
            cam_label = torch.zeros(1, dtype=torch.long, device=self.device)
            with torch.no_grad():
                feats = self.model(dummy, cam_label=cam_label)
                if isinstance(feats, tuple):
                    feats = feats[0]
            self._embedding_dim = feats.shape[-1]
        return self._embedding_dim

    def extract(self, paths: list[Path]) -> np.ndarray:
        imgs = [Image.open(p).convert("RGB") for p in paths]
        batch = torch.stack([self.transform(img) for img in imgs]).to(self.device)
        cam_label = torch.zeros(batch.shape[0], dtype=torch.long, device=self.device)
        with torch.no_grad():
            feats = self.model(batch, cam_label=cam_label)
            if isinstance(feats, tuple):
                feats = feats[0]
        feats = torch.nn.functional.normalize(feats, dim=-1)
        return feats.cpu().to(torch.float32).numpy()

    def _load(self, model_name: str, patch_name: str, local_dir: str):
        if not os.path.exists(local_dir):
            result = subprocess.run(
                ["git", "clone", "https://github.com/damo-cv/TransReID.git", local_dir]
            )
            if result.returncode != 0:
                raise RuntimeError(f"Failed to clone TransReID into {local_dir}.")
            print("TransReID model: Cloned.")

        # Google Drive file ID for the Market-1501 TransReID*(ViT) checkpoint
        # (transformer_120.pth), taken directly from the "model" link in the
        # "TransReID*(ViT)" row / "Market" column of the official README's
        # trained-models table: https://github.com/damo-cv/TransReID#trained-models-and-logs-size-256
        # We only auto-download this one, specific, verified checkpoint —
        # not an arbitrary URL — since that's the only file ID we've
        # actually confirmed matches this exact model/dataset combination.
        _MARKET1501_WEIGHTS_GDRIVE_ID = "11p4RjmpCGGAS-876VEt7OoFrUeHTUlyO"

        weight_path = os.path.join(local_dir, patch_name)
        if not os.path.exists(weight_path):
            if patch_name == "vit_transreid_market.pth":
                import gdown

                print(
                    "TransReID weights not found locally: Downloading "
                    "Market-1501 checkpoint from the original TransReID "
                    "Google Drive release."
                )
                gdown.download(
                    id=_MARKET1501_WEIGHTS_GDRIVE_ID, output=weight_path, quiet=False
                )
                if not os.path.exists(weight_path):
                    raise FileNotFoundError(
                        f"Automatic download of TransReID weights to {weight_path} "
                        "failed. Please download manually from "
                        "https://github.com/damo-cv/TransReID#trained-models-and-logs-size-256 "
                        "(TransReID*(ViT), Market column, 'model' link)."
                    )
            else:
                raise FileNotFoundError(
                    f"TransReID weights not found at {weight_path}, and no "
                    f"known Google Drive source exists for patch_name="
                    f"{patch_name!r}. Please download the matching checkpoint "
                    "manually from "
                    "https://github.com/damo-cv/TransReID#trained-models-and-logs-size-256."
                )

        try:
            cfg, make_model = self._import_transreid_modules(local_dir)

            cfg.merge_from_file(
                os.path.join(local_dir, f"configs/Market/{model_name}.yml")
            )
            cfg.MODEL.PRETRAIN_CHOICE = "self"
            cfg.TEST.WEIGHT = weight_path
            cfg.freeze()

            with contextlib.redirect_stdout(io.StringIO()):
                model = make_model(cfg, num_class=751, camera_num=6, view_num=0)
                _original_torch_load = torch.load

                def _torch_load_with_map_location(*args, **kwargs):
                    kwargs.setdefault("map_location", self.device)
                    return _original_torch_load(*args, **kwargs)

                torch.load = _torch_load_with_map_location
                try:
                    model.load_param(cfg.TEST.WEIGHT)
                finally:
                    torch.load = _original_torch_load
            model.to(self.device).eval()

            transform = transforms.Compose(
                [
                    transforms.Resize(cfg.INPUT.SIZE_TEST),
                    transforms.ToTensor(),
                    transforms.Normalize(
                        mean=cfg.INPUT.PIXEL_MEAN, std=cfg.INPUT.PIXEL_STD
                    ),
                ]
            )
            print("TransReID model: Loaded.")
            return model, transform
        except Exception as local_e:
            raise RuntimeError(
                "Could not load TransReID model."
                "Please verify the config, weights, and repo layout."
            ) from local_e

    @staticmethod
    def _import_transreid_modules(local_dir: str):
        """
        TransReID's own top-level packages ('config', 'model', 'loss', 'solver',
        'processor', 'datasets', 'utils') can collide with this project's own
        packages, or leak cached singleton state (e.g. an already-frozen yacs
        CfgNode) across repeated loads within the same process. Evicting only
        the top-level name isn't enough — submodules like 'config.defaults' stay
        cached and get silently reused on the next import, handing back a stale,
        already-frozen config object. Evict every module whose dotted path starts
        with one of these package names, import fresh, then restore whatever was
        cached before so the rest of the project keeps working.
        """
        if local_dir not in sys.path:
            sys.path.insert(0, local_dir)

        # TransReID's vit_pytorch.py does `from torch._six import
        # container_abcs`, an internal PyTorch compatibility shim removed
        # in modern PyTorch releases (it was always just an alias for
        # collections.abc). Install a stand-in so that old import
        # succeeds regardless of the installed PyTorch version, without
        # needing to patch the third-party cloned repo's files.
        try:
            from torch._six import container_abcs as _  # noqa: F401
        except ImportError:
            _torch_six_shim = types.ModuleType("torch._six")
            _torch_six_shim.container_abcs = collections.abc
            sys.modules["torch._six"] = _torch_six_shim

        top_level_names = (
            "config",
            "model",
            "loss",
            "solver",
            "processor",
            "datasets",
            "utils",
        )

        saved_modules = {
            name: mod
            for name, mod in list(sys.modules.items())
            if name.split(".")[0] in top_level_names
        }
        for name in saved_modules:
            del sys.modules[name]

        try:
            transreid_config = importlib.import_module("config")
            transreid_model = importlib.import_module("model")
            cfg = transreid_config.cfg
            make_model = transreid_model.make_model
        finally:
            for name in list(sys.modules.keys()):
                if name.split(".")[0] in top_level_names and name not in saved_modules:
                    del sys.modules[name]
            sys.modules.update(saved_modules)

        return cfg, make_model
