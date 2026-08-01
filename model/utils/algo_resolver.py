# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

_INTENT_PREFIX = "ib"


def resolve_ib_algo(name: str, base_algos: set[str], app_id: str) -> tuple[str, bool]:
    if name.startswith(_INTENT_PREFIX):
        base_name = name.removeprefix(_INTENT_PREFIX)
        if base_name in base_algos:
            return base_name, True
        raise ValueError(f"'{base_name}' is not a valid {app_id} algorithm.")
    return name, False
