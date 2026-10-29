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
import glob
import re
from pathlib import Path
import xml.etree.ElementTree as ET
from dataclasses import replace
import traceback
import shutil
import time
import numpy as np
import torch

import model.iboran_interfaces.iboran_shm_structure_py_binding as shm_py_binding
from ns3ai_utils import Experiment

from model.iboran_interfaces.iboran_e2sm_dapp_structure import E2DappInterface
from model.iboran_controller.iboran_dapp_mac_scheduler import IbOranDAppMacScheduler
from model.iboran_controller.iboran_xapp_mac_scheduler import IbOranXAppMacScheduler
from model.iboran_interfaces.iboran_shm_structure import SHM_SIZE
from model.iboran_interfaces.iboran_shm_protocol import (
    LinkDirection,
    RatType,
)
from model.iboran_intent_producer.iboran_intent_producer_pseudo_random import (
    IbOranIntentProducerPseudoRandom,
)
from model.iboran_kpi.iboran_kpi import KPI
from model.iboran_kpi.iboran_kpi_iqa import IbOranIQA
from model.iboran_kpi.iboran_kpi_ecs import IbOranECS
from model.utils.config_store import CommonRanParams
from model.utils.embedding_store import EmbeddingStore
from model.utils.file_store import FileStore

FILE_EXTENSION = {
    "IMAGE": ".png",
    "VIDEO": ".mp4",
    "EMBEDDING": ".f32",
    "TEXT": ".txt",
}

STD_KPI_TEMPLATES = {
    "PDR": KPI("PDR", unit="(%)", sort="desc"),
    "Throughput": KPI("Throughput", unit="(kbps)", sort="desc"),
    "Latency": KPI("Latency", unit="(ms)", sort="asc"),
    "Jitter": KPI("Jitter", unit="(ms)", sort="asc"),
}

_SPEC_RE = re.compile(r"%(0\d+)?([sd])")

_BASE_KPI_TEMPLATES = {
    **STD_KPI_TEMPLATES,
    "SatisfactionScore": KPI("Intent Satisfaction Score", sort="desc"),
    "PRBUtilizationRatio": KPI("RBG Usage", unit="(%)", sort="asc"),
    "DAppDecisionTime": KPI("dApp Decision Time", unit="(ms)", sort="asc"),
    "XAppDecisionTime": KPI("xApp Decision Time", unit="(ms)", sort="asc"),
}

_FLOW_KEYS = {
    "BitStats": ["txPackets", "rxPackets", "rxBytes"],
    "TimeStats": ["timeFirstTxPacket", "timeLastRxPacket", "delaySum", "jitterSum"],
    "LinkStats": [
        "sourceAddress",
        "destinationAddress",
        "sourcePort",
        "destinationPort",
        "protocol",
    ],
}

LINKS = [LinkDirection.UPLINK, LinkDirection.DOWNLINK]


class IbOranOrchestrator:
    def __init__(
        self,
        rat_type: RatType,
        dapp_control_period_sf: int,
        xapp_control_period_f: int,
        dapp_algo_ue_selection: str,
        xapp_algo_rbg_bounding: str,
        learning_mode: str,
        dataset_id: str,
        data_format: str,
        embedder: str,
        oid_range: tuple[int, int],
        save_dapp_log: bool,
        save_learning_convg: bool,
        verbose: bool,
        common_params: CommonRanParams,
        datasets: dict[str, dict],
    ):
        self.rat_type = rat_type
        self.dapp_control_period_sf = dapp_control_period_sf
        self.xapp_control_period_f = xapp_control_period_f
        self.dapp_algo_ue_selection = dapp_algo_ue_selection
        self.xapp_algo_rbg_bounding = xapp_algo_rbg_bounding
        self.learning_mode = learning_mode
        self.dataset_id = dataset_id
        self.data_format = data_format
        self.embedder = embedder
        self.oid_range = oid_range
        self.save_dapp_log = save_dapp_log
        self.save_learning_convg = save_learning_convg
        self.verbose = verbose
        self.common_params = common_params
        self.datasets = datasets

        self.dapp_scheduler: IbOranDAppMacScheduler = None
        self.xapp_scheduler: IbOranXAppMacScheduler = None

        self.e2_dapp_ = E2DappInterface()

        if self.data_format == "EMBEDDING" and self.embedder not in [
            "TransReID",
            "CLIP",
        ]:
            raise ValueError(
                f"\nib-ORAN Orchestrator: Unknown embedder '{self.embedder}'."
            )

        self.intent_producer = IbOranIntentProducerPseudoRandom(
            oid_range=self.oid_range
        )

        self.dataset = DatasetPatternResolver(
            self.dataset_id, self.data_format, self.datasets
        )

        self._init_paths()

        self.flow_stats = FlowStatsAnalyzer(self.notebook_dir, verbose=self.verbose)

    def execute(self, k=1) -> int:
        self.k = k  # part of the checkpoint key: a policy is trained per k

        # eval without a trained checkpoint would silently evaluate initial weights
        uses_learning = any(
            "Learning" in name
            for name in (self.dapp_algo_ue_selection, self.xapp_algo_rbg_bounding)
        )
        if (
            self.learning_mode == "eval"
            and uses_learning
            and not self._checkpoint_path().exists()
        ):
            raise FileNotFoundError(
                f"ib-ORAN Orchestrator: eval mode needs {self._checkpoint_path()}; "
                f"run IbOranCampaign.pretrain for this pattern first."
            )

        if self.rat_type == RatType.LTE:
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

        elif self.rat_type == RatType.NR:
            exp = Experiment(
                "ns3iboran_nr",
                "../../../..",
                shm_py_binding,
                segName="iboranSegment",
                cpp2pyMsgName="iboranCpp2Py",
                py2cppMsgName="iboranPy2Cpp",
                lockableName="iboran",
                handleFinish=True,
                shmSize=SHM_SIZE,
            )
            print("\nib-ORAN Orchestrator: 3GPP NR established.")

        else:
            raise TypeError("Invalid RAN standard: Maybe 'LTE' or 'NR'?")

        self._prepare_kpi()

        shm_interface = exp.run(show_output=True)
        print("\nib-ORAN Orchestrator: Shared memory interfaces established.")

        self.intent_producer.produce(
            num_intended_oids=1,
            seed=self.common_params.rngRun,
        )

        self.xapp_scheduler = IbOranXAppMacScheduler(
            intents=self.intent_producer.intents,
            rat_type=self.rat_type,
            control_period_f=self.xapp_control_period_f,
            algo_rbg_bounding=self.xapp_algo_rbg_bounding,
        )

        self.dapp_scheduler = IbOranDAppMacScheduler(
            intents=self.intent_producer.intents,
            rat_type=self.rat_type,
            control_period_sf=self.dapp_control_period_sf,
            save_log=self.save_dapp_log,
            k=k,
            algo_ue_selection=self.dapp_algo_ue_selection,
        )

        ckpt = self._checkpoint_path()
        if self.learning_mode in ("train", "eval") and ckpt.exists():
            state = torch.load(ckpt, weights_only=False)
            for role, learner in self._learners().items():
                if role in state:
                    learner.load_state_dict(state[role])

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
            print("ib-ORAN Orchestrator: Interrupted by user.")

        except Exception as e:
            print(f"ib-ORAN Orchestrator: Error occurred: {e}")
            traceback.print_exc()
            return 1

        finally:
            print("ib-ORAN Orchestrator: Scheduling terminated.")

            if self.verbose:
                self.xapp_scheduler.print_stats()
                self.dapp_scheduler.print_stats()

            for link in LINKS:
                dapp_stats, xapp_stats = (
                    self.dapp_scheduler.stats,
                    self.xapp_scheduler.stats,
                )

                if (
                    dapp_stats.decisions[link] > 0
                    and self.dapp_scheduler.sum_available_rb[link] > 0
                ):
                    self.kpi["PRBUtilizationRatio"][link].measure = (
                        100.0
                        * self.dapp_scheduler.sum_allocated_rb[link]
                        / self.dapp_scheduler.sum_available_rb[link]
                    )
                    self.kpi["DAppDecisionTime"][link].measure = (
                        1e3
                        * dapp_stats.decision_time[link]
                        / dapp_stats.decisions[link]
                    )  # ms

                if xapp_stats.decisions[link] > 0:
                    self.kpi["XAppDecisionTime"][link].measure = (
                        1e3
                        * xapp_stats.decision_time[link]
                        / xapp_stats.decisions[link]
                    )  # ms

            self.flow_stats.compute_std_kpis(self.kpi)
            self.compute_ib_kpis()

            if self.learning_mode == "train" and self._learners():
                ckpt = self._checkpoint_path()
                ckpt.parent.mkdir(parents=True, exist_ok=True)
                torch.save(
                    {r: a.state_dict() for r, a in self._learners().items()}, ckpt
                )
                if self.save_learning_convg:
                    for learner in self._learners().values():
                        if hasattr(learner, "save_convergence"):
                            learner.save_convergence(ckpt.stem)

            if shm_interface is not None:
                del shm_interface
            if exp is not None:
                del exp
            time.sleep(0.1)  # give OS time to fully release

        return 0

    def _checkpoint_path(self) -> Path:
        return (
            self.notebook_dir
            / "logs"
            / "checkpoints"
            / (
                f"{self.rat_type}_{self.dapp_algo_ue_selection}_{self.xapp_algo_rbg_bounding}"
                f"_{self.dapp_control_period_sf}_{self.xapp_control_period_f}_{self.k}.pt"
            )
        )

    def _learners(self) -> dict:
        roles = {
            "dapp": self.dapp_scheduler.algo_ue_selection,
            "xapp": self.xapp_scheduler.algo_rbg_bounding,
        }
        return {r: a for r, a in roles.items() if hasattr(a, "state_dict")}

    def compute_ib_kpis(self) -> None:
        if self.verbose:
            header = "-- Network Intent-Based KPIs "
            print(f"\n{header:-<39}")
        else:
            print("Analyzing intent satisfaction...")
        link_abbr = {LinkDirection.UPLINK: "ul", LinkDirection.DOWNLINK: "dl"}

        for link, abbr in link_abbr.items():
            # find different object IDs assigned to the UEs
            idx_to_object_id = self.dataset.find_object_id_for_ues(
                self.notebook_dir,
                abbr,
                self.common_params.ueNum,
            )

            # SatisfactionScore = mean over all UEs of relevance * fidelity,
            # i.e. the FidelityScore restricted to intent-relevant UEs, so
            # SatisfactionScore <= FidelityScore always
            sat_sum = 0.0

            for idx in range(self.common_params.ueNum):
                raw_object_id = idx_to_object_id.get(idx)
                if raw_object_id is None:
                    continue
                object_id = str(raw_object_id)

                intent_relevance = (
                    1 if object_id in self.intent_producer.intents[link] else 0
                )

                output_stem = self.dataset.build_stem(
                    "output",
                    self.dataset_dscv_dir,
                    abbr=abbr,
                    object_id=int(object_id),
                    idx=idx + 1,
                )
                output_path = output_stem.with_suffix(
                    FILE_EXTENSION.get(self.data_format)
                )
                if not glob.glob(str(output_path)):
                    continue

                self._vprint(f"{'-- Active UE':<22}: {idx+1}")
                self._vprint(f"{'Link direction':<22}: {link}")
                self._vprint(f"{'Object ID':<22}: {object_id}")

                try:
                    input_path = self.dataset.resolve_input_file(
                        self.dataset_dir,
                        abbr=abbr,
                        object_id=int(object_id),
                        camera_id=idx + 1,
                    )
                except FileNotFoundError:
                    continue

                done = self.ib_kpi_backend.compute(input_path, output_path)
                if not done:
                    print("ib-ORAN Orchestrator: IB-KPI computation failed.\n")
                    continue

                ue_fidelity = 0.0
                for key in self.ib_kpi_backend.metrics.keys():
                    measure = self.ib_kpi_backend.metrics[key].measure
                    if not np.isfinite(measure):
                        measure = 0.0  # one NaN must not poison the link aggregate
                    self.kpi[key][link].measure += measure / self.common_params.ueNum
                    if key == "FidelityScore":
                        ue_fidelity = measure

                sat_sum += intent_relevance * ue_fidelity

                self._vprint(
                    f'{"Intent satisfaction":<22}: '
                    f"{intent_relevance * ue_fidelity:.2f}"
                )

                if self.verbose:
                    if self.data_format == "IMAGE":
                        print()
                        self.ib_kpi_backend.print_spatial_map()
                    print()

            self.kpi["SatisfactionScore"][link].measure = (
                sat_sum / self.common_params.ueNum
            )

    def _prepare_kpi(self) -> None:
        if self.data_format == "IMAGE":
            self.ib_kpi_backend = IbOranIQA(verbose=self.verbose)

        elif self.data_format == "VIDEO":
            raise NotImplementedError

        elif self.data_format == "EMBEDDING":
            self.emb_store = EmbeddingStore(
                extraction_model=self.embedder,
            )
            done = self.emb_store.extract(
                dir=self.dataset_dir,
                pattern="*",
                extension=FILE_EXTENSION.get("IMAGE", ".png"),
            )
            if not done:
                raise RuntimeError("ib-ORAN Orchestrator: Embedding extraction failed.")
            print("\nib-ORAN Orchestrator: Embeddings extracted.")
            self.ib_kpi_backend = IbOranECS(
                extraction_model=self.embedder,
                verbose=self.verbose,
            )

        elif self.data_format == "TEXT":
            raise NotImplementedError

        else:
            raise ValueError(
                f"Data format must be one of: {list(FILE_EXTENSION.keys())}."
            )

        kpi_templates = {**_BASE_KPI_TEMPLATES, **self.ib_kpi_backend.metrics}
        self.kpi = {
            key: self._link_pair(template) for key, template in kpi_templates.items()
        }

    @staticmethod
    def _link_pair(template: KPI) -> dict[LinkDirection, KPI]:
        return {
            LinkDirection.DOWNLINK: replace(template, measure=0.0),
            LinkDirection.UPLINK: replace(template, measure=0.0),
        }

    def _vprint(self, *args, **kwargs) -> None:
        if self.verbose:
            print(*args, **kwargs)

    def _init_paths(self) -> None:
        self.abs_path = os.path.dirname(os.path.abspath(__file__))
        self.notebook_dir = Path(self.abs_path) / "../../notebook"
        repo = self.dataset.patterns["repository"]
        self.dataset_dir = self.notebook_dir / "dataset" / repo
        self.dataset_dscv_dir = self.notebook_dir / "dataset_dscv" / repo

    def cleanup(self) -> None:
        paths = {"flowstats": self.notebook_dir / ".flowstats"}

        for name, path in paths.items():
            if os.path.exists(path):
                try:
                    shutil.rmtree(path)
                except PermissionError as e:
                    raise PermissionError(
                        f"ib-ORAN Orchestrator: Could not remove {name} folder — "
                        f"file may be locked or permissions insufficient.\n{e}"
                    )


class FlowStatsAnalyzer:
    def __init__(self, notebook_dir: Path, verbose: bool = False):
        self.notebook_dir = notebook_dir
        self.verbose = verbose
        self.link_n_flows = self._per_link(int)

    def compute_std_kpis(self, kpi: dict) -> None:
        self.link_n_flows = self._per_link(int)
        agg = {
            l: dict(
                tx=0,
                rx=0,
                bytes=0,
                delay=0.0,
                jitter=0.0,
                jit_n=0,
                t0=float("inf"),
                t1=0.0,
            )
            for l in LINKS
        }

        for stats, _ in self._get_flowstats():
            if stats["sourceAddress"] == "1.0.0.2" and stats[
                "destinationAddress"
            ].startswith("7.0.0"):
                link = LinkDirection.DOWNLINK
            elif stats["destinationAddress"] == "1.0.0.2" and stats[
                "sourceAddress"
            ].startswith("7.0.0"):
                link = LinkDirection.UPLINK
            else:
                continue
            a = agg[link]
            a["tx"] += stats["txPackets"]
            a["rx"] += stats["rxPackets"]
            a["bytes"] += stats["rxBytes"]
            a["delay"] += stats["delaySum"]
            a["jitter"] += stats["jitterSum"]
            a["jit_n"] += max(stats["rxPackets"] - 1, 0)
            if stats["rxPackets"] > 0:
                a["t0"] = min(a["t0"], stats["timeFirstTxPacket"])
                a["t1"] = max(a["t1"], stats["timeLastRxPacket"])
            self.link_n_flows[link] += 1

        for link, a in agg.items():
            if a["tx"] > 0:
                kpi["PDR"][link].measure = 100.0 * a["rx"] / a["tx"]
            if a["rx"] > 0:
                kpi["Latency"][link].measure = 1e3 * a["delay"] / a["rx"]  # ms
            if a["jit_n"] > 0:
                kpi["Jitter"][link].measure = 1e3 * a["jitter"] / a["jit_n"]  # ms
            if a["t1"] > a["t0"] and self.link_n_flows[link] > 0:
                kpi["Throughput"][link].measure = (
                    a["bytes"]
                    * 8.0
                    / (a["t1"] - a["t0"])
                    / 1e3
                    / self.link_n_flows[link]
                )  # kbps per UE

        if self.verbose:
            self._print_std_kpis(kpi)

    def _print_std_kpis(self, kpi: dict) -> None:
        header = "-- Network Standard KPIs "
        print(f"\n{header:-<39}")
        for link in LINKS:
            print(f"-- {link}")
            print(f"{'Avg PDR':<22}: {kpi['PDR'][link].measure:.2f} %")
            print(
                f"{'Total throughput':<22}: {kpi['Throughput'][link].measure * self.link_n_flows[link]:.2f} kbps"
            )
            print(f"{'Avg throughput':<22}: {kpi['Throughput'][link].measure:.2f} kbps")
            print(f"{'Avg latency':<22}: {kpi['Latency'][link].measure:.2f} ms")
            print(f"{'Avg jitter':<22}: {kpi['Jitter'][link].measure:.2f} ms")
            print()

    def _get_flowstats(self) -> list[tuple[dict, dict]]:
        stats_path = self.notebook_dir / ".flowstats" / "iboran-flowstats.xml"
        tree = ET.parse(stats_path)
        root = tree.getroot()
        classifiers = self._get_flow_classifiers(root)

        flowstats = root.find(".//FlowStats")
        if flowstats is None:
            return []

        header = "-- Flow Monitoring "
        self._vprint(f"\n{header:-<39}")

        results = []
        for flow in flowstats.findall("Flow"):
            stats, flow_id = self._parse_flow(flow, classifiers)
            self._vprint(f"{'-- Flow ID':<22}: {flow_id}")
            self._vprint(f"{'Source address':<22}: {stats['sourceAddress']}")
            self._vprint(f"{'Source port':<22}: {stats['sourcePort']}")
            self._vprint(f"{'Dest address':<22}: {stats['destinationAddress']}")
            self._vprint(f"{'Dest port':<22}: {stats['destinationPort']}")
            self._vprint(f"{'Protocol':<22}: {stats['protocol']}")
            self._vprint(f"{'Packets':<22}: {stats['txPackets']}")
            flow_kpis = self._computed_flow_kpis(stats)
            results.append((stats, flow_kpis))

        if not self.verbose:
            print()

        return results

    def _computed_flow_kpis(self, stats) -> dict[str, KPI]:
        flow_kpis = {
            key: replace(template) for key, template in STD_KPI_TEMPLATES.items()
        }
        if stats["txPackets"] > 0:
            flow_kpis["PDR"].measure = (stats["rxPackets"] / stats["txPackets"]) * 100.0
            self._vprint(f"{'PDR':<22}: {flow_kpis['PDR'].measure:.2f} %")
        else:
            flow_kpis["PDR"].measure = 0
            self._vprint(f"{'PDR':<22}: nan")

        duration = stats["timeLastRxPacket"] - stats["timeFirstTxPacket"]
        if duration > 0:
            flow_kpis["Throughput"].measure = (stats["rxBytes"] * 8.0) / (
                duration * 1e3
            )  # kbps
            self._vprint(
                f"{'Throughput':<22}: {flow_kpis['Throughput'].measure:.2f} kbps"
            )
        else:
            flow_kpis["Throughput"].measure = 0
            self._vprint(f"{'Throughput':<22}: nan")

        if stats["rxPackets"] > 0:
            flow_kpis["Latency"].measure = (
                stats["delaySum"] / stats["rxPackets"]
            ) * 1e3  # ms
            self._vprint(f"{'Latency':<22}: {flow_kpis['Latency'].measure:.2f} ms")

            if stats["rxPackets"] > 1:
                flow_kpis["Jitter"].measure = (
                    stats["jitterSum"] / (stats["rxPackets"] - 1)
                ) * 1e3  # ms
                self._vprint(f"{'Jitter':<22}: {flow_kpis['Jitter'].measure:.2f} ms")
            else:
                flow_kpis["Jitter"].measure = 0
                self._vprint(f"{'Jitter':<22}: nan")
        else:
            flow_kpis["Latency"].measure = 0
            flow_kpis["Jitter"].measure = 0
            self._vprint(f"{'Latency':<22}: nan")
            self._vprint(f"{'Jitter':<22}: nan")
        self._vprint()

        return flow_kpis

    def _per_link(self, factory):
        return {LinkDirection.DOWNLINK: factory(), LinkDirection.UPLINK: factory()}

    def _vprint(self, *args, **kwargs) -> None:
        if self.verbose:
            print(*args, **kwargs)

    def _parse_flow(self, flow, classifiers) -> tuple[dict, int]:
        stats = {}
        flow_id = int(flow.get("flowId", 0))
        classifier_info = classifiers.get(flow_id, {})

        for b_key in _FLOW_KEYS["BitStats"]:
            stats[b_key] = int(flow.get(b_key, 0))
        for t_key in _FLOW_KEYS["TimeStats"]:
            stats[t_key] = self._parse_time(flow.get(t_key, "0ns"))
        for l_key in _FLOW_KEYS["LinkStats"]:
            stats[l_key] = classifier_info.get(l_key, "nan")
        return stats, flow_id

    @staticmethod
    def _get_flow_classifiers(root) -> dict[int, dict[str, str]]:
        classifiers = {}
        ipv4_classifier = root.find(".//Ipv4FlowClassifier")
        if ipv4_classifier is not None:
            for flow in ipv4_classifier.findall("Flow"):
                flow_id = int(flow.get("flowId", 0))
                classifiers[flow_id] = {
                    key: flow.get(key, "nan") for key in _FLOW_KEYS["LinkStats"]
                }
        return classifiers

    @staticmethod
    def _parse_time(time_string) -> float:
        if not time_string:
            return 0.0
        time_string = time_string.replace("+", "").strip()
        if time_string.endswith("ns"):
            return float(time_string[:-2]) / 1e9
        return 0.0


class DatasetPatternResolver:
    def __init__(self, dataset_id: str, data_format: str, datasets: dict):
        if dataset_id not in datasets:
            raise ValueError(
                f"\nib-ORAN Orchestrator: Unknown dataset id '{dataset_id}'."
            )
        self.dataset_id = dataset_id
        self.patterns = datasets[dataset_id]

        FILE_EXTENSION["IMAGE"] = self.patterns["input_image_format"]

        if data_format not in FILE_EXTENSION:
            raise ValueError(
                f"\nib-ORAN Orchestrator: Unknown data format '{data_format}'."
            )
        self.data_format = data_format

    def build_stem(self, kind: str, base_dir: Path, **values) -> Path:
        pattern = self.patterns[f"{kind}_pattern"]
        fields = self.patterns[f"{kind}_fields"]
        args = tuple(values[field] for field in fields)
        name = pattern % args
        return (base_dir / name).resolve()

    def resolve_input_file(self, base_dir: Path, **values) -> Path:
        """
        Resolve the actual input file on disk for any registered dataset.
        If the formatted pattern is fully computable (no wildcard), this
        behaves exactly like build_stem. If the pattern contains a literal
        '*' (e.g. Market-1501's arbitrary sequence/frame/box suffix), glob
        for matches and pick the first (deterministic given a sorted listing).
        """
        pattern = self.patterns["input_pattern"]
        fields = self.patterns["input_fields"]
        args = tuple(values[field] for field in fields)
        name = pattern % args
        extension = self.patterns["input_image_format"]

        if "*" in name:
            matches = sorted(base_dir.glob(name + extension))
            if not matches:
                raise FileNotFoundError(
                    f"No input file matching '{name}{extension}' in {base_dir}"
                )
            return matches[0]

        return (base_dir / name).with_suffix(extension).resolve()

    def compile_output_regex(self) -> re.Pattern:
        body_pattern = self._output_pattern_to_regex(self.patterns["output_pattern"])
        extension = re.escape(FILE_EXTENSION.get(self.data_format, ""))
        return re.compile(body_pattern + extension + "$")

    def find_object_id_for_ues(
        self,
        notebook_dir: Path,
        link_abbr: str,
        ue_num: int,
    ) -> dict[int, int]:
        idx_object_id_mapping: dict[int, int] = {}

        object_id_queries = FileStore(
            str(notebook_dir), "logs", "oidQueries.json"
        ).json_load()

        pattern = self.patterns["input_pattern"]
        fields = self.patterns["input_fields"]
        input_regex = self._input_pattern_to_regex(pattern, fields)

        for ue_idx in range(ue_num):
            filename = object_id_queries.get(link_abbr, {}).get(str(ue_idx + 1))
            if not filename:
                continue

            match = input_regex.match(filename)
            if not match:
                continue

            values = match.groupdict()
            object_id = int(values["object_id"])
            idx_object_id_mapping[ue_idx] = object_id

        return idx_object_id_mapping

    @staticmethod
    def _output_pattern_to_regex(pattern: str) -> str:
        regex_parts = []
        last_end = 0
        for m in _SPEC_RE.finditer(pattern):
            literal = pattern[last_end : m.start()]
            regex_parts.append(re.escape(literal))

            spec_type = m.group(2)
            if spec_type == "d":
                regex_parts.append(r"(\d+)")
            elif spec_type == "s":
                regex_parts.append(r"(\w+)")

            last_end = m.end()

        regex_parts.append(re.escape(pattern[last_end:]))
        return "".join(regex_parts)

    @staticmethod
    def _input_pattern_to_regex(pattern: str, fields: list[str]) -> re.Pattern:
        regex = ""
        field_iter = iter(fields)
        i = 0
        while i < len(pattern):
            c = pattern[i]
            if c == "%":
                j = i + 1
                width = ""
                while j < len(pattern) and pattern[j].isdigit():
                    width += pattern[j]
                    j += 1
                if j < len(pattern) and pattern[j] in ("d", "s"):
                    field = next(field_iter)
                    if pattern[j] == "d":
                        regex += (
                            f"(?P<{field}>\\d{{{width}}})"
                            if width
                            else f"(?P<{field}>\\d+)"
                        )
                    else:
                        regex += f"(?P<{field}>.+?)"
                    i = j + 1
                    continue
            elif c == "*":
                regex += ".*"
                i += 1
                continue
            regex += re.escape(c)
            i += 1
        return re.compile(regex)
