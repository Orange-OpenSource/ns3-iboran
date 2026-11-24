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
    E2KpmIndication,
)
from model.iboran_interfaces.iboran_e2sm_dapp_structure import (
    E2DAppIndication,
    UeE2DAppControlRequest,
)
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    MessageType,
    ProtocolLimits,
    RanFunctionId,
    RicRequestId,
)
from model.iboran_controller.iboran_xapp import IbOranXApp
from model.iboran_controller.iboran_xapp_algos.xapp_algos_factory import (
    setup_xapp_algo_rbg_bounding,
    BASE_XAPP_ALGOS_KEYS,
)

_UNBOUNDED = max(ProtocolLimits.MAX_PRB_NUM_LTE, ProtocolLimits.MAX_PRB_NUM_NR)


class IbOranXAppMacScheduler(IbOranXApp):
    def __init__(
        self, intents, rat_type, control_period_f, save_log, algo_rbg_bounding
    ):
        super().__init__(
            intents, rat_type, control_period_f, save_log, app_id="MacScheduler"
        )
        self.algo_rbg_bounding = algo_rbg_bounding

        self._reset_variables()

    def decide(self, e2KpmIndication: E2KpmIndication) -> bool:
        link = e2KpmIndication.linkDirection

        if e2KpmIndication.numUe == 0 or e2KpmIndication.numRbg == 0:
            return False  # -> _reset_primary_control_to_skip -> _open_link

        # E2SM-RC addresses UEs by UE ID; a UE not identified yet is left unbounded
        ue_id_of = {}
        for i in range(e2KpmIndication.numUe):
            ue = e2KpmIndication.ue[i]
            if ue.ueId != 0:
                ue_id_of[int(ue.rnti)] = int(ue.ueId)
        self._rnti_of[link].update({u: r for r, u in ue_id_of.items()})

        self.candidate_idx, self.candidate_rnti, self.bounded_rbg = (
            self.algo_rbg_bounding.execute(
                e2KpmIndication=e2KpmIndication,
                e2DAppIndication=self._e2_dapp_indication[link],
            )
        )
        num_candidate = len(self.candidate_idx)
        if num_candidate == 0:
            self._open_link(link)
            return True

        bounds = {
            ue_id_of[int(r)]: int(c)
            for r, c in zip(self.candidate_rnti, self.bounded_rbg)
            if int(r) in ue_id_of
        }
        # a backlogged UE the algorithm left out (e.g. intent pre-selection) is
        # blocked for this decision, as in the original design
        for i in range(e2KpmIndication.numUe):
            ue = e2KpmIndication.ue[i]
            ue_id = ue_id_of.get(int(ue.rnti))
            if ue.bufferSize > 0 and ue_id is not None and ue_id not in bounds:
                bounds[ue_id] = 0

        # a UE bounded by an earlier decision and not covered by this one is released
        released = sorted(self._bounded_ue_ids[link] - set(bounds))
        entries = (list(bounds.items()) + [(u, _UNBOUNDED) for u in released])[
            : ProtocolLimits.MAX_UE_NUM
        ]
        self._bounded_ue_ids[link] = {
            u for u, c in bounds.items() if c < e2KpmIndication.numRbg
        }
        self._decision_id[link] += 1

        rc = self._e2_rc_control[link]
        rc.skipThisStep = False
        rc.maxUeSchedSize = len(entries)  # C++ loops over exactly these entries
        for i, (ue_id, bound) in enumerate(entries):
            rc.ue[i].ueId = ue_id
            rc.ue[i].maxRbgAllocation = bound

            if self.save_log:
                self.store.append(
                    f"Control step: {self.stats.steps[link]} | "
                    f"{'UPLINK' if link == LinkDirection.UPLINK else 'DOWNLINK'} | "
                    f"UE ID={ue_id:1d}, RNTI={self._rnti_of[link].get(ue_id, 0):1d}, "
                    f"{f'RBG bound={bound:1d}' if bound < e2KpmIndication.numRbg else 'Unbounded'}\n"
                )

        dc = self._e2_dapp_control[link]
        dc.skipThisStep = False
        dc.maxUeSchedSize = num_candidate
        dc.numAvailableRbg = int(sum(bounds.values()))
        dc.decisionId = self._decision_id[link]
        dc.ue = [
            UeE2DAppControlRequest(
                rnti=self._rnti_of[link].get(u, 0), ueId=u, maxRbgAllocation=c
            )
            for u, c in entries
        ]

        return True

    def _open_link(self, link: LinkDirection) -> None:
        "No new bounds: release every outstanding bound and leave both schedulers free."
        released = sorted(self._bounded_ue_ids[link])[: ProtocolLimits.MAX_UE_NUM]
        self._bounded_ue_ids[link] = set()
        self._decision_id[link] += 1

        rc = self._e2_rc_control[link]
        rc.skipThisStep = False
        rc.maxUeSchedSize = ProtocolLimits.MAX_UE_NUM
        for i in range(ProtocolLimits.MAX_UE_NUM):
            if i < len(released):
                rc.ue[i].ueId = released[i]
                rc.ue[i].maxRbgAllocation = _UNBOUNDED
            else:
                rc.ue[i].ueId = 0  # UE ID 0 is never assigned: entry is inert
                rc.ue[i].maxRbgAllocation = 0

        dc = self._e2_dapp_control[link]
        dc.skipThisStep = False
        dc.maxUeSchedSize = ProtocolLimits.MAX_UE_NUM
        dc.numAvailableRbg = _UNBOUNDED
        dc.decisionId = self._decision_id[link]
        dc.ue = [
            UeE2DAppControlRequest(
                rnti=self._rnti_of[link].get(u, 0), ueId=u, maxRbgAllocation=_UNBOUNDED
            )
            for u in released
        ]

    def _reset_primary_control_to_skip(self, link_dir: LinkDirection) -> None:
        # Overrides IbOranXApp: a skipped xApp step must not mute the link.
        self._open_link(link_dir)

    def write_e2_rc_control(
        self, py2Cpp: Py2Cpp, e2KpmIndication: E2KpmIndication
    ) -> None:
        link = e2KpmIndication.linkDirection
        rc = self._e2_rc_control[link]
        msg = py2Cpp.e2RcControlRequest
        msg.type = MessageType.E2SM_RC_CONTROL_REQUEST
        msg.ricRequestorId = RicRequestId.REQUESTOR
        msg.ricInstanceId = RicRequestId.INSTANCE
        msg.ranFunctionId = RanFunctionId.RC
        msg.requestSn = self.stats.steps[link]
        msg.maxUeSchedSize = rc.maxUeSchedSize
        msg.skipThisStep = rc.skipThisStep
        msg.linkDirection = link
        for i in range(msg.maxUeSchedSize):
            msg.ue[i].ueId = rc.ue[i].ueId
            msg.ue[i].maxRbgAllocation = rc.ue[i].maxRbgAllocation

    def write_e2_dapp_control(self, link_dir: LinkDirection) -> None:
        dc = self._e2_dapp_control[link_dir]
        self.e2_dapp_.control.type = MessageType.E2SM_DAPP_CONTROL_REQUEST
        self.e2_dapp_.control.ricRequestorId = RicRequestId.REQUESTOR
        self.e2_dapp_.control.ricInstanceId = RicRequestId.INSTANCE
        self.e2_dapp_.control.ranFunctionId = RanFunctionId.DAPP
        self.e2_dapp_.control.requestSn = self.stats.steps[link_dir]
        self.e2_dapp_.control.maxUeSchedSize = dc.maxUeSchedSize
        self.e2_dapp_.control.numAvailableRbg = dc.numAvailableRbg
        self.e2_dapp_.control.skipThisStep = dc.skipThisStep
        self.e2_dapp_.control.linkDirection = link_dir
        self.e2_dapp_.control.decisionId = dc.decisionId
        self.e2_dapp_.control.ue = list(dc.ue)

    def store_e2_dapp_indication(
        self, link_dir: LinkDirection, e2_dapp_indication: E2DAppIndication
    ) -> None:
        self._e2_dapp_indication[link_dir].dAppId = e2_dapp_indication.dAppId
        self._e2_dapp_indication[link_dir].indicationSn = (
            e2_dapp_indication.indicationSn
        )
        self._e2_dapp_indication[link_dir].ueSchedSize = e2_dapp_indication.ueSchedSize
        self._e2_dapp_indication[link_dir].totalNumAllocatedRbg = (
            e2_dapp_indication.totalNumAllocatedRbg
        )
        self._e2_dapp_indication[link_dir].rbgSize = e2_dapp_indication.rbgSize
        self._e2_dapp_indication[link_dir].linkDirection = (
            e2_dapp_indication.linkDirection
        )
        self._e2_dapp_indication[link_dir].ue = list(e2_dapp_indication.ue)

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

        self._bounded_ue_ids = self._per_link(set)  # UE IDs holding a finite bound
        self._rnti_of = self._per_link(dict)  # UE ID -> last known RNTI
        self._decision_id = self._per_link(int)

        # start open: no scheduling blackout before the first xApp decision
        for link in (LinkDirection.DOWNLINK, LinkDirection.UPLINK):
            self._open_link(link)
        self.write_e2_dapp_control(LinkDirection.DOWNLINK)
