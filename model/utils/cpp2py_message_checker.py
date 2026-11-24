# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

from model.iboran_interfaces.iboran_shm_structure_py_binding import (
    E3Indication,
    E2KpmIndication,
)
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    RatType,
    MessageType,
    ProtocolLimits,
    RanFunctionId,
)


class Cpp2PyMessageChecker:
    def __init__(self, rat_type=RatType.LTE):
        self.rat_type = rat_type

        self.max_prb_num = (
            ProtocolLimits.MAX_PRB_NUM_LTE
            if self.rat_type == RatType.LTE
            else ProtocolLimits.MAX_PRB_NUM_NR
        )

        self._rbg_size_valid_values = (
            {1, 2, 3, 4}  # TS 36.213 Table 7.1.6.1-1
            if self.rat_type == RatType.LTE
            # 5G-LENA allows any size; TS 38.214 Type 0 uses 2-16
            else set(range(1, 17))
        )

        self.errors = []

    def detect_errors(self, message=None) -> list[str]:
        self.errors = []

        if isinstance(message, E3Indication):
            self._detect_e3_indication_errors(message)
        elif isinstance(message, E2KpmIndication):
            self._detect_e2_kpm_report_errors(message)
        else:
            raise ValueError(
                f"Message does not follow any defined structure: got {type(message).__name__}."
            )

        return self.errors

    def _detect_e3_indication_errors(self, message: E3Indication) -> None:
        if message.type != MessageType.E3SM_INDICATION:
            self.errors.append("E3SM message type mismatch")

        if not (0 <= message.subframeNo <= 9):
            self.errors.append(f"subframeNo ({message.subframeNo}) out of range (0-9)")

        if message.numUe > ProtocolLimits.MAX_UE_NUM:
            self.errors.append(
                f"numUe ({message.numUe}) exceeds MAX_UE_NUM ({ProtocolLimits.MAX_UE_NUM})"
            )

        if message.initRbg > self.max_prb_num:
            self.errors.append(
                f"initRbg ({message.initRbg}) exceeds MAX_PRB_NUM_{self.rat_type} ({self.max_prb_num})"
            )

        if message.numRbg > self.max_prb_num:
            self.errors.append(
                f"numRbg ({message.numRbg}) exceeds MAX_PRB_NUM_{self.rat_type} ({self.max_prb_num})"
            )

        if message.initRbg + message.numRbg > self.max_prb_num:
            self.errors.append(
                f"initRbg ({message.initRbg}) + numRbg ({message.numRbg}) "
                f"exceeds MAX_PRB_NUM_{self.rat_type} ({self.max_prb_num})"
            )

        if message.rbgSize not in self._rbg_size_valid_values:
            self.errors.append(
                f"rbgSize ({message.rbgSize}) is invalid "
                f"(must be one of {self._rbg_size_valid_values} for {self.rat_type})."
            )

        if message.linkDirection not in [LinkDirection.DOWNLINK, LinkDirection.UPLINK]:
            self.errors.append(f"linkDirection ({message.linkDirection}) is invalid.")

        if message.numUe <= ProtocolLimits.MAX_UE_NUM:
            computed_total = sum(message.ue[i].bufferSize for i in range(message.numUe))
            if computed_total != message.totalBufferSize:
                self.errors.append(
                    f"totalBufferSize ({message.totalBufferSize}) does not match "
                    f"sum of UE bufferSizes ({computed_total})"
                )

        seen_rntis = {}
        for i in range(message.numUe):
            ue = message.ue[i]
            prefix = f"UE[{i}] (link={message.linkDirection})"

            if ue.rnti == 0 or ue.rnti == 0xFFFF:
                self.errors.append(
                    f"{prefix} has reserved/invalid RNTI {ue.rnti} "
                    f"(valid range: 1–65534)"
                )
            if ue.rnti in seen_rntis:
                self.errors.append(
                    f"{prefix} has duplicate RNTI {ue.rnti} "
                    f"(already seen at UE[{seen_rntis[ue.rnti]}])"
                )
            else:
                seen_rntis[ue.rnti] = i

            if not (0 <= ue.cqi <= 15):
                self.errors.append(f"{prefix} has invalid CQI {ue.cqi} (must be 0–15)")

            if not (1 <= ue.contentCriticality <= 10):
                self.errors.append(
                    f"{prefix} has invalid contentCriticality {ue.contentCriticality} "
                    f"(must be 1–10)"
                )

            content_id_str = (
                ue.contentId
                if isinstance(ue.contentId, str)
                else ue.contentId.decode("utf-8", errors="replace")
            )
            if not content_id_str or content_id_str.strip("\x00") == "":
                self.errors.append(f"{prefix} has empty contentId")
            elif len(content_id_str) >= ProtocolLimits.MAX_ID_LEN:
                self.errors.append(
                    f"{prefix} contentId length ({len(content_id_str)}) "
                    f"meets or exceeds MAX_ID_LEN ({ProtocolLimits.MAX_ID_LEN}) — no null terminator"
                )

            max_qos_id = 9 if self.rat_type == RatType.LTE else 255
            if not (1 <= ue.qosId <= max_qos_id):
                self.errors.append(
                    f"{prefix} has invalid QoS ID {ue.qosId} (must be 1–{max_qos_id})"
                )

            if ue.achievableRate < 0:
                self.errors.append(
                    f"{prefix} has negative achievableRate {ue.achievableRate} kbps"
                )

            if ue.drbUeThp < 0:
                self.errors.append(
                    f"{prefix} has negative DRB.UEThp {ue.drbUeThp} kbps"
                )

    def _detect_e2_kpm_report_errors(self, e2KpmIndication: E2KpmIndication) -> None:
        if e2KpmIndication.type != MessageType.E2SM_KPM_INDICATION:
            self.errors.append("E2SM-KPM message type mismatch")

        if e2KpmIndication.ranFunctionId != RanFunctionId.KPM:
            self.errors.append(
                f"E2SM-KPM RAN function ID {e2KpmIndication.ranFunctionId} "
                f"is not {int(RanFunctionId.KPM)}"
            )

        if e2KpmIndication.numUe > ProtocolLimits.MAX_UE_NUM:
            self.errors.append(
                f"numUe ({e2KpmIndication.numUe}) exceeds MAX_UE_NUM ({ProtocolLimits.MAX_UE_NUM})"
            )

        if e2KpmIndication.numRbg > self.max_prb_num:
            self.errors.append(
                f"numRbg ({e2KpmIndication.numRbg}) exceeds MAX_PRB_NUM_{self.rat_type} ({self.max_prb_num})"
            )

        if e2KpmIndication.rbgSize not in self._rbg_size_valid_values:
            self.errors.append(
                f"rbgSize ({e2KpmIndication.rbgSize}) is invalid "
                f"(must be one of {self._rbg_size_valid_values} for {self.rat_type})."
            )

        if e2KpmIndication.linkDirection not in [
            LinkDirection.DOWNLINK,
            LinkDirection.UPLINK,
        ]:
            self.errors.append(
                f"linkDirection ({e2KpmIndication.linkDirection}) is invalid."
            )

        seen_rntis = {}
        for i in range(e2KpmIndication.numUe):
            ue = e2KpmIndication.ue[i]
            prefix = f"UE[{i}] (link={e2KpmIndication.linkDirection})"

            if ue.rnti == 0 or ue.rnti == 0xFFFF:
                self.errors.append(
                    f"{prefix} has reserved/invalid RNTI {ue.rnti} "
                    f"(valid range: 1–65534)"
                )
            if ue.rnti in seen_rntis:
                self.errors.append(
                    f"{prefix} has duplicate RNTI {ue.rnti} "
                    f"(already seen at UE[{seen_rntis[ue.rnti]}])"
                )
            else:
                seen_rntis[ue.rnti] = i

            if not (0 <= ue.cqi <= 15):
                self.errors.append(f"{prefix} has invalid CQI {ue.cqi} (must be 0–15)")

            max_qos_id = 9 if self.rat_type == RatType.LTE else 255
            if not (1 <= ue.qosId <= max_qos_id):
                self.errors.append(
                    f"{prefix} has invalid QoS ID {ue.qosId} (must be 1–{max_qos_id})"
                )

            if ue.drbUeThp < 0:
                self.errors.append(
                    f"{prefix} has negative DRB.UEThp {ue.drbUeThp} kbps"
                )
