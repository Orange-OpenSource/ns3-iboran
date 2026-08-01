# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from __future__ import annotations

from pathlib import Path
from typing import Literal

import yaml
from pydantic import BaseModel, Field

DEFAULT_CONFIG_PATH = (
    Path(__file__).resolve().parents[1] / "iboran_campaign" / "iboranConfig.yaml"
)
RUN_CONFIG_PATH = (
    Path(__file__).resolve().parents[2] / "notebook" / "logs" / "runConfig.yaml"
)


class CommonRanParams(BaseModel):
    simTime: float = 20.0
    simAreaLength: float = 500.0
    ueNum: int = 6
    xnbNum: int = 1
    remoteHostNum: int = 1

    handoverHysteresis: float = 3.0
    handoverTimeToTrigger: float = 256

    ueMinTxPower: float = -40.0
    ueMaxTxPower: float = 23.0
    xnbTxPower: float = 46.0
    compensationFactor: float = Field(0.8, ge=0.0, le=1.0)

    backhaulDataRate: str = "100Gbps"
    backhaulMtu: int = 1500
    backhaulDelay: float = 10.0

    enableDlFlow: bool = True
    dlTxRepeat: int = 1
    dlPacketSize: int = 512
    dlBitRate: int = 200_000
    dlUseUdp: bool = True

    enableUlFlow: bool = True
    ulTxRepeat: int = 1
    ulPacketSize: int = 512
    ulBitRate: int = 100_000
    ulUseUdp: bool = True

    enableIbOranSchedCoreLog: bool = False
    enableRrcLog: bool = False


class LteParams(BaseModel):
    dlEarfcn: int = 2750
    ulEarfcn: int = 20750
    dlRbNum: int = 100
    ulRbNum: int = 100
    transmissionMode: int = Field(2, ge=1, le=7)
    p0NominalPusch: float = -96.0


class NrParams(BaseModel):
    centralFrequency: float = 3.5e9
    bandwidth: float = 100e6
    numCc: int = Field(1, ge=1, le=16)
    numBwp: int = Field(1, ge=1, le=4)
    numerology: int = Field(1, ge=0, le=6)
    tddPattern: str = "DL|DL|DL|DL|F|UL|UL|UL|UL|F|"
    gNbAntennaNumRows: int = 4
    gNbAntennaNumColumns: int = 4
    ueAntennaNumRows: int = 2
    ueAntennaNumColumns: int = 2


class OrchestratorConfig(BaseModel):
    k: int = 1
    dapp_control_period_sf: int = 1
    xapp_control_period_f: int = 1
    dapp_algo_ue_selection: str = "Random"
    xapp_algo_rbg_bounding: str = "Bypass"
    dataset_id: str = "toy"
    data_format: Literal["IMAGE", "VIDEO", "EMBEDDING", "TEXT"] = "IMAGE"
    embedder: Literal["TransReID", "CLIP"] = "TransReID"
    oid_range: tuple[int, int] = (1, 2)
    save_dapp_log: bool = False
    verbose: bool = True


class DatasetConfig(BaseModel):
    repository: str
    input_image_format: str
    input_pattern: str
    output_pattern: str
    input_fields: list[str] = []
    output_fields: list[str] = []


class ConfigStore(BaseModel):
    rat_type: Literal["LTE", "NR"] = "LTE"
    common: CommonRanParams = Field(default_factory=CommonRanParams)
    lte: LteParams = Field(default_factory=LteParams)
    nr: NrParams = Field(default_factory=NrParams)
    orchestrator: OrchestratorConfig = Field(default_factory=OrchestratorConfig)
    datasets: dict[str, DatasetConfig] = Field(default_factory=dict)

    @property
    def active_dataset(self) -> DatasetConfig:
        try:
            return self.datasets[self.orchestrator.dataset_id]
        except KeyError:
            raise KeyError(
                f"dataset_id={self.orchestrator.dataset_id!r} has no entry under "
                f"`datasets` in the config file. Known datasets: "
                f"{sorted(self.datasets)}"
            ) from None

    def dump_run_config(self, path: str | Path) -> None:
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w") as f:
            yaml.safe_dump(self.model_dump(mode="json"), f, sort_keys=False)


def load_config(path: str | Path) -> ConfigStore:
    with open(path) as f:
        raw = yaml.safe_load(f) or {}
    return ConfigStore(**raw)
