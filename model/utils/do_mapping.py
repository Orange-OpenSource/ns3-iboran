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
        cqi_to_mcs_dict = {
            0: {LTE: 0},
            1: {LTE: 0},
            2: {LTE: 2},
            3: {LTE: 4},
            4: {LTE: 6},
            5: {LTE: 8},
            6: {LTE: 9},
            7: {LTE: 12},
            8: {LTE: 14},
            9: {LTE: 16},
            10: {LTE: 18},
            11: {LTE: 20},
            12: {LTE: 22},
            13: {LTE: 24},
            14: {LTE: 26},
            15: {LTE: 28},
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
