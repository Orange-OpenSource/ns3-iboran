# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

import sys
import argparse

from model.iboran_interfaces.iboran_shm_protocol import RatType
from model.iboran_campaign.iboran_orchestrator import IbOranOrchestrator
from model.utils.file_store import FileStore
from model.utils.config_store import (
    ConfigStore,
    load_config,
    DEFAULT_CONFIG_PATH,
    RUN_CONFIG_PATH,
)


def main():
    args = _config_args()
    overrides = _collect_cli_overrides(args)
    master = load_config(DEFAULT_CONFIG_PATH)

    cfg = (
        ConfigStore(**_deep_merge(master.model_dump(), overrides))
        if overrides
        else master
    )
    cfg.dump_run_config(RUN_CONFIG_PATH)

    orchestrator = IbOranOrchestrator(
        rat_type=getattr(RatType, cfg.rat_type),
        dapp_control_period_sf=cfg.orchestrator.dapp_control_period_sf,
        xapp_control_period_f=cfg.orchestrator.xapp_control_period_f,
        dapp_algo_ue_selection=cfg.orchestrator.dapp_algo_ue_selection,
        xapp_algo_rbg_bounding=cfg.orchestrator.xapp_algo_rbg_bounding,
        learning_mode=cfg.orchestrator.learning_mode,
        dataset_id=cfg.orchestrator.dataset_id,
        data_format=cfg.orchestrator.data_format,
        embedder=cfg.orchestrator.embedder,
        oid_range=cfg.orchestrator.oid_range,
        save_dapp_log=cfg.orchestrator.save_dapp_log,
        save_learning_convg=cfg.orchestrator.save_learning_convg,
        verbose=cfg.orchestrator.verbose,
        common_params=cfg.common,
        datasets={name: entry.model_dump() for name, entry in cfg.datasets.items()},
    )
    result = orchestrator.execute(k=cfg.orchestrator.k)

    if result == 0:
        store = FileStore(orchestrator.notebook_dir, "logs", "kpi.json")
        store.json_dump(orchestrator.kpi)
        orchestrator.cleanup()

    sys.exit(result)


def _deep_merge(base: dict, overrides: dict) -> dict:
    result = dict(base)
    for key, value in overrides.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = _deep_merge(result[key], value)
        else:
            result[key] = value
    return result


def _collect_cli_overrides(args) -> dict:
    _TOP_LEVEL_FIELDS = {"rat_type"}

    all_fields = [
        "rat_type",
        "k",
        "dapp_control_period_sf",
        "xapp_control_period_f",
        "dapp_algo_ue_selection",
        "xapp_algo_rbg_bounding",
        "learning_mode",
        "dataset_id",
        "data_format",
        "embedder",
        "oid_range",
        "save_dapp_log",
        "save_learning_convg",
        "verbose",
    ]

    overrides: dict = {}
    orchestrator_overrides: dict = {}
    for field in all_fields:
        value = getattr(args, field)
        if value is None:
            continue
        if field in _TOP_LEVEL_FIELDS:
            overrides[field] = value
        else:
            orchestrator_overrides[field] = value

    if orchestrator_overrides:
        overrides["orchestrator"] = orchestrator_overrides
    # replication index lives in `common`: ns-3 reads it from runConfig.yaml
    if args.rng_run is not None:
        overrides.setdefault("common", {})["rngRun"] = args.rng_run
    return overrides


def _config_args():
    parser = argparse.ArgumentParser(description="ib-ORAN")
    parser.add_argument("--rat_type", default=None, help="RAT")
    parser.add_argument(
        "--rng_run",
        type=int,
        default=None,
        help="Replication index: ns-3 RngRun and seed of all other RNGs",
    )
    parser.add_argument(
        "--dapp_control_period_sf",
        type=int,
        default=None,
        help="dApp control period (in subframe)",
    )
    parser.add_argument(
        "--xapp_control_period_f",
        type=int,
        default=None,
        help="xApp control period (in frame)",
    )
    parser.add_argument(
        "--k",
        type=int,
        default=None,
        help="Maximum parallel UEs scheduled per TTI/slot",
    )
    parser.add_argument(
        "--dapp_algo_ue_selection",
        default=None,
        help="dApp MAC scheduling algorithm",
    )
    parser.add_argument(
        "--xapp_algo_rbg_bounding", default=None, help="xApp RBG bounding algorithm"
    )
    parser.add_argument(
        "--learning_mode",
        type=str,
        default=None,
        choices=["online", "train", "eval"],
        help="online: learn within the run | train: load, learn, save checkpoint "
        "| eval: load checkpoint, frozen policies",
    )
    parser.add_argument("--dataset_id", default=None, help="Identifier of the dataset")
    parser.add_argument("--data_format", default=None, help="Application data format")
    parser.add_argument("--embedder", default=None, help="Emedding extraction model")
    parser.add_argument(
        "--oid_range",
        type=lambda s: tuple(int(x) for x in s.split(",")),
        default=None,
        help="Range of object IDs for intent generation (format: low,high)",
    )
    parser.add_argument(
        "--save_dapp_log",
        type=_str2bool,
        default=None,
        help="Save dApp control log to file",
    )
    parser.add_argument(
        "--save_learning_convg",
        type=_str2bool,
        default=None,
        help="Save learning convergence histories (train mode) to a file",
    )
    parser.add_argument(
        "--verbose",
        type=_str2bool,
        default=None,
        help="Print statistics and KPIs' summary to console",
    )
    return parser.parse_args()


def _str2bool(value) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        if value.lower() in ("true", "1", "yes"):
            return True
        if value.lower() in ("false", "0", "no"):
            return False
    raise argparse.ArgumentTypeError(f"Boolean value expected, got: {value!r}")


if __name__ == "__main__":
    main()
