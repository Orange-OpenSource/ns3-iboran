# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from model.iboran_interfaces.iboran_shm_protocol import LinkDirection


class IbOranControllerStats:
    def __init__(self):
        self.steps = {LinkDirection.DOWNLINK: 0, LinkDirection.UPLINK: 0}
        self.decisions = {LinkDirection.DOWNLINK: 0, LinkDirection.UPLINK: 0}
        self.decision_time = {LinkDirection.DOWNLINK: 0.0, LinkDirection.UPLINK: 0.0}
        self.errors = []

    def print_stats(self, kind: str, app_id: str) -> None:
        header = f"-- {kind} {app_id}: Stats "
        print(f"\n{header:-<39}")

        tot_decisions = (
            self.decisions[LinkDirection.UPLINK]
            + self.decisions[LinkDirection.DOWNLINK]
        )
        for link in [LinkDirection.UPLINK, LinkDirection.DOWNLINK]:
            print(f"-- {link}")
            print(f"{'Control steps':<22}: {self.steps[link]}")
            if tot_decisions > 0:
                print(
                    f"{'Decisions':<22}: {self.decisions[link]}"
                    f" ({100 * self.decisions[link] / tot_decisions:.2f} %)"
                )
            if self.decisions[link] > 0:
                avg_decision_time = (
                    1000 * self.decision_time[link] / self.decisions[link]
                )
                print(f"{'Avg decision time':<22}: {avg_decision_time:.2f} ms")
            print()

        print(f"{'Errors encountered':<22}: {len(self.errors)}")
        if self.errors:
            print("\n error(s)")
            for err in self.errors:
                print(f" -> {err}")
