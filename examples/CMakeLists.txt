# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

# == LTE ============================================================
build_lib_example(
    NAME ns3iboran_lte
    SOURCE_FILES
        ../model/iboran_ran_environment/iboran-lte-environment.cc
    LIBRARIES_TO_LINK
        ${libiboran}
        ${liblte}
)
