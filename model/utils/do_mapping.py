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
import numpy as np


class DoMapping:
    def __init__(self):
        pass

    @staticmethod
    def bandwidth_to_rbg_size(bandwidth=10):
        "Standard bandwidth to rbg size mapping."
        """
        LTE uses Type 0 resource allocation, where the downlink bandwidth is divided
        into RBGs of varying sizes depending on the total number of RBs available. 
        This function returns the appropriate RBG size index for the given bandwidth.
        The mapping is as follows (see 3GPP TS 36.213, Table 7.1.6.1-1):
        - If 63 < dlBandwidth <= 110 -> returns 4
        - If 26 < dlBandwidth <= 63  -> returns 3
        - If 10 < dlBandwidth <= 26  -> returns 2
        - If dlBandwidth <= 10  -> returns 1
        - Otherwise, returns -1 indicating an invalid bandwidth.
        """
        type0_allocation_rbg_dict = {
            1: 10,  # For smallest bandwidths (RBG size 1)
            2: 24,  # For medium bandwidths (RBG size 2)
            3: 63,  # For large bandwidths (RBG size 3)
            4: 110,  # For very large bandwidths (RBG size 4)
        }
        for rbg_size, upper_bound in type0_allocation_rbg_dict.items():
            if bandwidth <= upper_bound:
                return rbg_size
        raise ValueError("Bandwidth value is not correctly initialized.")

    @staticmethod
    def cqi_to_mcs(rat_type=LTE, cqi=0):
        "Standard CQI to MCS mapping."

        if cqi < 0 or cqi > 15:
            raise ValueError("CQI value is out of range (0-15).")

        # LTE: TS 36.213 Table 7.2.3-1 matched to Table 7.1.7.1-1
        # NR: TS 38.214 Table 5.2.2.1-2 matched to Table 5.1.3.1-1
        cqi_to_mcs_dict = {
            0: {LTE: 0, NR: 0},
            1: {LTE: 0, NR: 0},
            2: {LTE: 2, NR: 0},
            3: {LTE: 4, NR: 2},
            4: {LTE: 6, NR: 4},
            5: {LTE: 8, NR: 6},
            6: {LTE: 9, NR: 8},
            7: {LTE: 12, NR: 11},
            8: {LTE: 14, NR: 13},
            9: {LTE: 16, NR: 15},
            10: {LTE: 18, NR: 18},
            11: {LTE: 20, NR: 20},
            12: {LTE: 22, NR: 22},
            13: {LTE: 24, NR: 24},
            14: {LTE: 26, NR: 26},
            15: {LTE: 28, NR: 28},
        }
        return cqi_to_mcs_dict[cqi][rat_type]

    @staticmethod
    def mcs_to_tbs_index(mcs=0):
        "Standard MCS to TBS index mapping."
        # ref: 3GPP TS 36.213, v15.5, Table 7.1.7.1-1
        if mcs < 0 or mcs > 28:
            raise ValueError("MCS value is out of range (0-28).")

        mcs_to_tbs_index_dict = {
            0: 0,
            1: 1,
            2: 2,
            3: 3,
            4: 4,
            5: 5,
            6: 6,
            7: 7,
            8: 8,
            9: 9,
            10: 9,
            11: 10,
            12: 11,
            13: 12,
            14: 13,
            15: 14,
            16: 15,
            17: 15,
            18: 16,
            19: 17,
            20: 18,
            21: 19,
            22: 20,
            23: 21,
            24: 22,
            25: 23,
            26: 24,
            27: 25,
            28: 26,
        }
        return mcs_to_tbs_index_dict[mcs]

    @staticmethod
    def nr_mcs_to_qm_coderate(mod_order=64, mcs=0):
        "Standard NR MCS to modulation order (Qm) and code rate mapping."
        # NR MCS Table 1 — TS 38.214 Table 5.1.3.1-1 (64-QAM max)
        # Default table used by 5G-LENA
        # Format: mcs_index -> (Qm, code_rate)

        mcs_to_qm_coderate_dict_64QAM = {
            0: (2, 0.1172),
            1: (2, 0.1533),
            2: (2, 0.1885),
            3: (2, 0.2451),
            4: (2, 0.3008),
            5: (2, 0.3701),
            6: (2, 0.4385),
            7: (2, 0.5137),
            8: (2, 0.5879),
            9: (2, 0.6631),
            10: (4, 0.3320),
            11: (4, 0.3691),
            12: (4, 0.4238),
            13: (4, 0.4785),
            14: (4, 0.5400),
            15: (4, 0.6016),
            16: (4, 0.6426),
            17: (6, 0.4277),
            18: (6, 0.4551),
            19: (6, 0.5049),
            20: (6, 0.5537),
            21: (6, 0.6016),
            22: (6, 0.6504),
            23: (6, 0.7021),
            24: (6, 0.7539),
            25: (6, 0.8027),
            26: (6, 0.8525),
            27: (6, 0.8887),
            28: (6, 0.9258),
        }

        mcs_to_qm_coderate_dict_256QAM = {
            0: (2, 0.1172),  # 120/1024
            1: (2, 0.1885),  # 193/1024
            2: (2, 0.3008),  # 308/1024
            3: (2, 0.4385),  # 449/1024
            4: (2, 0.5879),  # 602/1024
            5: (4, 0.3691),  # 378/1024
            6: (4, 0.4238),  # 434/1024
            7: (4, 0.4785),  # 490/1024
            8: (4, 0.5400),  # 553/1024
            9: (4, 0.6016),  # 616/1024
            10: (4, 0.6426),  # 658/1024
            11: (6, 0.4551),  # 466/1024
            12: (6, 0.5049),  # 517/1024
            13: (6, 0.5537),  # 567/1024
            14: (6, 0.6016),  # 616/1024
            15: (6, 0.6504),  # 666/1024
            16: (6, 0.7021),  # 719/1024
            17: (6, 0.7539),  # 772/1024
            18: (6, 0.8027),  # 822/1024
            19: (6, 0.8525),  # 873/1024
            20: (8, 0.6665),  # 682.5/1024
            21: (8, 0.6943),  # 711/1024
            22: (8, 0.7363),  # 754/1024
            23: (8, 0.7783),  # 797/1024
            24: (8, 0.8213),  # 841/1024
            25: (8, 0.8643),  # 885/1024
            26: (8, 0.8950),  # 916.5/1024
            27: (8, 0.9258),  # 948/1024
            28: (
                8,
                0.9258,
            ),  # 948/1024 (same as mcs 27, since 256-QAM maxes out at mcs 27)
        }

        if mod_order == 64:
            return mcs_to_qm_coderate_dict_64QAM[mcs]
        elif mod_order == 256:
            return mcs_to_qm_coderate_dict_256QAM[mcs]
        else:
            raise ValueError(
                "Unsupported modulation order. Only 64 and 256 QAM are supported."
            )

    @staticmethod
    def nr_quantize_small_tbs(n_info_prime):
        "Quantize small TBS values using TS 38.214 Table 5.1.3.2-1."
        nr_small_tbs_table = [
            24,
            32,
            40,
            48,
            56,
            64,
            72,
            80,
            88,
            96,
            104,
            112,
            120,
            128,
            136,
            144,
            152,
            160,
            168,
            176,
            184,
            192,
            208,
            224,
            240,
            256,
            272,
            288,
            304,
            320,
            336,
            352,
            368,
            384,
            408,
            432,
            456,
            480,
            504,
            528,
            552,
            576,
            608,
            640,
            672,
            704,
            736,
            768,
            808,
            848,
            888,
            928,
            984,
            1032,
            1064,
            1128,
            1160,
            1192,
            1224,
            1256,
            1288,
            1320,
            1352,
            1416,
            1480,
            1544,
            1608,
            1672,
            1736,
            1800,
            1864,
            1928,
            2024,
            2088,
            2152,
            2216,
            2280,
            2408,
            2472,
            2536,
            2600,
            2664,
            2728,
            2792,
            2856,
            2976,
            3104,
            3240,
            3368,
            3496,
            3624,
            3752,
            3824,
        ]

        idx = np.searchsorted(nr_small_tbs_table, n_info_prime)
        idx = min(idx, len(nr_small_tbs_table) - 1)
        return nr_small_tbs_table[idx]
