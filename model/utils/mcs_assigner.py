# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md


from model.utils.mapper import Mapper


class McsAssigner:
    """
    Maps CQI (0-15) to MCS (0-28/31) using extended 256-QAM table.
    LTE: TS 36.213 Tables 7.2.3-1 and 7.1.7.1-1
    NR:  TS 38.214 Tables 5.2.2.1-2 and 5.1.3.1-1
    Target BLER < 10%.
    """

    def __init__(self, rat_type):
        self.rat_type = rat_type

        self.mapper = Mapper()

    def assign(self, Cpp2PyReport, input_idx_vect) -> list[int]:
        assigned_mcs_k = []
        for idx in input_idx_vect:
            cqi = max(0, min(15, Cpp2PyReport.ue[idx].cqi))
            mcs = self.mapper.cqi_to_mcs(self.rat_type, cqi)
            assigned_mcs_k.append(mcs)

        return assigned_mcs_k
