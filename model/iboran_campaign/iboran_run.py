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
import os
import argparse
from model.iboran_campaign.iboran_orchestrator import IbOranOrchestrator
from model.utils.do_log import DoLog


def main():
    args = config_args()

    orchestrator = IbOranOrchestrator(
        rat_type=args.rat_type,
        dapp_control_period_sf=args.dapp_control_period_sf,
        xapp_control_period_f=args.xapp_control_period_f,
        dapp_algo_ue_selection=args.dapp_algo_ue_selection,
        xapp_algo_rbg_bounding=args.xapp_algo_rbg_bounding,
        show_flowstats=args.show_flowstats,
        show_iqa_diagnosis=args.show_iqa_diagnosis,
        save_dapp_log=args.save_dapp_log,
    )
    result = (
        orchestrator.execute(k=args.k) if args.k is not None else orchestrator.execute()
    )

    if result == 0:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        logger = DoLog(script_dir, "../../notebook/logs", "kpi.json")
        logger.json_dump(orchestrator.kpi)
        orchestrator.cleanup()

    sys.exit(result)


def config_args():
    parser = argparse.ArgumentParser(description="ib-ORAN")
    parser.add_argument("--rat_type", default="LTE", help="RAT type")
    parser.add_argument(
        "--dapp_control_period_sf",
        type=int,
        default=1,
        help="dApp control period (scale: subframe)",
    )
    parser.add_argument(
        "--xapp_control_period_f",
        type=int,
        default=1,
        help="xApp control period (scale: frame)",
    )
    parser.add_argument("--k", type=int, default=1, help="UE selection size")
    parser.add_argument(
        "--dapp_algo_ue_selection", default="Random", help="dApp UE selection algorithm"
    )
    parser.add_argument(
        "--xapp_algo_rbg_bounding", default="Bypass", help="xApp RBG bounding algorithm"
    )
    parser.add_argument(
        "--show_flowstats", type=lambda x: x.lower() == "true", default=True
    )
    parser.add_argument(
        "--show_iqa_diagnosis", type=lambda x: x.lower() == "true", default=True
    )
    parser.add_argument(
        "--save_dapp_log", type=lambda x: x.lower() == "true", default=False
    )
    return parser.parse_args()


if __name__ == "__main__":
    main()
