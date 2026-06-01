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


def e3_indication_validation(e3Indication: E3Indication, rat_type=LTE):
    max_prb_num = MAX_PRB_NUM_LTE
    errors = []

    if e3Indication.type != E3SM_INDICATION:
        errors.append("E3SM message type mismatch")

    if not (0 <= e3Indication.subframeNo <= 9):
        errors.append(f"subframeNo ({e3Indication.subframeNo}) out of range (0-9)")

    if e3Indication.numUe > MAX_UE_NUM:
        errors.append(f"numUe ({e3Indication.numUe}) exceeds MAX_UE_NUM ({MAX_UE_NUM})")

    if e3Indication.initRbg > max_prb_num:
        errors.append(
            f"initRbg ({e3Indication.initRbg}) exceeds MAX_PRB_NUM_{rat_type} ({max_prb_num})"
        )

    if e3Indication.numRbg > max_prb_num:
        errors.append(
            f"numRbg ({e3Indication.numRbg}) exceeds MAX_PRB_NUM_{rat_type} ({max_prb_num})"
        )

    if e3Indication.initRbg + e3Indication.numRbg > max_prb_num:
        errors.append(
            f"initRbg ({e3Indication.initRbg}) + numRbg ({e3Indication.numRbg}) "
            f"exceeds MAX_PRB_NUM_{rat_type} ({max_prb_num})"
        )

    if e3Indication.rbgSize not in {1, 2, 3, 4}:
        errors.append(
            f"rbgSize ({e3Indication.rbgSize}) is invalid "
            f"(must be one of {{1, 2, 3, 4}} per TS 36.213 Table 7.1.6.1-1)"
        )

    if e3Indication.linkDirection not in [DOWNLINK, UPLINK]:
        errors.append(
            f"linkDirection ({e3Indication.linkDirection}) is invalid "
            f"(expected {DOWNLINK}=DL or {UPLINK}=UL)"
        )

    if e3Indication.numUe <= MAX_UE_NUM:
        computed_total = sum(
            e3Indication.ue[i].bufferLevel for i in range(e3Indication.numUe)
        )
        if computed_total != e3Indication.totalBufferLevel:
            errors.append(
                f"totalBufferLevel ({e3Indication.totalBufferLevel}) does not match "
                f"sum of UE bufferLevels ({computed_total})"
            )

    seen_rntis = {}
    for i in range(e3Indication.numUe):
        ue = e3Indication.ue[i]
        prefix = f"UE[{i}] (link={e3Indication.linkDirection})"

        if ue.rnti == 0 or ue.rnti == 0xFFFF:
            errors.append(
                f"{prefix} has reserved/invalid RNTI {ue.rnti} "
                f"(valid range: 1–65534)"
            )
        if ue.rnti in seen_rntis:
            errors.append(
                f"{prefix} has duplicate RNTI {ue.rnti} "
                f"(already seen at UE[{seen_rntis[ue.rnti]}])"
            )
        else:
            seen_rntis[ue.rnti] = i

        if not (0 <= ue.cqi <= 15):
            errors.append(f"{prefix} has invalid CQI {ue.cqi} (must be 0–15)")

        if not (1 <= ue.contentCriticality <= 10):
            errors.append(
                f"{prefix} has invalid contentCriticality {ue.contentCriticality} "
                f"(must be 1–10)"
            )

        content_id_str = (
            ue.contentId
            if isinstance(ue.contentId, str)
            else ue.contentId.decode("utf-8", errors="replace")
        )
        if not content_id_str or content_id_str.strip("\x00") == "":
            errors.append(f"{prefix} has empty contentId")
        elif len(content_id_str) >= MAX_ID_LEN:
            errors.append(
                f"{prefix} contentId length ({len(content_id_str)}) "
                f"meets or exceeds MAX_ID_LEN ({MAX_ID_LEN}) — no null terminator"
            )

        if not (1 <= ue.qci <= 9):
            errors.append(f"{prefix} has invalid QCI {ue.qci} (must be 1–9)")

        if ue.achievableRate < 0:
            errors.append(
                f"{prefix} has negative achievableRate {ue.achievableRate} Mbps"
            )

        if ue.actualThroughput < 0:
            errors.append(
                f"{prefix} has negative actualThroughput {ue.actualThroughput} Mbps"
            )
        if ue.actualThroughput > ue.achievableRate:
            errors.append(
                f"{prefix} actualThroughput ({ue.actualThroughput:.2f} Mbps) "
                f"exceeds achievableRate ({ue.achievableRate:.2f} Mbps)"
            )

    return errors


def e2_kpm_report_validation(e2KpmReport, rat_type=LTE):
    max_prb_num = MAX_PRB_NUM_LTE
    errors = []

    if e2KpmReport.type != E2SM_KPM_REPORT:
        errors.append("E2SM-KPM message type mismatch")

    if e2KpmReport.numUe > MAX_UE_NUM:
        errors.append(f"numUe ({e2KpmReport.numUe}) exceeds MAX_UE_NUM ({MAX_UE_NUM})")

    if e2KpmReport.numRbg > max_prb_num:
        errors.append(
            f"numRbg ({e2KpmReport.numRbg}) exceeds MAX_PRB_NUM_{rat_type} ({max_prb_num})"
        )

    if e2KpmReport.rbgSize not in {1, 2, 3, 4}:
        errors.append(
            f"rbgSize ({e2KpmReport.rbgSize}) is invalid "
            f"(must be one of {{1, 2, 3, 4}} per TS 36.213 Table 7.1.6.1-1)"
        )

    if e2KpmReport.linkDirection not in [DOWNLINK, UPLINK]:
        errors.append(
            f"linkDirection ({e2KpmReport.linkDirection}) is invalid "
            f"(expected {DOWNLINK}=DL or {UPLINK}=UL)"
        )

    if e2KpmReport.numUe <= MAX_UE_NUM:
        computed_total = sum(
            e2KpmReport.ue[i].bufferLevel for i in range(e2KpmReport.numUe)
        )
        if computed_total != e2KpmReport.totalBufferLevel:
            errors.append(
                f"totalBufferLevel ({e2KpmReport.totalBufferLevel}) does not match "
                f"sum of UE bufferLevels ({computed_total})"
            )

    seen_rntis = {}
    for i in range(e2KpmReport.numUe):
        ue = e2KpmReport.ue[i]
        prefix = f"UE[{i}] (link={e2KpmReport.linkDirection})"

        if ue.rnti == 0 or ue.rnti == 0xFFFF:
            errors.append(
                f"{prefix} has reserved/invalid RNTI {ue.rnti} "
                f"(valid range: 1–65534)"
            )
        if ue.rnti in seen_rntis:
            errors.append(
                f"{prefix} has duplicate RNTI {ue.rnti} "
                f"(already seen at UE[{seen_rntis[ue.rnti]}])"
            )
        else:
            seen_rntis[ue.rnti] = i

        if not (0 <= ue.cqi <= 15):
            errors.append(f"{prefix} has invalid CQI {ue.cqi} (must be 0–15)")

        if not (1 <= ue.qci <= 9):
            errors.append(f"{prefix} has invalid QCI {ue.qci} (must be 1–9)")

        if ue.achievableRate < 0:
            errors.append(
                f"{prefix} has negative achievableRate {ue.achievableRate} Mbps"
            )

        if ue.actualThroughput < 0:
            errors.append(
                f"{prefix} has negative actualThroughput {ue.actualThroughput} Mbps"
            )
        if ue.actualThroughput > ue.achievableRate:
            errors.append(
                f"{prefix} actualThroughput ({ue.actualThroughput:.2f} Mbps) "
                f"exceeds achievableRate ({ue.achievableRate:.2f} Mbps)"
            )

    return errors
