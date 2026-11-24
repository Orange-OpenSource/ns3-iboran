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
from model.iboran_interfaces.iboran_shm_protocol import RatType


class Mapper:
    def __init__(self):
        pass

    @staticmethod
    def bandwidth_to_rbg_size(bandwidth=10) -> int:
        """
        Standard bandwidth to rbg size mapping.

        LTE uses Type 0 resource allocation, where the downlink bandwidth is divided
        into RBGs of varying sizes depending on the total number of RBs available.
        This function returns the appropriate RBG size index for the given bandwidth.
        The mapping is as follows (see 3GPP TS 36.213, Table 7.1.6.1-1):
        - If 63 < dlBandwidth <= 110 -> returns 4
        - If 26 < dlBandwidth <= 63  -> returns 3
        - If 10 < dlBandwidth <= 26  -> returns 2
        - If dlBandwidth <= 10  -> returns 1
        """
        type0_allocation_rbg_dict = {
            1: 10,  # For smallest bandwidths (RBG size 1)
            2: 26,  # For medium bandwidths (RBG size 2)
            3: 63,  # For large bandwidths (RBG size 3)
            4: 110,  # For very large bandwidths (RBG size 4)
        }
        for rbg_size, upper_bound in type0_allocation_rbg_dict.items():
            if bandwidth <= upper_bound:
                return rbg_size
        raise ValueError("Bandwidth value is not correctly initialized.")

    @staticmethod
    def cqi_to_mcs(rat_type=RatType.LTE, cqi=0) -> int:
        "Standard CQI to MCS mapping."

        if cqi < 0 or cqi > 15:
            raise ValueError("CQI value is out of range (0-15).")

        # LTE: TS 36.213 Table 7.2.3-1 matched to Table 7.1.7.1-1
        # NR: TS 38.214 Table 5.2.2.1-2 matched to Table 5.1.3.1-1
        cqi_to_mcs_dict = {
            0: {RatType.LTE: 0, RatType.NR: 0},
            1: {RatType.LTE: 0, RatType.NR: 0},
            2: {RatType.LTE: 2, RatType.NR: 0},
            3: {RatType.LTE: 4, RatType.NR: 2},
            4: {RatType.LTE: 6, RatType.NR: 4},
            5: {RatType.LTE: 8, RatType.NR: 6},
            6: {RatType.LTE: 9, RatType.NR: 8},
            7: {RatType.LTE: 12, RatType.NR: 11},
            8: {RatType.LTE: 14, RatType.NR: 13},
            9: {RatType.LTE: 16, RatType.NR: 15},
            10: {RatType.LTE: 18, RatType.NR: 18},
            11: {RatType.LTE: 20, RatType.NR: 20},
            12: {RatType.LTE: 22, RatType.NR: 22},
            13: {RatType.LTE: 24, RatType.NR: 24},
            14: {RatType.LTE: 26, RatType.NR: 26},
            15: {RatType.LTE: 28, RatType.NR: 28},
        }
        return cqi_to_mcs_dict[cqi][rat_type]

    @staticmethod
    def mcs_to_tbs_index(mcs=0) -> int:
        "Standard MCS to TBS index mapping."
        # ref: 3GPP TS 36.213, v15.5, Table 7.1.7.1-1
        if mcs < 0 or mcs > 28:
            raise ValueError("MCS value is out of range (0-28).")

        # fmt: off
        mcs_to_tbs_index_dict = {
            0: 0, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 6, 7: 7,
            8: 8, 9: 9, 10: 9, 11: 10, 12: 11, 13: 12, 14: 13, 15: 14,
            16: 15, 17: 15, 18: 16, 19: 17, 20: 18, 21: 19, 22: 20, 23: 21,
            24: 22, 25: 23, 26: 24, 27: 25, 28: 26,
        }
        # fmt: on
        return mcs_to_tbs_index_dict[mcs]

    @staticmethod
    def nr_mcs_to_qm_coderate(mod_order=64, mcs=0) -> tuple[int, float]:
        "Standard NR MCS to modulation order (Qm) and code rate mapping."
        # NR MCS Table 1 — TS 38.214 Table 5.1.3.1-1 (64-QAM max)
        # Default table used by 5G-LENA
        # Format: mcs_index -> (Qm, code_rate)

        # fmt: off
        mcs_to_qm_coderate_dict_64QAM = {
            # QPSK
            0: (2, 0.1172), 1: (2, 0.1533), 2: (2, 0.1885), 3: (2, 0.2451),
            4: (2, 0.3008), 5: (2, 0.3701), 6: (2, 0.4385), 7: (2, 0.5137),
            8: (2, 0.5879), 9: (2, 0.6631),
            # 16QAM
            10: (4, 0.3320), 11: (4, 0.3691), 12: (4, 0.4238), 13: (4, 0.4785),
            14: (4, 0.5400), 15: (4, 0.6016), 16: (4, 0.6426),
            # 64QAM
            17: (6, 0.4277), 18: (6, 0.4551), 19: (6, 0.5049), 20: (6, 0.5537),
            21: (6, 0.6016), 22: (6, 0.6504), 23: (6, 0.7021), 24: (6, 0.7539),
            25: (6, 0.8027), 26: (6, 0.8525), 27: (6, 0.8887), 28: (6, 0.9258),
        }
        # fmt: on

        # fmt: off
        # TS 38.214 Table 5.1.3.1-2: (Qm, R), R = target code rate x 1024 / 1024.
        # MCS 28 is reserved in the table and repeats MCS 27.
        mcs_to_qm_coderate_dict_256QAM = {
            # QPSK
            0: (2, 0.1172), 1: (2, 0.1885), 2: (2, 0.3008), 3: (2, 0.4385),
            4: (2, 0.5879),
            # 16QAM
            5: (4, 0.3691), 6: (4, 0.4238), 7: (4, 0.4785), 8: (4, 0.5400),
            9: (4, 0.6016), 10: (4, 0.6426),
            # 64QAM
            11: (6, 0.4551), 12: (6, 0.5049), 13: (6, 0.5537), 14: (6, 0.6016),
            15: (6, 0.6504), 16: (6, 0.7021), 17: (6, 0.7539), 18: (6, 0.8027),
            19: (6, 0.8525),
            # 256QAM
            20: (8, 0.6665), 21: (8, 0.6943), 22: (8, 0.7363), 23: (8, 0.7783),
            24: (8, 0.8213), 25: (8, 0.8643), 26: (8, 0.8950), 27: (8, 0.9258),
            28: (8, 0.9258),
        }
        # fmt: on

        if mod_order == 64:
            return mcs_to_qm_coderate_dict_64QAM[mcs]
        elif mod_order == 256:
            return mcs_to_qm_coderate_dict_256QAM[mcs]
        else:
            raise ValueError(
                "Unsupported modulation order. Only 64 and 256 QAM are supported."
            )

    @staticmethod
    def nr_quantize_small_tbs(n_info_prime) -> int:
        "Quantize small TBS values using TS 38.214 Table 5.1.3.2-1."
        # fmt: off
        # TS 38.214 Table 5.1.3.2-1: TBS for N_info <= 3824 (93 entries)
        nr_small_tbs_table = [
            24, 32, 40, 48, 56, 64, 72, 80, 88, 96,
            104, 112, 120, 128, 136, 144, 152, 160, 168, 176,
            184, 192, 208, 224, 240, 256, 272, 288, 304, 320,
            336, 352, 368, 384, 408, 432, 456, 480, 504, 528,
            552, 576, 608, 640, 672, 704, 736, 768, 808, 848,
            888, 928, 984, 1032, 1064, 1128, 1160, 1192, 1224, 1256,
            1288, 1320, 1352, 1416, 1480, 1544, 1608, 1672, 1736, 1800,
            1864, 1928, 2024, 2088, 2152, 2216, 2280, 2408, 2472, 2536,
            2600, 2664, 2728, 2792, 2856, 2976, 3104, 3240, 3368, 3496,
            3624, 3752, 3824,
        ]
        # fmt: on

        idx = np.searchsorted(nr_small_tbs_table, n_info_prime)
        idx = min(idx, len(nr_small_tbs_table) - 1)
        return nr_small_tbs_table[idx]

    @staticmethod
    def qos_priority_level(qos_id: int) -> float:
        "Priority level of a QCI or 5QI; lower is served first, unknown IDs rank last."
        # fmt: off
        # TS 23.501 Table 5.7.4-1 default priority levels, divided by 10. For IDs
        # 1-9 they equal the QCI levels of TS 23.203 Table 6.1.7-A, so LTE uses
        # the same table.
        _QOS_PRIORITY_LEVEL = {
            # GBR
            1: 2.0, 2: 4.0, 3: 3.0, 4: 5.0, 65: 0.7, 66: 2.0, 67: 1.5,
            71: 5.6, 72: 5.6, 73: 5.6, 74: 5.6, 75: 2.5, 76: 5.6,
            # non-GBR
            5: 1.0, 6: 6.0, 7: 7.0, 8: 8.0, 9: 9.0, 10: 9.0,
            69: 0.5, 70: 5.5, 79: 6.5, 80: 6.8,
            # delay-critical GBR
            82: 1.9, 83: 2.2, 84: 2.4, 85: 2.1, 86: 1.8,
            87: 2.5, 88: 2.5, 89: 2.5, 90: 2.5,
        }
        # fmt: on
        return _QOS_PRIORITY_LEVEL.get(int(qos_id), 9.0)
