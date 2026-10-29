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

from model.iboran_interfaces.iboran_shm_protocol import LinkDirection, RatType
from model.utils.config_store import load_config, RUN_CONFIG_PATH


class RewardTerms:
    def __init__(self, rat_type):
        cfg = load_config(RUN_CONFIG_PATH)
        common = cfg.common
        self.target_mbps = {
            LinkDirection.DOWNLINK: common.dlBitRate / 1e6,
            LinkDirection.UPLINK: common.ulBitRate / 1e6,
        }
        self.delay_budget_ms = 140.0 if rat_type == RatType.LTE else 120.0
        self.seed = common.rngRun
        self.ue_num = common.ueNum
        self.learning_mode = cfg.orchestrator.learning_mode

    @property
    def frozen(self) -> bool:
        "eval: policies are fixed, no parameter update, no action sampling"
        return self.learning_mode == "eval"

    def throughput(self, actual_mbps, link):
        return np.clip(actual_mbps / self.target_mbps[link], 0.0, 1.0)

    def delay(self, delay_ms):
        return np.clip(1.0 - delay_ms / self.delay_budget_ms, -1.0, 1.0)
