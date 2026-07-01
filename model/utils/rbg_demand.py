# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from model.iboran_interfaces.iboran_shm_structure import *
from model.utils.do_log import DoLog
from model.utils.do_mapping import DoMapping
import numpy as np
import math

import time


class RbgDemand:
    def __init__(
        self,
        rat_type,
        n_layers=1,
        dl_ctrl_symbols=2,
        n_dmrs=12,
        n_oh=0,
    ):
        """
        Parameters
        ----------
        rat_type        : LTE or NR (from iboran_shm_structure)
        n_layers        : number of MIMO layers (1 for SISO, >1 for spatial mux)
        dl_ctrl_symbols : PDCCH symbols reserved per slot (default 2 in 5G-LENA)
                          → effective data symbols = 14 - dl_ctrl_symbols
        n_re_dmrs       : DMRS RE overhead per PRB (default 12 = Type A single symbol)
        n_oh            : additional overhead REs per PRB (0/6/12/18, default 0)
        """
        self.rat_type = rat_type
        self.n_layers = n_layers
        self.n_dmrs = n_dmrs
        self.n_oh = n_oh

        # effective OFDM data symbols per slot (5G-LENA: 14 - m_dlCtrlSymbols)
        self.n_symb = 14 - dl_ctrl_symbols

        # LTE TBS table loaded from tbs.json
        # Source: TS 36.213 v15.5 Table 7.1.7.2.1-1
        # Provided at https://gist.github.com/pgorczak/a10c0cc260ab949dc20226476ec6fd1b
        logger = DoLog(None, "", "tbs.json")
        self.tbs_index_to_tbs_list = logger.json_load()

        self.mapper = DoMapping()

    def compute(
        self, e3Indication: E3Indication, selected_idx_k, assigned_mcs_k
    ) -> np.ndarray:
        rbg_demand_k = np.zeros(len(selected_idx_k), dtype=int)

        for i, idx in enumerate(selected_idx_k):
            pending_bits = e3Indication.ue[idx].bufferLevel * 8 / self.n_layers

            if self.rat_type == LTE:
                prb_demand = self._compute_prb_lte(pending_bits, assigned_mcs_k[i])
                prb_demand = min(prb_demand, MAX_PRB_NUM_LTE)
            else:  # NR
                prb_demand = self._compute_prb_nr(pending_bits, assigned_mcs_k[i])
                prb_demand = min(prb_demand, MAX_PRB_NUM_NR)

            rbg_demand_k[i] = math.ceil(prb_demand / e3Indication.rbgSize)

        return rbg_demand_k

    def _compute_prb_lte(self, pending_bits, mcs) -> int:
        # reverse lookup on TS 36.213 Table 7.1.7.2.1-1
        itbs = self.mapper.mcs_to_tbs_index(mcs)
        prb_demand = max(
            1,
            np.searchsorted(
                self.tbs_index_to_tbs_list[itbs],
                pending_bits,
            ),
        )

        return int(prb_demand)

    def _compute_prb_nr(self, pending_bits, mcs) -> int:
        # inverse TBS formula per TS 38.214 Section 5.1.3
        n_re_per_prb = min(156, 12 * self.n_symb - self.n_dmrs - self.n_oh)
        bit_per_symb, code_rate = self.mapper.nr_mcs_to_qm_coderate(
            mod_order=64, mcs=mcs
        )
        prb_demand = max(
            1,
            math.ceil(
                pending_bits / (n_re_per_prb * code_rate * bit_per_symb * self.n_layers)
            ),
        )

        while (
            self._derive_nr_tbs(prb_demand, n_re_per_prb, code_rate, bit_per_symb)
            < pending_bits
        ):
            prb_demand += 1

        return int(prb_demand)

    def _derive_nr_tbs(
        self,
        prb_demand,
        n_re_per_prb,
        code_rate,
        bit_per_symb,
    ) -> int:
        n_info = n_re_per_prb * prb_demand * code_rate * bit_per_symb * self.n_layers

        if n_info <= 3824:  # small TBS path
            quant_step = max(3, math.floor(math.log2(n_info)) - 6)
            n_info_prime = max(
                24, (2**quant_step) * math.floor(n_info / (2**quant_step))
            )
            return self.mapper.nr_quantize_small_tbs(n_info_prime)

        else:  # large TBS path
            quant_step = math.floor(math.log2(n_info - 24)) - 5  # 24 bit CRC overhead
            n_info_prime = max(
                3840, (2**quant_step) * round((n_info - 24) / (2**quant_step))
            )

            if code_rate <= 0.25:
                # Low code rate: small segment size (3816 bits)
                n_segment = math.ceil((n_info_prime + 24) / 3816)
                tbs = (
                    8 * n_segment * math.ceil((n_info_prime + 24) / (8 * n_segment))
                    - 24
                )
            else:
                if n_info_prime > 8424:
                    # High code rate, large payload: 8424-bit segments
                    n_segment = math.ceil((n_info_prime + 24) / 8424)
                    tbs = (
                        8 * n_segment * math.ceil((n_info_prime + 24) / (8 * n_segment))
                        - 24
                    )
                else:
                    # High code rate, moderate payload: single segment
                    tbs = 8 * math.ceil((n_info_prime + 24) / 8) - 24

            return tbs
