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
    E2KpmReport,
)
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    RatType,
    MessageType,
    ProtocolLimits,
)


class Cpp2PyMessageChecker:
    def __init__(self, rat_type=RatType.LTE):
        self.rat_type = rat_type

        self.max_prb_num = (
            ProtocolLimits.MAX_PRB_NUM_LTE
            if self.rat_type == RatType.LTE
            else ProtocolLimits.MAX_PRB_NUM_NR
        )

        self.errors = []

    def detect_errors(self, message=None) -> list[str]:
        self.errors = []

        if isinstance(message, E3Indication):
            self._detect_e3_indication_errors(message)
        elif isinstance(message, E2KpmReport):
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

        if message.rbgSize not in {1, 2, 3, 4}:
            self.errors.append(
                f"rbgSize ({message.rbgSize}) is invalid "
                f"(must be one of {{1, 2, 3, 4}} per TS 36.213 Table 7.1.6.1-1)"
            )

        if message.linkDirection not in [LinkDirection.DOWNLINK, LinkDirection.UPLINK]:
            self.errors.append(f"linkDirection ({message.linkDirection}) is invalid.")

        if message.numUe <= ProtocolLimits.MAX_UE_NUM:
            computed_total = sum(
                message.ue[i].bufferLevel for i in range(message.numUe)
            )
            if computed_total != message.totalBufferLevel:
                self.errors.append(
                    f"totalBufferLevel ({message.totalBufferLevel}) does not match "
                    f"sum of UE bufferLevels ({computed_total})"
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

            if not (1 <= ue.qci <= 9):
                self.errors.append(f"{prefix} has invalid QCI {ue.qci} (must be 1–9)")

            if ue.achievableRate < 0:
                self.errors.append(
                    f"{prefix} has negative achievableRate {ue.achievableRate} Mbps"
                )

            if ue.actualThroughput < 0:
                self.errors.append(
                    f"{prefix} has negative actualThroughput {ue.actualThroughput} Mbps"
                )
            if ue.actualThroughput > ue.achievableRate:
                self.errors.append(
                    f"{prefix} actualThroughput ({ue.actualThroughput:.2f} Mbps) "
                    f"exceeds achievableRate ({ue.achievableRate:.2f} Mbps)"
                )

    def _detect_e2_kpm_report_errors(self, e2KpmReport: E2KpmReport) -> None:
        if e2KpmReport.type != MessageType.E2SM_KPM_REPORT:
            self.errors.append("E2SM-KPM message type mismatch")

        if e2KpmReport.numUe > ProtocolLimits.MAX_UE_NUM:
            self.errors.append(
                f"numUe ({e2KpmReport.numUe}) exceeds MAX_UE_NUM ({ProtocolLimits.MAX_UE_NUM})"
            )

        if e2KpmReport.numRbg > self.max_prb_num:
            self.errors.append(
                f"numRbg ({e2KpmReport.numRbg}) exceeds MAX_PRB_NUM_{self.rat_type} ({self.max_prb_num})"
            )

        if e2KpmReport.rbgSize not in {1, 2, 3, 4}:
            self.errors.append(
                f"rbgSize ({e2KpmReport.rbgSize}) is invalid "
                f"(must be one of {{1, 2, 3, 4}} per TS 36.213 Table 7.1.6.1-1)"
            )

        if e2KpmReport.linkDirection not in [
            LinkDirection.DOWNLINK,
            LinkDirection.UPLINK,
        ]:
            self.errors.append(
                f"linkDirection ({e2KpmReport.linkDirection}) is invalid."
            )

        if e2KpmReport.numUe <= ProtocolLimits.MAX_UE_NUM:
            computed_total = sum(
                e2KpmReport.ue[i].bufferLevel for i in range(e2KpmReport.numUe)
            )
            if computed_total != e2KpmReport.totalBufferLevel:
                self.errors.append(
                    f"totalBufferLevel ({e2KpmReport.totalBufferLevel}) does not match "
                    f"sum of UE bufferLevels ({computed_total})"
                )

        seen_rntis = {}
        for i in range(e2KpmReport.numUe):
            ue = e2KpmReport.ue[i]
            prefix = f"UE[{i}] (link={e2KpmReport.linkDirection})"

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

            if not (1 <= ue.qci <= 9):
                self.errors.append(f"{prefix} has invalid QCI {ue.qci} (must be 1–9)")

            if ue.achievableRate < 0:
                self.errors.append(
                    f"{prefix} has negative achievableRate {ue.achievableRate} Mbps"
                )

            if ue.actualThroughput < 0:
                self.errors.append(
                    f"{prefix} has negative actualThroughput {ue.actualThroughput} Mbps"
                )
            if ue.actualThroughput > ue.achievableRate:
                self.errors.append(
                    f"{prefix} actualThroughput ({ue.actualThroughput:.2f} Mbps) "
                    f"exceeds achievableRate ({ue.achievableRate:.2f} Mbps)"
                )
