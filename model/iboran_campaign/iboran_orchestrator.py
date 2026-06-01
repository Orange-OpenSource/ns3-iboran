# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

import os
import re
from collections import defaultdict
import xml.etree.ElementTree as ET
import shutil
import time

import model.iboran_interfaces.iboran_shm_structure_py_binding as shm_py_binding
from ns3ai_utils import Experiment

from model.iboran_interfaces.iboran_shm_structure import *
from model.iboran_interfaces.iboran_e2sm_dapp_structure import *
from model.iboran_controller.iboran_dapp_mac_scheduler import IbOranDAppMacScheduler
from model.iboran_controller.iboran_xapp_mac_scheduler import IbOranXAppMacScheduler
from model.utils.iqa_metric import IQAMetric
from model.utils.intent_producer import IntentProducer


class IbOranOrchestrator:
    def __init__(
        self,
        rat_type=LTE,
        dapp_control_period_sf=1,
        xapp_control_period_f=1,
        dapp_algo_ue_selection="Random",
        xapp_algo_rbg_bounding="Bypass",
        show_flowstats=False,
        show_iqa_diagnosis=False,
        save_dapp_log=False,
    ):
        self.rat_type = rat_type
        self.dapp_control_period_sf = dapp_control_period_sf
        self.xapp_control_period_f = xapp_control_period_f
        self.dapp_algo_ue_selection = dapp_algo_ue_selection
        self.xapp_algo_rbg_bounding = xapp_algo_rbg_bounding
        self.show_flowstats = show_flowstats
        self.show_iqa_diagnosis = show_iqa_diagnosis
        self.save_dapp_log = save_dapp_log

        self.dapp_scheduler: IbOranDAppMacScheduler = None
        self.xapp_scheduler: IbOranXAppMacScheduler = None

        self.e2_dapp_ = E2DappInterface()

        self.link_n_flows = {"UPLINK": 0, "DOWNLINK": 0}

        self.kpi = {
            "PDR": {"DOWNLINK": 0.0, "UPLINK": 0.0},
            "Throughput": {"DOWNLINK": 0.0, "UPLINK": 0.0},
            "Latency": {"DOWNLINK": 0.0, "UPLINK": 0.0},
            "Jitter": {"DOWNLINK": 0.0, "UPLINK": 0.0},
            "IntentSatisfactionScore": {"DOWNLINK": 0.0, "UPLINK": 0.0},
            "FidelityScore": {"DOWNLINK": 0.0, "UPLINK": 0.0},
            "SemanticFidelity": {"DOWNLINK": 0.0, "UPLINK": 0.0},
            "ContentFidelity": {"DOWNLINK": 0.0, "UPLINK": 0.0},
            "StructuralFidelity": {"DOWNLINK": 0.0, "UPLINK": 0.0},
            "PRBAllocationRatio": 0.0,
            "DecisionTimeMs": 0.0,
        }

        self.iqa_metric = IQAMetric()
        self.intent_producer = IntentProducer()
        self.intents = None

    def execute(self, k=1):
        if self.rat_type == LTE:
            exp = Experiment(
                "ns3iboran_lte",
                "../../../..",  # root dir
                shm_py_binding,
                segName="iboranSegment",
                cpp2pyMsgName="iboranCpp2Py",
                py2cppMsgName="iboranPy2Cpp",
                lockableName="iboran",
                handleFinish=True,
                shmSize=SHM_SIZE,
            )
            print("\nib-ORAN Orchestrator: 3GPP LTE established.")
        else:
            raise TypeError("Invalid RAN standard: Maybe 'LTE'?")

        shm_interface = exp.run(show_output=True)
        print("\nib-ORAN Orchestrator: Shared memory interfaces established.")

        self.intents = self.intent_producer.do(
            id_range=[1, 5],  # same as when defining apps in 'iboran-ran-common.cc'
            seed=k,  # arbitrary
        )
        self.xapp_scheduler = IbOranXAppMacScheduler(
            intents=self.intents,
            rat_type=self.rat_type,
            control_period_f=self.xapp_control_period_f,
            algo_rbg_bounding=self.xapp_algo_rbg_bounding,
        )
        self.dapp_scheduler = IbOranDAppMacScheduler(
            intents=self.intents,
            rat_type=self.rat_type,
            control_period_sf=self.dapp_control_period_sf,
            save_log=self.save_dapp_log,
            k=k,
            algo_ue_selection=self.dapp_algo_ue_selection,
        )
        print("ib-ORAN Orchestrator: MAC Scheduler xApps and dApps initialized.")

        try:
            start = time.time()
            while True:
                shm_interface.PyRecvBegin()
                if shm_interface.PyGetFinished():
                    elapsed = int(time.time() - start)
                    print(f"ib-ORAN Orchestrator: Simulation finished in {elapsed}s.")
                    break
                cpp2Py = shm_interface.GetCpp2PyStruct()
                shm_interface.PyRecvEnd()

                # invoke the xApp and dApp schedulers
                shm_interface.PySendBegin()
                py2Cpp = shm_interface.GetPy2CppStruct()
                self.e2_dapp_.control = self.xapp_scheduler.run(
                    cpp2Py, py2Cpp, self.e2_dapp_.report
                )
                self.e2_dapp_.report = self.dapp_scheduler.run(
                    cpp2Py, py2Cpp, self.e2_dapp_.control
                )
                shm_interface.PySendEnd()

        except KeyboardInterrupt:
            print("ib-ORAN Orchestrator: Interrupted by user (Ctrl+C)")

        except Exception as e:
            print(f"ib-ORAN Orchestrator: Error occurred: {e}")
            import traceback

            traceback.print_exc()
            return 1

        finally:
            print("ib-ORAN Orchestrator: Scheduling terminated.")
            self.xapp_scheduler.print_stats()
            self.dapp_scheduler.print_stats()

            self.kpi["PRBAllocationRatio"] = self.dapp_scheduler.sum_allocated_rb / (
                MAX_PRB_NUM_LTE
            )
            if self.dapp_scheduler.step > 0:
                self.kpi["DecisionTimeMs"] = (
                    10e3
                    * self.dapp_scheduler.decision_time_ms
                    / self.dapp_scheduler.step
                )

            self._get_flowstats()
            self._compute_intent_satisfaction()

            if shm_interface is not None:
                del shm_interface
            if exp is not None:
                del exp
            time.sleep(0.1)  # give OS time to fully release

        return 0

    def _get_flowstats(self):
        stats_path = os.path.join(
            self.dapp_scheduler.script_dir,
            "../../notebook/.flowstats",
            "iboran-flowstats.xml",
        )
        tree = ET.parse(stats_path)
        root = tree.getroot()

        classifiers = self._get_flow_classifiers(root)

        flowstats = root.find(".//FlowStats")
        if flowstats is not None:
            self.link_n_flows["UPLINK"] = 0
            self.link_n_flows["DOWNLINK"] = 0
            if self.show_flowstats:
                print("\n-- Flow Monitoring --------------------")
            for flow in flowstats.findall("Flow"):
                flow_id = int(flow.get("flowId", 0))
                classifier_info = classifiers.get(flow_id, {})
                tx_packets = int(flow.get("txPackets", 0))
                rx_packets = int(flow.get("rxPackets", 0))
                rx_bytes = int(flow.get("rxBytes", 0))
                time_first_tx = self._parse_time(flow.get("timeFirstTxPacket", "0ns"))
                time_last_rx = self._parse_time(flow.get("timeLastRxPacket", "0ns"))
                delay_sum = self._parse_time(flow.get("delaySum", "0ns"))
                jitter_sum = self._parse_time(flow.get("jitterSum", "0ns"))

                if self.show_flowstats:
                    print(f"{'-- Flow ID':<22}: {flow_id}")
                    print(
                        f"{'Source address':<22}: {classifier_info['source_address']}"
                    )
                    print(f"{'Source port':<22}: {classifier_info['source_port']}")
                    print(
                        f"{'Dest address':<22}: {classifier_info['destination_address']}"
                    )
                    print(f"{'Dest port':<22}: {classifier_info['destination_port']}")
                    print(f"{'Protocol':<22}: {classifier_info['protocol']}")
                    print(f"{'Packets':<22}: {tx_packets}")

                if tx_packets > 0:
                    delivery_ratio = (rx_packets / tx_packets) * 100.0
                    if self.show_flowstats:
                        print(f"{'PDR':<22}: {delivery_ratio:.2f} %")
                else:
                    delivery_ratio = 0
                    if self.show_flowstats:
                        print(f"{'PDR':<22}: nan")

                if time_last_rx > 0:
                    throughput = (rx_bytes * 8.0) / ((time_last_rx) * 1e3)  # kbps
                    if self.show_flowstats:
                        print(f"{'Throughput':<22}: {throughput:.2f} kbps")
                else:
                    throughput = 0
                    if self.show_flowstats:
                        print(f"{'Throughput':<22}: nan")

                if rx_packets > 0:
                    avg_latency = (delay_sum / rx_packets) * 1e3  # ms
                    if self.show_flowstats:
                        print(f"{'Avg latency':<22}: {avg_latency:.2f} ms")

                    if rx_packets > 1:
                        avg_jitter = (jitter_sum / (rx_packets - 1)) * 1e3  # ms
                        if self.show_flowstats:
                            print(f"{'Avg jitter':<22}: {avg_jitter:.2f} ms")
                    else:
                        avg_jitter = 0
                        if self.show_flowstats:
                            print(f"{'Avg jitter':<22}: nan")
                else:
                    avg_latency = 0
                    avg_jitter = 0
                    if self.show_flowstats:
                        print(f"{'Avg latency':<22}: nan")
                        print(f"{'Avg jitter':<22}: nan")
                if self.show_flowstats:
                    print()

                if classifier_info["source_address"] == "1.0.0.2" and classifier_info[
                    "destination_address"
                ].startswith("7.0.0"):
                    self.kpi["PDR"]["DOWNLINK"] += delivery_ratio
                    self.kpi["Throughput"]["DOWNLINK"] += throughput
                    self.kpi["Latency"]["DOWNLINK"] += avg_latency
                    self.kpi["Jitter"]["DOWNLINK"] += avg_jitter
                    self.link_n_flows["DOWNLINK"] += 1
                elif classifier_info[
                    "destination_address"
                ] == "1.0.0.2" and classifier_info["source_address"].startswith(
                    "7.0.0"
                ):
                    self.kpi["PDR"]["UPLINK"] += delivery_ratio
                    self.kpi["Throughput"]["UPLINK"] += throughput
                    self.kpi["Latency"]["UPLINK"] += avg_latency
                    self.kpi["Jitter"]["UPLINK"] += avg_jitter
                    self.link_n_flows["UPLINK"] += 1

            if not self.show_flowstats:
                print()
            print("-- Overall Network Performance --------")
            for link in ["DOWNLINK", "UPLINK"]:
                self.kpi["PDR"][link] /= self.link_n_flows[link]
                self.kpi["Throughput"][link] /= self.link_n_flows[link]
                self.kpi["Latency"][link] /= self.link_n_flows[link]
                self.kpi["Jitter"][link] /= self.link_n_flows[link]
                print(f"-- {link}")
                print(f"{'Avg PDR':<22}: {self.kpi['PDR'][link]:.2f} %")
                print(
                    f"{'Total throughput':<22}: {self.kpi['Throughput'][link] * self.link_n_flows[link]:.2f} kbps"
                )
                print(
                    f"{'Avg throughput':<22}: {self.kpi['Throughput'][link]:.2f} kbps"
                )
                print(f"{'Avg latency':<22}: {self.kpi['Latency'][link]:.2f} ms")
                print(f"{'Avg jitter':<22}: {self.kpi['Jitter'][link]:.2f} ms")
                print()

    @staticmethod
    def _get_flow_classifiers(root):
        classifiers = {}
        ipv4_classifier = root.find(".//Ipv4FlowClassifier")
        if ipv4_classifier is not None:
            for flow in ipv4_classifier.findall("Flow"):
                flow_id = int(flow.get("flowId", 0))
                classifiers[flow_id] = {
                    "source_address": flow.get("sourceAddress", "N/A"),
                    "destination_address": flow.get("destinationAddress", "N/A"),
                    "source_port": flow.get("sourcePort", "N/A"),
                    "destination_port": flow.get("destinationPort", "N/A"),
                    "protocol": flow.get("protocol", "N/A"),
                }
        return classifiers

    @staticmethod
    def _parse_time(time_string):
        if not time_string:
            return 0.0
        time_string = time_string.replace("+", "").strip()
        if time_string.endswith("ns"):
            return float(time_string[:-2]) / 1e9
        return 0.0

    def _compute_intent_satisfaction(self):
        if self.show_iqa_diagnosis:
            print("-- Intent Satisfaction Evaluation -----")
        else:
            print("Analyzing intent satisfaction...")
        link_abbr = {"DOWNLINK": "dl", "UPLINK": "ul"}
        output_dir = os.path.join(
            self.dapp_scheduler.script_dir, "../../notebook/dataset_dscv"
        )

        for link, abbr in link_abbr.items():
            # find different object IDs assigned to the UEs
            idx_to_object_id = self._find_object_ids_for_ues(output_dir, abbr)

            ue_num = int(
                self.link_n_flows[link] / 2
            )  # flowstats file has two sets of flows per UE
            for idx in range(ue_num):
                object_id = str(idx_to_object_id.get(idx, "0"))
                input_path = os.path.join(
                    self.dapp_scheduler.script_dir,
                    f"../../notebook/dataset/input_{abbr}_{object_id}.png",
                )
                output_path = os.path.join(
                    output_dir, f"output_{abbr}_{object_id}_ueidx_{idx}.png"
                )
                if not os.path.exists(output_path):
                    continue
                if self.show_iqa_diagnosis:
                    print(f"{'-- Active UE':<22}: {idx+1}")
                    print(f"{'Link direction':<22}: {link}")
                    print(f"{'Object ID':<22}: {object_id}")

                # derive IQA (Image Quality Assessment) metrics
                # NOTE: need other metrics for other data formats rather than IMAGE
                done = self.iqa_metric.compute(
                    input_path,
                    output_path,
                    diagnosis_needed=self.show_iqa_diagnosis,
                )
                if not done:
                    print("ib-ORAN Orchestrator: IQA operation failed.\n")
                    continue

                intent_relevance = (
                    1
                    if object_id
                    == self.intents[DOWNLINK if link == "DOWNLINK" else UPLINK]
                    else 0
                )
                self.kpi["IntentSatisfactionScore"][link] += intent_relevance * (
                    self.iqa_metric.fidelity_score / ue_num
                )
                self.kpi["FidelityScore"][link] += (
                    self.iqa_metric.fidelity_score / ue_num
                )
                self.kpi["SemanticFidelity"][link] += intent_relevance * (
                    self.iqa_metric.semantic_fidelity / ue_num
                )
                self.kpi["ContentFidelity"][link] += intent_relevance * (
                    self.iqa_metric.content_fidelity / ue_num
                )
                self.kpi["StructuralFidelity"][link] += intent_relevance * (
                    self.iqa_metric.structural_fidelity / ue_num
                )

                if self.show_iqa_diagnosis:
                    print(
                        f"{'Intent satisfaction':<22}: "
                        f"{intent_relevance * self.iqa_metric.fidelity_score:.2f}"
                    )
                    self.iqa_metric.display_recovery_map()
                    print()

    def _find_object_ids_for_ues(self, output_dir, link_abbr):
        idx_object_id_mapping = defaultdict(list)
        pattern = rf"output_{link_abbr}_(\d+)_ueidx_(\d+)\.png$"
        for filename in os.listdir(output_dir):
            match = re.match(pattern, filename)
            if match:
                object_id = int(match.group(1))
                ue_idx = int(match.group(2))
                filepath = os.path.join(output_dir, filename)
                timestamp = os.path.getmtime(filepath)  # to find the latest IDs
                idx_object_id_mapping[ue_idx].append((object_id, timestamp))

        return {
            idx: max(pairs, key=lambda x: x[1])[0]
            for idx, pairs in idx_object_id_mapping.items()
        }

    def cleanup(self):
        path_to_notebook = os.path.normpath(
            os.path.join(self.dapp_scheduler.script_dir, "../../notebook")
        )
        paths = {"flowstats": os.path.join(path_to_notebook, ".flowstats")}

        for name, path in paths.items():
            if os.path.exists(path):
                try:
                    shutil.rmtree(path)
                except PermissionError as e:
                    raise PermissionError(
                        f"ib-ORAN Orchestrator: Could not remove {name} folder — "
                        f"file may be locked or permissions insufficient.\n{e}"
                    )
