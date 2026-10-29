# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from model.utils.algo_resolver import resolve_ib_algo
from model.iboran_interfaces.iboran_shm_structure import (
    Py2Cpp,
    E2KpmReport,
)
from model.iboran_interfaces.iboran_e2sm_dapp_structure import (
    E2DAppReport,
    UeE2DAppControl,
)
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    MessageType,
    ProtocolLimits,
)
from model.iboran_controller.iboran_xapp import IbOranXApp
from model.iboran_controller.iboran_xapp_algos.xapp_algos_factory import (
    setup_xapp_algo_rbg_bounding,
    BASE_XAPP_ALGOS_KEYS,
)

_UNBOUNDED = max(ProtocolLimits.MAX_PRB_NUM_LTE, ProtocolLimits.MAX_PRB_NUM_NR)


class IbOranXAppMacScheduler(IbOranXApp):
    def __init__(self, intents, rat_type, control_period_f, algo_rbg_bounding):
        super().__init__(intents, rat_type, control_period_f, app_id="MacScheduler")
        self.algo_rbg_bounding = algo_rbg_bounding

        self._reset_variables()

    def decide(self, e2KpmReport: E2KpmReport) -> bool:
        link = e2KpmReport.linkDirection

        if e2KpmReport.numUe == 0 or e2KpmReport.numRbg == 0:
            return False  # -> _reset_primary_control_to_skip -> _open_link

        self.candidate_idx, self.candidate_rnti, self.bounded_rbg = (
            self.algo_rbg_bounding.execute(
                e2KpmReport=e2KpmReport, e2DAppReport=self._e2_dapp_report[link]
            )
        )
        num_candidate = len(self.candidate_idx)
        if num_candidate == 0:
            self._open_link(link)
            return True

        bounds = {int(r): int(c) for r, c in zip(self.candidate_rnti, self.bounded_rbg)}
        # a backlogged UE the algorithm left out (e.g. intent pre-selection) is
        # blocked for this decision, as in the original design
        for i in range(e2KpmReport.numUe):
            ue = e2KpmReport.ue[i]
            if ue.bufferLevel > 0 and int(ue.rnti) not in bounds:
                bounds[int(ue.rnti)] = 0

        # a UE bounded by an earlier decision and not covered by this one is released
        released = sorted(self._bounded_rntis[link] - set(bounds))
        entries = (list(bounds.items()) + [(r, _UNBOUNDED) for r in released])[
            : ProtocolLimits.MAX_UE_NUM
        ]
        self._bounded_rntis[link] = {
            r for r, c in bounds.items() if c < e2KpmReport.numRbg
        }
        self._decision_id[link] += 1

        rc = self._e2_rc_control[link]
        rc.skipThisStep = False
        rc.maxUeSchedSize = len(entries)  # C++ loops over exactly these entries
        for i, (rnti, bound) in enumerate(entries):
            rc.ue[i].rnti = rnti
            rc.ue[i].maxRbgAllocation = bound

        dc = self._e2_dapp_control[link]
        dc.skipThisStep = False
        dc.maxUeSchedSize = num_candidate
        dc.numAvailableRbg = int(sum(bounds.values()))
        dc.decisionId = self._decision_id[link]
        dc.ue = [UeE2DAppControl(rnti=r, maxRbgAllocation=c) for r, c in entries]

        return True

    def _open_link(self, link: LinkDirection) -> None:
        "No new bounds: release every outstanding bound and leave both schedulers free."
        released = sorted(self._bounded_rntis[link])[: ProtocolLimits.MAX_UE_NUM]
        self._bounded_rntis[link] = set()
        self._decision_id[link] += 1

        rc = self._e2_rc_control[link]
        rc.skipThisStep = False
        rc.maxUeSchedSize = ProtocolLimits.MAX_UE_NUM
        for i in range(ProtocolLimits.MAX_UE_NUM):
            if i < len(released):
                rc.ue[i].rnti = released[i]
                rc.ue[i].maxRbgAllocation = _UNBOUNDED
            else:
                rc.ue[i].rnti = 0  # RNTI 0 is never assigned: entry is inert
                rc.ue[i].maxRbgAllocation = 0

        dc = self._e2_dapp_control[link]
        dc.skipThisStep = False
        dc.maxUeSchedSize = ProtocolLimits.MAX_UE_NUM
        dc.numAvailableRbg = _UNBOUNDED
        dc.decisionId = self._decision_id[link]
        dc.ue = [UeE2DAppControl(rnti=r, maxRbgAllocation=_UNBOUNDED) for r in released]

    def _reset_primary_control_to_skip(self, link_dir: LinkDirection) -> None:
        # Overrides IbOranXApp: a skipped xApp step must not mute the link.
        self._open_link(link_dir)

    def write_e2_rc_control(self, py2Cpp: Py2Cpp, e2KpmReport: E2KpmReport) -> None:
        rc = self._e2_rc_control[e2KpmReport.linkDirection]
        py2Cpp.e2RcControl.type = MessageType.E2SM_RC_CONTROL
        py2Cpp.e2RcControl.maxUeSchedSize = rc.maxUeSchedSize
        py2Cpp.e2RcControl.skipThisStep = rc.skipThisStep
        py2Cpp.e2RcControl.id = self.stats.steps[e2KpmReport.linkDirection]
        py2Cpp.e2RcControl.linkDirection = e2KpmReport.linkDirection
        for i in range(py2Cpp.e2RcControl.maxUeSchedSize):
            py2Cpp.e2RcControl.ue[i].rnti = rc.ue[i].rnti
            py2Cpp.e2RcControl.ue[i].maxRbgAllocation = rc.ue[i].maxRbgAllocation

    def write_e2_dapp_control(self, link_dir: LinkDirection) -> None:
        dc = self._e2_dapp_control[link_dir]
        self.e2_dapp_.control.type = MessageType.E2SM_DAPP_CONTROL
        self.e2_dapp_.control.maxUeSchedSize = dc.maxUeSchedSize
        self.e2_dapp_.control.numAvailableRbg = dc.numAvailableRbg
        self.e2_dapp_.control.skipThisStep = dc.skipThisStep
        self.e2_dapp_.control.linkDirection = link_dir
        self.e2_dapp_.control.id = self.stats.steps[link_dir]
        self.e2_dapp_.control.decisionId = dc.decisionId
        self.e2_dapp_.control.ue = list(dc.ue)

    def store_e2_dapp_report(
        self, link_dir: LinkDirection, e2_dapp_report: E2DAppReport
    ) -> None:
        self._e2_dapp_report[link_dir].ueSchedSize = e2_dapp_report.ueSchedSize
        self._e2_dapp_report[link_dir].totalNumAllocatedRbg = (
            e2_dapp_report.totalNumAllocatedRbg
        )
        self._e2_dapp_report[link_dir].rbgSize = e2_dapp_report.rbgSize
        self._e2_dapp_report[link_dir].linkDirection = e2_dapp_report.linkDirection
        self._e2_dapp_report[link_dir].id = e2_dapp_report.id
        self._e2_dapp_report[link_dir].ue = list(e2_dapp_report.ue)

    def _reset_variables(self) -> None:
        super()._reset_variables()

        resolved_algo_rbg_bounding, is_ib = resolve_ib_algo(
            name=self.algo_rbg_bounding,
            base_algos=BASE_XAPP_ALGOS_KEYS,
            app_id=self.app_id,
        )
        if not is_ib:
            self.intents = None
        self.algo_rbg_bounding = setup_xapp_algo_rbg_bounding(
            resolved_algo_rbg_bounding, self.intents, self.rat_type
        )

        self._bounded_rntis = self._per_link(set)  # RNTIs holding a finite bound
        self._decision_id = self._per_link(int)

        # start open: no scheduling blackout before the first xApp decision
        for link in (LinkDirection.DOWNLINK, LinkDirection.UPLINK):
            self._open_link(link)
        self.write_e2_dapp_control(LinkDirection.DOWNLINK)
