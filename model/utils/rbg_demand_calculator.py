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
import math

from model.iboran_interfaces.iboran_shm_structure import E3Indication
from model.iboran_interfaces.iboran_shm_protocol import RatType, ProtocolLimits
from model.utils.file_store import FileStore
from model.utils.mapper import Mapper


class RbgDemandCalculator:
    # TS 38.214 Section 5.1.3.2 constants
    _CRC_OVERHEAD_BITS = 24
    _SMALL_TBS_THRESHOLD = 3824
    _SMALL_TBS_MIN_QUANT_STEP = 3
    _SMALL_TBS_QUANT_SHIFT = 6
    _LARGE_TBS_MIN_N_INFO = 3840
    _LARGE_TBS_QUANT_SHIFT = 5
    _LOW_CODE_RATE_THRESHOLD = 0.25
    _LOW_RATE_SEGMENT_SIZE = 3816
    _HIGH_RATE_LARGE_PAYLOAD_THRESHOLD = 8424
    _SYMBOLS_PER_SLOT = 14  # NR slot length in OFDM symbols, normal CP
    _MAX_RE_PER_PRB = 156

    NR_MOD_ORDER = (
        64  # matches Mapper.cqi_to_mcs's NR column (TS 38.214 Table 5.1.3.1-1)
    )

    _tbs_table_cache: dict | None = None

    def __init__(
        self,
        rat_type,
        n_layers=1,
        dl_ctrl_symbols=2,
        n_dmrs=12,
        n_oh=0,
        mod_order=NR_MOD_ORDER,
    ):
        self.rat_type = rat_type
        self.n_layers = n_layers
        self.n_dmrs = n_dmrs
        self.n_oh = n_oh
        self.mod_order = mod_order

        self.n_symb = self._SYMBOLS_PER_SLOT - dl_ctrl_symbols

        if RbgDemandCalculator._tbs_table_cache is None:
            # LTE TBS table loaded from tbs.json
            # Source: TS 36.213 v15.5 Table 7.1.7.2.1-1
            # Provided at https://gist.github.com/pgorczak/a10c0cc260ab949dc20226476ec6fd1b
            RbgDemandCalculator._tbs_table_cache = FileStore(
                None, "", "tbs.json"
            ).json_load()
        self.tbs_index_to_tbs_list = RbgDemandCalculator._tbs_table_cache

        self.mapper = Mapper()

    def compute(
        self, e3Indication: E3Indication, selected_idx_k, assigned_mcs_k
    ) -> np.ndarray:
        rbg_demand_k = np.zeros(len(selected_idx_k), dtype=int)

        for i, idx in enumerate(selected_idx_k):
            pending_bits = e3Indication.ue[idx].bufferLevel * 8 / self.n_layers

            if self.rat_type == RatType.LTE:
                prb_demand = self._compute_prb_lte(pending_bits, assigned_mcs_k[i])
                prb_demand = min(prb_demand, ProtocolLimits.MAX_PRB_NUM_LTE)
            elif self.rat_type == RatType.NR:
                prb_demand = self._compute_prb_nr(pending_bits, assigned_mcs_k[i])
                prb_demand = min(prb_demand, ProtocolLimits.MAX_PRB_NUM_NR)
            else:
                raise ValueError(
                    f"RbgDemandCalculator: unsupported rat_type '{self.rat_type}'."
                )

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
        n_re_per_prb = self._n_re_per_prb()
        bit_per_symb, code_rate = self.mapper.nr_mcs_to_qm_coderate(
            mod_order=self.mod_order, mcs=mcs
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

    def _n_re_per_prb(self) -> int:
        return min(self._MAX_RE_PER_PRB, 12 * self.n_symb - self.n_dmrs - self.n_oh)

    def _derive_nr_tbs(
        self,
        prb_demand,
        n_re_per_prb,
        code_rate,
        bit_per_symb,
    ) -> int:
        n_info = n_re_per_prb * prb_demand * code_rate * bit_per_symb * self.n_layers

        if n_info <= self._SMALL_TBS_THRESHOLD:  # small TBS path
            quant_step = max(
                self._SMALL_TBS_MIN_QUANT_STEP,
                math.floor(math.log2(n_info)) - self._SMALL_TBS_QUANT_SHIFT,
            )
            n_info_prime = max(
                24, (2**quant_step) * math.floor(n_info / (2**quant_step))
            )
            return self.mapper.nr_quantize_small_tbs(n_info_prime)

        else:  # large TBS path
            quant_step = (
                math.floor(math.log2(n_info - self._CRC_OVERHEAD_BITS))
                - self._LARGE_TBS_QUANT_SHIFT
            )
            n_info_prime = max(
                self._LARGE_TBS_MIN_N_INFO,
                (2**quant_step)
                * round((n_info - self._CRC_OVERHEAD_BITS) / (2**quant_step)),
            )

            if code_rate <= self._LOW_CODE_RATE_THRESHOLD:
                # Low code rate: small segment size (3816 bits)
                n_segment = math.ceil(
                    (n_info_prime + self._CRC_OVERHEAD_BITS)
                    / self._LOW_RATE_SEGMENT_SIZE
                )
                tbs = (
                    8
                    * n_segment
                    * math.ceil(
                        (n_info_prime + self._CRC_OVERHEAD_BITS) / (8 * n_segment)
                    )
                    - self._CRC_OVERHEAD_BITS
                )
            else:
                if n_info_prime > self._HIGH_RATE_LARGE_PAYLOAD_THRESHOLD:
                    # High code rate, large payload: 8424-bit segments
                    n_segment = math.ceil(
                        (n_info_prime + self._CRC_OVERHEAD_BITS)
                        / self._HIGH_RATE_LARGE_PAYLOAD_THRESHOLD
                    )
                    tbs = (
                        8
                        * n_segment
                        * math.ceil(
                            (n_info_prime + self._CRC_OVERHEAD_BITS) / (8 * n_segment)
                        )
                        - self._CRC_OVERHEAD_BITS
                    )
                else:
                    # High code rate, moderate payload: single segment
                    tbs = (
                        8 * math.ceil((n_info_prime + self._CRC_OVERHEAD_BITS) / 8)
                        - self._CRC_OVERHEAD_BITS
                    )

            return tbs
