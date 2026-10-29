# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

import numpy as np
import pandas as pd
import subprocess
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
from IPython.display import display
from itertools import product
from tqdm.auto import tqdm
import tol_colors as tc
import shutil
import os
from pathlib import Path

from model.iboran_interfaces.iboran_shm_protocol import LinkDirection, RatType

from model.iboran_kpi.iboran_kpi_iqa import IbOranIQA
from model.iboran_kpi.iboran_kpi_ecs import IbOranECS
from model.iboran_campaign.iboran_orchestrator import _BASE_KPI_TEMPLATES, LINKS
from model.utils.file_store import FileStore

_FIDELITY_TEMPLATES_BY_FORMAT = {
    "IMAGE": IbOranIQA.METRIC_TEMPLATES,
    "VIDEO": None,
    "EMBEDDING": IbOranECS.METRIC_TEMPLATES,
    "TEXT": None,
}


class IbOranCampaign:
    def __init__(
        self,
        run_cwd,
        run_env,
        mc_iterations,
        dataset_id,
        data_format,
        embedder,
        oid_range,
        save_dapp_log,
        save_learning_convg,
        verbose,
        n_pretrain_runs=1,
    ):
        self.run_cwd = run_cwd
        self.run_env = run_env
        self.mc_iterations = mc_iterations
        self.dataset_id = dataset_id
        self.data_format = data_format
        self.embedder = embedder
        self.oid_range = oid_range
        self.save_dapp_log = save_dapp_log
        self.save_learning_convg = save_learning_convg
        self.verbose = verbose
        self.n_pretrain_runs = n_pretrain_runs

        self.results = []

    def run(
        self,
        dom_rat_type,
        dom_dapp_control_period_sf,
        dom_xapp_control_period_f,
        dom_k,
        dom_dapp_algo_ue_selection,
        dom_xapp_algo_rbg_bounding,
    ) -> pd.DataFrame:
        dom_combinations = list(
            product(
                dom_rat_type,
                dom_dapp_control_period_sf,
                dom_xapp_control_period_f,
                dom_k,
                dom_dapp_algo_ue_selection,
                dom_xapp_algo_rbg_bounding,
                range(self.mc_iterations),
            )
        )

        for (
            rat_type,
            dapp_control_period_sf,
            xapp_control_period_f,
            k,
            dapp_algo_ue_selection,
            xapp_algo_rbg_bounding,
            mc_idx,
        ) in tqdm(
            dom_combinations,
            desc="ib-ORAN Campaign",
            unit="run",
        ):
            records = self._run_single(
                rat_type=rat_type,
                dapp_control_period_sf=dapp_control_period_sf,
                xapp_control_period_f=xapp_control_period_f,
                k=k,
                dapp_algo_ue_selection=dapp_algo_ue_selection,
                xapp_algo_rbg_bounding=xapp_algo_rbg_bounding,
                mc_idx=mc_idx,
            )
            self.results.extend(records)

        # successive run() calls (e.g. one-factor sweeps sharing the base
        # configuration) must not count the same replication twice
        self.df_raw = pd.DataFrame(self.results).drop_duplicates(
            subset=[
                "rat_type",
                "dapp_control_period_sf",
                "xapp_control_period_f",
                "k",
                "dapp_algo_ue_selection",
                "xapp_algo_rbg_bounding",
                "mc_idx",
                "link",
            ],
            keep="last",
        )

        self.df = (
            self.df_raw.groupby(
                [
                    "rat_type",
                    "dapp_control_period_sf",
                    "xapp_control_period_f",
                    "k",
                    "dapp_algo_ue_selection",
                    "xapp_algo_rbg_bounding",
                    "link",
                ]
            )
            .mean(numeric_only=True)
            .reset_index()
        )

        self._setup_plot_params()

        return self.df

    def _run_single(
        self,
        rat_type,
        dapp_control_period_sf,
        xapp_control_period_f,
        k,
        dapp_algo_ue_selection,
        xapp_algo_rbg_bounding,
        mc_idx,
    ) -> list[dict]:
        records_store = FileStore(
            script_dir=".",
            folder_name="logs/run_data",
            log_name=f"{rat_type}{dapp_control_period_sf}{xapp_control_period_f}"
            f"{k}{dapp_algo_ue_selection}{xapp_algo_rbg_bounding}{mc_idx}"
            f"{self.dataset_id}{self.data_format}{self.embedder}"
            f"{''.join(map(str, self.oid_range))}eval.joblib",
        )
        if records_store.exists():
            saved_records = records_store.joblib_load()
            return saved_records

        uses_learning = any(
            "Learning" in name
            for name in (dapp_algo_ue_selection, xapp_algo_rbg_bounding)
        )
        if (
            uses_learning
            and not self._checkpoint_path(
                rat_type,
                dapp_algo_ue_selection,
                xapp_algo_rbg_bounding,
                dapp_control_period_sf,
                xapp_control_period_f,
                k,
            ).exists()
        ):
            self.pretrain(
                rat_type,
                dapp_control_period_sf,
                xapp_control_period_f,
                k,
                dapp_algo_ue_selection,
                xapp_algo_rbg_bounding,
                self.n_pretrain_runs,
            )

        self._cleanup()

        # a failed run must never be recorded with the KPIs of a previous run
        kpi_store = FileStore(".", "logs", "kpi.json")
        kpi_store.remove()

        proc = subprocess.run(
            [
                "python3",
                "iboran_run.py",
                f"--rat_type={rat_type}",
                f"--dapp_control_period_sf={dapp_control_period_sf}",
                f"--xapp_control_period_f={xapp_control_period_f}",
                f"--k={k}",
                f"--dapp_algo_ue_selection={dapp_algo_ue_selection}",
                f"--xapp_algo_rbg_bounding={xapp_algo_rbg_bounding}",
                f"--dataset_id={self.dataset_id}",
                f"--data_format={self.data_format}",
                f"--embedder={self.embedder}",
                f"--oid_range={','.join(map(str, self.oid_range))}",
                f"--save_dapp_log={'true' if self.save_dapp_log else 'false'}",
                f"--save_learning_convg={'true' if self.save_learning_convg else 'false'}",
                f"--verbose={'true' if self.verbose else 'false'}",
                "--learning_mode=eval",
                f"--rng_run={mc_idx + 1}",
            ],
            capture_output=not self.verbose,
            text=not self.verbose,
            cwd=self.run_cwd,
            env=self.run_env,
        )
        if proc.returncode != 0 or not kpi_store.exists():
            raise RuntimeError(
                f"ib-ORAN Campaign: run failed (rc={proc.returncode}) for "
                f"{rat_type}/{dapp_algo_ue_selection}/{xapp_algo_rbg_bounding} "
                f"sf={dapp_control_period_sf} f={xapp_control_period_f} k={k} "
                f"mc_idx={mc_idx}\n{(proc.stderr or '')[-2000:]}"
            )

        kpi = kpi_store.json_load()

        records = [
            {
                "rat_type": rat_type,
                "dapp_control_period_sf": dapp_control_period_sf,
                "xapp_control_period_f": xapp_control_period_f,
                "k": k,
                "dapp_algo_ue_selection": dapp_algo_ue_selection,
                "xapp_algo_rbg_bounding": xapp_algo_rbg_bounding,
                "mc_idx": mc_idx,
                "link": link,
                **{
                    key: value[str(link.value)]["measure"]
                    for key, value in kpi.items()
                    if "measure" not in value
                },
                **{
                    key: value["measure"]
                    for key, value in kpi.items()
                    if "measure" in value
                },
            }
            for link in LINKS
        ]

        records_store.joblib_dump(records)
        return records

    def pretrain(
        self, rat_type, dapp_period, xapp_period, k, dapp_algo, xapp_algo, n_runs
    ):
        "Train one pattern over n_runs sequential replications into its checkpoint."
        ckpt = self._checkpoint_path(
            rat_type, dapp_algo, xapp_algo, dapp_period, xapp_period, k
        )
        ckpt.unlink(missing_ok=True)

        for r in range(n_runs):
            proc = subprocess.run(
                [
                    "python3",
                    "iboran_run.py",
                    f"--rat_type={rat_type}",
                    f"--dapp_control_period_sf={dapp_period}",
                    f"--xapp_control_period_f={xapp_period}",
                    f"--k={k}",
                    f"--dapp_algo_ue_selection={dapp_algo}",
                    f"--xapp_algo_rbg_bounding={xapp_algo}",
                    f"--dataset_id={self.dataset_id}",
                    f"--data_format={self.data_format}",
                    f"--embedder={self.embedder}",
                    f"--oid_range={','.join(map(str, self.oid_range))}",
                    "--learning_mode=train",
                    f"--rng_run={10_000 + r}",  # disjoint from eval runs 1..mc
                    "--save_dapp_log=false",
                    f"--save_learning_convg={'true' if self.save_learning_convg else 'false'}",
                    "--verbose=false",
                ],
                capture_output=True,
                text=True,
                cwd=self.run_cwd,
                env=self.run_env,
            )
            if proc.returncode != 0:
                raise RuntimeError(
                    f"ib-ORAN Campaign: pretrain run {r} failed "
                    f"(rc={proc.returncode})\n{proc.stderr[-2000:]}"
                )

        if not ckpt.exists():
            raise RuntimeError(f"ib-ORAN Campaign: no checkpoint written at {ckpt}")

    @staticmethod
    def _checkpoint_path(
        rat_type, dapp_algo, xapp_algo, dapp_period, xapp_period, k
    ) -> Path:
        "Same file as IbOranOrchestrator._checkpoint_path (contrib/iboran/notebook)."
        return (
            Path(__file__).resolve().parents[2]
            / "notebook"
            / "logs"
            / "checkpoints"
            / f"{rat_type}_{dapp_algo}_{xapp_algo}_{dapp_period}_{xapp_period}_{k}.pt"
        )

    def plot_kpi_vs_param_per_algo(
        self,
        against: str,
        per: str,
        which_kpi: list[str],
        which_dapp_control_period_sf: int | list[int],
        which_xapp_control_period_f: int | list[int],
        which_k: int | list[int],
        which_dapp_algo_ue_selection: str | list[str],
        which_xapp_algo_rbg_bounding: str | list[str],
        which_rat: str | list[str] = [RatType.LTE, RatType.NR],
        append_title: str = "",
        save_dir: str | None = None,
    ) -> None:
        base_params = {
            "dapp_control_period_sf": which_dapp_control_period_sf,
            "xapp_control_period_f": which_xapp_control_period_f,
            "k": which_k,
        }
        base_algos = {
            "dapp_algo_ue_selection": which_dapp_algo_ue_selection,
            "xapp_algo_rbg_bounding": which_xapp_algo_rbg_bounding,
        }

        if against not in base_params.keys():
            raise ValueError(
                f"ib-ORAN Campaign: '{against}' is invalid. "
                f"Must be one of {base_params.keys()}."
            )
        if per not in base_algos.keys():
            raise ValueError(
                f"ib-ORAN Campaign: '{per}' is invalid. "
                f"Must be one of {base_algos.keys()}."
            )

        for col, val in base_params.items():
            if col == against:
                if not isinstance(val, list) or not all(
                    isinstance(v, int) for v in val
                ):
                    raise ValueError(
                        f"ib-ORAN Campaign: '{against}' must be a list of ints as the swept param."
                    )
            else:
                if not isinstance(val, int):
                    raise ValueError(
                        f"ib-ORAN Campaign: '{col}' must be a fixed int when not the swept param."
                    )

        # define base filter
        not_per = next(k for k in base_algos.keys() if k != per)
        not_per_val = base_algos[not_per]
        base_mask = (
            self.df[not_per].isin(not_per_val)
            if isinstance(not_per_val, list)
            else self.df[not_per] == not_per_val
        )
        for col, val in base_params.items():
            if isinstance(val, list):
                base_mask &= self.df[col].isin(val)
            else:
                base_mask &= self.df[col] == val

        # main plot loop
        n_kpis = len(which_kpi)
        for rat in which_rat if isinstance(which_rat, list) else [which_rat]:
            fig, axes = plt.subplots(nrows=2, ncols=n_kpis, figsize=(5 * n_kpis, 10))
            if n_kpis == 1:
                axes = axes.reshape(2, 1)

            fig.suptitle(
                f"{rat} RAT {append_title}",
                fontsize=self.big_fontsize,
                fontweight="bold",
            )
            for row_idx, link in enumerate(LINKS):
                mask = (
                    base_mask & (self.df["rat_type"] == rat) & (self.df["link"] == link)
                )
                depict_df = self.df[mask]

                if depict_df.empty:
                    for col_idx in range(n_kpis):
                        axes[row_idx, col_idx].set_visible(False)
                    continue

                algos = depict_df[per].unique()
                for col_idx, kpi in enumerate(which_kpi):
                    ax = axes[row_idx, col_idx]

                    if kpi not in depict_df.columns:
                        raise ValueError(
                            f"ib-ORAN Campaign: KPI '{kpi}' not found in dataframe columns."
                        )

                    algos_sorted = sorted(
                        algos,
                        key=lambda algo: depict_df[depict_df[per] == algo][kpi].mean(),
                        reverse=(
                            True if self.kpi_kwds[kpi]["sort"] == "desc" else False
                        ),
                    )

                    def plot_fn(
                        ax, _algos_sorted=algos_sorted, _kpi=kpi, _depict_df=depict_df
                    ):
                        for algo in _algos_sorted:
                            df_algo = _depict_df[_depict_df[per] == algo].sort_values(
                                against
                            )
                            ax.plot(
                                df_algo[against],
                                df_algo[_kpi],
                                marker=("o" if algo.startswith("ib") else "s"),
                                color=self.algo_kwds[per][algo]["color"],
                                label=self.algo_kwds[per][algo]["label"],
                            )
                        ax.set_ylabel(
                            f'{self.kpi_kwds[_kpi]["label"]} {self.kpi_kwds[_kpi]["unit"]}',
                            fontsize=self.small_fontsize,
                        )
                        ax.set_xlabel(
                            self.param_kwds[against]["label"],
                            fontsize=self.small_fontsize,
                        )
                        ax.xaxis.set_major_locator(MaxNLocator(integer=True))
                        ax.legend(fontsize=self.small_fontsize)

                    plot_fn(ax)

                    if col_idx == 0:
                        ax.text(
                            -0.25,
                            0.5,
                            link,
                            transform=ax.transAxes,
                            fontsize=self.big_fontsize,
                            fontweight="bold",
                            va="center",
                            ha="center",
                            rotation=90,
                        )

                    if save_dir is not None:
                        os.makedirs(save_dir, exist_ok=True)
                        self._save_subplot(
                            file_name=os.path.join(save_dir, f"{rat}_{link}_{kpi}.png"),
                            plot_fn=plot_fn,
                            title=f"{link}",
                        )

            plt.tight_layout()
            plt.show()

    def plot_bar_kpi_per_algo(
        self,
        per: str,
        which_kpi: list[str],
        which_dapp_algo_ue_selection: str | list[str],
        which_xapp_algo_rbg_bounding: str | list[str],
        which_rat: str | list[str] = [RatType.LTE, RatType.NR],
        append_title: str = "",
        save_dir: str | None = None,
    ) -> None:
        base_algos = {
            "dapp_algo_ue_selection": which_dapp_algo_ue_selection,
            "xapp_algo_rbg_bounding": which_xapp_algo_rbg_bounding,
        }

        if per not in base_algos.keys():
            raise ValueError(
                f"ib-ORAN Campaign: '{per}' is invalid. "
                f"Must be one of {base_algos.keys()}."
            )

        not_per = next(k for k in base_algos.keys() if k != per)
        not_per_val = base_algos[not_per]
        base_mask = (
            self.df[not_per].isin(not_per_val)
            if isinstance(not_per_val, list)
            else self.df[not_per] == not_per_val
        )

        n_kpis = len(which_kpi)
        for rat in which_rat if isinstance(which_rat, list) else [which_rat]:
            fig, axes = plt.subplots(nrows=2, ncols=n_kpis, figsize=(5 * n_kpis, 10))
            if n_kpis == 1:
                axes = axes.reshape(2, 1)

            fig.suptitle(
                f"{rat} RAT {append_title}",
                fontsize=self.big_fontsize,
                fontweight="bold",
            )

            # Collect per-link data for the side-by-side save
            link_data: dict[str, dict] = (
                {}
            )  # link -> {kpi -> (depict_df, algos_sorted)}

            for row_idx, link in enumerate(LINKS):
                mask = (
                    base_mask & (self.df["rat_type"] == rat) & (self.df["link"] == link)
                )
                depict_df = self.df[mask]

                if depict_df.empty:
                    for col_idx in range(n_kpis):
                        axes[row_idx, col_idx].set_visible(False)
                    continue

                algos = depict_df[per].unique()
                link_data[link] = {}

                for col_idx, kpi in enumerate(which_kpi):
                    ax = axes[row_idx, col_idx]

                    if kpi not in depict_df.columns:
                        raise ValueError(
                            f"ib-ORAN Campaign: KPI '{kpi}' not found in dataframe columns."
                        )

                    algos_sorted = sorted(
                        algos,
                        key=lambda algo: depict_df[depict_df[per] == algo][kpi].mean(),
                        reverse=True,
                    )
                    link_data[link][kpi] = (depict_df, algos_sorted)

                    self._draw_bar_kpi(ax, depict_df, per, kpi, algos_sorted)

                    if col_idx == 0:
                        ax.text(
                            -0.25,
                            0.5,
                            link,
                            transform=ax.transAxes,
                            fontsize=self.big_fontsize,
                            fontweight="bold",
                            va="center",
                            ha="center",
                            rotation=90,
                        )

            # --- side-by-side save ---
            if save_dir is not None and all(lnk in link_data for lnk in LINKS):
                os.makedirs(save_dir, exist_ok=True)
                subplot_size = (5, 5)  # (w, h) per subplot — keeps both identical

                for kpi in which_kpi:
                    if not all(kpi in link_data[lnk] for lnk in LINKS):
                        continue

                    fig_side, (ax_dl, ax_ul) = plt.subplots(
                        nrows=1,
                        ncols=2,
                        figsize=(subplot_size[0] * 2, subplot_size[1]),
                    )

                    dl_df, dl_algos = link_data[LinkDirection.DOWNLINK][kpi]
                    ul_df, ul_algos = link_data[LinkDirection.UPLINK][kpi]

                    self._draw_bar_kpi(ax_dl, dl_df, per, kpi, dl_algos)
                    self._draw_bar_kpi(ax_ul, ul_df, per, kpi, ul_algos)

                    ax_dl.set_title(
                        "Downlink", fontsize=self.big_fontsize, fontweight="bold"
                    )
                    ax_ul.set_title(
                        "Uplink", fontsize=self.big_fontsize, fontweight="bold"
                    )
                    ax_ul.set_ylabel("")  # remove y-label from right subplot

                    fig_side.savefig(
                        os.path.join(save_dir, f"{rat}_{kpi}_bar.png"),
                        dpi=150,
                        bbox_inches="tight",
                    )
                    plt.close(fig_side)

            # --- averaged-over-links save ---
            if save_dir is not None:
                os.makedirs(save_dir, exist_ok=True)
                rat_mask = base_mask & (self.df["rat_type"] == rat)
                full_df = self.df[rat_mask]

                if not full_df.empty:
                    fidelity_templates = (
                        _FIDELITY_TEMPLATES_BY_FORMAT.get(self.data_format) or {}
                    )
                    fidelity_cols = list(fidelity_templates.keys())

                    cols_to_avg = list(
                        dict.fromkeys(
                            which_kpi
                            + [c for c in fidelity_cols if c in full_df.columns]
                        )
                    )

                    avg_df = full_df.groupby(per)[cols_to_avg].mean().reset_index()

                    for kpi in which_kpi:
                        if kpi not in avg_df.columns:
                            continue

                        algos_sorted_avg = sorted(
                            avg_df[per].unique(),
                            key=lambda algo: avg_df[avg_df[per] == algo][kpi].mean(),
                            reverse=True,
                        )

                        self._save_subplot(
                            file_name=os.path.join(
                                save_dir, f"{rat}_AVERAGE_{kpi}_bar.png"
                            ),
                            plot_fn=lambda ax, _df=avg_df, _algos=algos_sorted_avg: self._draw_bar_kpi(
                                ax, _df, per, kpi, _algos
                            ),
                            title=None,
                        )

            plt.tight_layout()
            plt.show()

    def plot_table_kpi_per_algo(
        self,
        per: str,
        which_kpi: list[str],
        which_dapp_algo_ue_selection: str | list[str],
        which_xapp_algo_rbg_bounding: str | list[str],
        which_rat: str | list[str] = [RatType.LTE, RatType.NR],
        append_title: str = "",
        decimals: int = 3,
    ) -> None:
        base_algos = {
            "dapp_algo_ue_selection": which_dapp_algo_ue_selection,
            "xapp_algo_rbg_bounding": which_xapp_algo_rbg_bounding,
        }
        if per not in base_algos.keys():
            raise ValueError(
                f"ib-ORAN Campaign: '{per}' is invalid. "
                f"Must be one of {base_algos.keys()}."
            )

        not_per = next(k for k in base_algos if k != per)
        not_per_val = base_algos[not_per]
        base_mask = (
            self.df[not_per].isin(not_per_val)
            if isinstance(not_per_val, list)
            else self.df[not_per] == not_per_val
        )

        for rat in which_rat if isinstance(which_rat, list) else [which_rat]:
            rat_mask = base_mask & (self.df["rat_type"] == rat)
            if self.df[rat_mask].empty:
                continue

            stats = self._compute_kpi_stats(rat_mask, per, which_kpi)
            algos = sorted(self.df[rat_mask][per].unique())

            print(f"-- {rat} RAT {append_title} --")

            for algo in algos:
                if algo not in stats.index.get_level_values("algo"):
                    continue

                df_algo = stats.xs(algo, level="algo")
                df_algo.index.name = None
                df_algo.columns = pd.MultiIndex.from_tuples(
                    [
                        (
                            f'{self.kpi_kwds[kpi]["label"]} {self.kpi_kwds[kpi]["unit"]}',
                            stat,
                        )
                        for kpi, stat in df_algo.columns
                    ],
                    names=["kpi", "stat"],
                )
                df_algo.columns.names = ["KPI", "Statistics"]
                df_algo = df_algo.reindex(LINKS)

                def style_table(styler: "pd.Styler") -> "pd.Styler":
                    fmt = {col: f"{{:.{decimals}f}}" for col in df_algo.columns}
                    styler.format(fmt, na_rep="—")

                    styler.set_table_styles(
                        [
                            {
                                "selector": "table",
                                "props": "background-color: #ffffff; color: #111111; border-collapse: collapse;",
                            },
                            {
                                "selector": "th",
                                "props": (
                                    "background-color: #f0f0f0; color: #111111;"
                                    "text-align: center; padding: 6px 10px;"
                                    "border: 1px solid #cccccc; font-weight: 600;"
                                ),
                            },
                            {
                                "selector": "td",
                                "props": (
                                    "background-color: #ffffff; color: #111111;"
                                    "text-align: right; padding: 5px 10px;"
                                    "border: 1px solid #dddddd; font-family: monospace;"
                                ),
                            },
                            {
                                "selector": "tr:nth-child(even) td",
                                "props": "background-color: #f7f7f7;",
                            },
                            {
                                "selector": "caption",
                                "props": (
                                    "background-color: #ffffff; color: #333333;"
                                    "font-size: 1rem; font-weight: 600;"
                                    "padding: 6px; text-align: left;"
                                ),
                            },
                        ],
                        overwrite=False,
                    )

                    styler.apply(
                        lambda col: [
                            (
                                "color: #777777; font-size: 0.9em;"
                                if col.name[1] == "std"
                                else "color: #111111;"
                            )
                            for _ in col
                        ],
                        axis=0,
                    )

                    mean_cols = [
                        (kpi, "mean") for kpi, stat in df_algo.columns if stat == "mean"
                    ]
                    for col in mean_cols:
                        styler.bar(subset=[col], vmin=0, color="#c8d8e8")

                    styler.set_caption(f"Algo — {self._algo_label(per, algo)}")
                    return styler

                styled = df_algo.style.pipe(style_table)
                display(styled)

    def get_table_kpi_per_algo(
        self,
        per: str,
        which_kpi: list[str],
        which_dapp_algo_ue_selection: str | list[str],
        which_xapp_algo_rbg_bounding: str | list[str],
        which_rat: str | list[str] = [RatType.LTE, RatType.NR],
    ) -> dict:
        """
        Same values as plot_table_kpi_per_algo, returned as a dict.

        Returns
        -------
        results[rat][algo][link][kpi] = {"mean": float, "std": float}
        """
        base_algos = {
            "dapp_algo_ue_selection": which_dapp_algo_ue_selection,
            "xapp_algo_rbg_bounding": which_xapp_algo_rbg_bounding,
        }
        if per not in base_algos:
            raise ValueError(
                f"ib-ORAN Campaign: '{per}' is invalid. "
                f"Must be one of {base_algos.keys()}."
            )

        not_per = next(k for k in base_algos if k != per)
        not_per_val = base_algos[not_per]
        base_mask = (
            self.df[not_per].isin(not_per_val)
            if isinstance(not_per_val, list)
            else self.df[not_per] == not_per_val
        )

        results: dict = {}
        for rat in which_rat if isinstance(which_rat, list) else [which_rat]:
            rat_mask = base_mask & (self.df["rat_type"] == rat)
            if self.df[rat_mask].empty:
                continue

            stats = self._compute_kpi_stats(rat_mask, per, which_kpi)
            available = set(stats.index.get_level_values("algo"))
            rat_results = results.setdefault(rat, {})

            for algo in sorted(self.df[rat_mask][per].unique()):
                if algo not in available:
                    continue
                df_algo = stats.xs(algo, level="algo").reindex(LINKS)
                rat_results[algo] = {
                    link: {
                        kpi: {
                            "mean": float(df_algo.at[link, (kpi, "mean")]),
                            "std": float(df_algo.at[link, (kpi, "std")]),
                        }
                        for kpi in which_kpi
                    }
                    for link in LINKS
                }

        return results

    def get_kpi_stats_at_config(
        self,
        which_kpi: list[str],
        dapp_algo: str,
        xapp_algo: str,
        rat: str,
        dapp_control_period_sf: int,
        xapp_control_period_f: int,
        k: int,
    ) -> dict:
        """
        Mean/std over Monte Carlo iterations at one fixed configuration.

        Returns
        -------
        results[link][kpi] = {"mean": float, "std": float, "n": int}
        """
        d = self.df_raw
        mask = (
            (d["rat_type"] == rat)
            & (d["dapp_algo_ue_selection"] == dapp_algo)
            & (d["xapp_algo_rbg_bounding"] == xapp_algo)
            & (d["dapp_control_period_sf"] == dapp_control_period_sf)
            & (d["xapp_control_period_f"] == xapp_control_period_f)
            & (d["k"] == k)
        )
        sub = d[mask]
        results = {}
        for link in LINKS:
            s = sub[sub["link"] == link]
            results[link] = {
                kpi: {
                    "mean": float(s[kpi].mean()) if kpi in s else np.nan,
                    "std": float(s[kpi].std()) if kpi in s else np.nan,
                    "n": int(s[kpi].notna().sum()) if kpi in s else 0,
                }
                for kpi in which_kpi
            }
        return results

    def plot_table_gain_loss_kpi_per_algo(
        self,
        per: str,
        which_kpi: list[str],
        which_dapp_algo_ue_selection: str | list[str],
        which_xapp_algo_rbg_bounding: str | list[str],
        ref_algo: str,
        decimals: int = 2,
        which_rat: str | list[str] = [RatType.LTE, RatType.NR],
        append_title: str = "",
    ) -> None:
        base_algos = {
            "dapp_algo_ue_selection": which_dapp_algo_ue_selection,
            "xapp_algo_rbg_bounding": which_xapp_algo_rbg_bounding,
        }
        if per not in base_algos:
            raise ValueError(
                f"'{per}' invalid. Must be one of {list(base_algos.keys())}."
            )

        not_per = next(k for k in base_algos if k != per)
        not_per_val = base_algos[not_per]
        base_mask = (
            self.df[not_per].isin(not_per_val)
            if isinstance(not_per_val, list)
            else self.df[not_per] == not_per_val
        )

        for rat in which_rat if isinstance(which_rat, list) else [which_rat]:
            rat_mask = base_mask & (self.df["rat_type"] == rat)
            if self.df[rat_mask].empty:
                continue

            stats = self._compute_kpi_stats(rat_mask, per, which_kpi)
            all_algos = sorted(self.df[rat_mask][per].unique())
            compare_algos = [a for a in all_algos if a != ref_algo]

            if not compare_algos:
                print(
                    f"[{rat}] Nothing to compare: ref_algo '{ref_algo}' is the only algo."
                )
                continue

            gain_df = self._compute_gain_loss(
                stats, per, ref_algo, which_kpi, compare_algos
            )

            gain_df.columns.names = ["KPI", "Algorithm"]
            gain_df.index.name = None
            gain_df = gain_df.reindex(LINKS)

            gain_df.columns = pd.MultiIndex.from_tuples(
                [
                    (
                        f'{self.kpi_kwds[kpi]["label"]} {self.kpi_kwds[kpi]["unit"]}',
                        algo,
                    )
                    for kpi, algo in gain_df.columns
                ],
                names=["KPI", "Algorithm"],
            )

            gain_df = gain_df.reindex(
                columns=pd.MultiIndex.from_tuples(
                    [
                        (
                            f'{self.kpi_kwds[kpi]["label"]} {self.kpi_kwds[kpi]["unit"]}',
                            self._algo_label(per, a),
                        )
                        for kpi in which_kpi
                        for a in compare_algos
                        if (
                            f'{self.kpi_kwds[kpi]["label"]} {self.kpi_kwds[kpi]["unit"]}',
                            self._algo_label(per, a),
                        )
                        in gain_df.columns
                    ],
                    names=["KPI", "Algorithm"],
                )
            )

            def style_gain_loss(styler: "pd.Styler") -> "pd.Styler":
                styler.format(
                    lambda v: f"{v:+.{decimals}f} %" if not pd.isna(v) else "—"
                )

                def color_gain_loss(v):
                    if pd.isna(v):
                        return ""
                    if v > 0:
                        return "background-color: #d4edda; color: #111111;"
                    if v < 0:
                        return "background-color: #f8d7da; color: #111111;"
                    return "background-color: #e0e0e0; color: #111111;"

                styler.apply(lambda df: df.map(color_gain_loss), axis=None)
                styler.format(
                    lambda v: f"{v:+.{decimals}f} %" if not pd.isna(v) else "—"
                )

                styler.set_table_styles(
                    [
                        {
                            "selector": "table",
                            "props": "background-color: #ffffff; border-collapse: collapse;",
                        },
                        {
                            "selector": "th",
                            "props": (
                                "background-color: #f0f0f0; color: #111111;"
                                "text-align: center; padding: 6px 10px;"
                                "border: 1px solid #cccccc; font-weight: 600;"
                            ),
                        },
                        {
                            "selector": "td",
                            "props": (
                                "color: #111111;"
                                "text-align: right; padding: 5px 10px;"
                                "border: 1px solid #dddddd; font-family: monospace; font-weight: 500;"
                            ),
                        },
                        {
                            "selector": "caption",
                            "props": (
                                "background-color: #ffffff; color: #333333;"
                                "font-size: 1rem; font-weight: 600;"
                                "padding: 6px; text-align: left;"
                            ),
                        },
                    ],
                    overwrite=False,
                )

                ref_label = self._algo_label(per, ref_algo)
                styler.set_caption(f"Ref. Algo — {ref_label}")

                # Underline best gain per KPI
                for kpi_label in gain_df.columns.get_level_values("KPI").unique():
                    kpi_cols = [col for col in gain_df.columns if col[0] == kpi_label]
                    if kpi_cols:
                        styler.highlight_max(
                            subset=kpi_cols,
                            axis=1,
                            props="font-weight: 900; text-decoration: underline;",
                        )

                return styler

            styled = gain_df.style.pipe(style_gain_loss)
            print(f"-- {rat} RAT {append_title} --")
            display(styled)

    def _draw_bar_kpi(self, ax, depict_df, per, kpi, algos_sorted) -> None:
        algos_pos = range(len(algos_sorted))
        for pos_idx, algo in zip(algos_pos, algos_sorted):
            df_algo = depict_df[depict_df[per] == algo]
            edgecolor = "white" if algo.startswith("ib") else "black"
            if not kpi == "SatisfactionScore":
                ax.bar(
                    pos_idx,
                    df_algo[kpi].mean(),
                    yerr=[
                        [np.abs(df_algo[kpi].mean() - np.min(df_algo[kpi]))],
                        [df_algo[kpi].std()],
                    ],
                    color=self.algo_kwds[per][algo]["color"],
                    label=self.algo_kwds[per][algo]["label"],
                    edgecolor=edgecolor,
                    linewidth=0.5,
                    hatch="\\" if algo.startswith("ib") else "",
                )
                kpi_y_label = (
                    f'{self.kpi_kwds[kpi]["label"]} {self.kpi_kwds[kpi]["unit"]}'
                )
            else:
                components = [
                    (kpi, self.kpi_kwds[kpi]["label"], ""),
                    ("FidelityScore", "Fidelity Score", "//"),
                ]
                n_components = len(components)
                bar_gap = 0.1
                bar_width = 0.8 / n_components
                drawn_width = bar_width * (1 - bar_gap)

                for comp_idx, (col, comp_label, hatch) in enumerate(components):
                    offset = (comp_idx - (n_components - 1) / 2) * bar_width
                    ax.bar(
                        pos_idx + offset,
                        df_algo[col].mean(),
                        width=drawn_width,
                        alpha=0.8,
                        color=self.algo_kwds[per][algo]["color"],
                        capsize=5,
                        label=comp_label,
                        edgecolor=edgecolor,
                        linewidth=1.5,
                        hatch=hatch,
                    )

                handles, legend_labels = ax.get_legend_handles_labels()
                kpi_y_label = "Intent-Based KPIs"
                ax.legend(
                    handles[:n_components],
                    legend_labels[:n_components],
                    fontsize=self.small_fontsize,
                    loc="best",
                )

        ax.set_ylabel(
            kpi_y_label,
            fontsize=self.small_fontsize,
        )
        ax.set_xticks(list(algos_pos))
        labels = [self.algo_kwds[per][algo]["label"] for algo in algos_sorted]
        ax.set_xticklabels(
            labels, rotation=45, ha="right", fontsize=self.small_fontsize
        )

    def _algo_label(self, per: str, algo: str) -> str:
        return self.algo_kwds[per][algo]["label"]

    def _compute_kpi_stats(
        self,
        rat_mask: pd.Series,
        per: str,
        which_kpi: list[str],
    ) -> pd.DataFrame:
        sub = self.df[rat_mask].copy()
        records = []
        for algo in sub[per].unique():
            for link in sub["link"].unique():
                mask = (sub[per] == algo) & (sub["link"] == link)
                slice_ = sub[mask]
                row = {"algo": algo, "link": link}
                for kpi in which_kpi:
                    if kpi in slice_.columns and not slice_.empty:
                        row[(kpi, "mean")] = slice_[kpi].mean()
                        row[(kpi, "std")] = slice_[kpi].std()
                    else:
                        row[(kpi, "mean")] = np.nan
                        row[(kpi, "std")] = np.nan
                records.append(row)

        df = pd.DataFrame(records).set_index(["algo", "link"])
        df.columns = pd.MultiIndex.from_tuples(df.columns, names=["kpi", "stat"])
        return df.sort_index()

    def _compute_gain_loss(
        self,
        stats: pd.DataFrame,
        per: str,
        ref_algo: str,
        which_kpi: list[str],
        compare_algos: list[str],
    ) -> pd.DataFrame:
        links = stats.index.get_level_values("link").unique()
        records = []
        for link in links:
            row = {"link": link}
            ref_vals = {
                kpi: (
                    stats.loc[(ref_algo, link), (kpi, "mean")]
                    if (ref_algo, link) in stats.index
                    else np.nan
                )
                for kpi in which_kpi
            }
            for algo in compare_algos:
                for kpi in which_kpi:
                    ref = ref_vals[kpi]
                    val = (
                        stats.loc[(algo, link), (kpi, "mean")]
                        if (algo, link) in stats.index
                        else np.nan
                    )
                    delta = (
                        (val - ref) / abs(ref) * 100
                        if (not np.isnan(ref) and not np.isnan(val) and ref != 0)
                        else np.nan
                    )
                    row[(kpi, self._algo_label(per, algo))] = delta
            records.append(row)

        df = pd.DataFrame(records).set_index("link")
        df.columns = pd.MultiIndex.from_tuples(df.columns, names=["kpi", "algo"])
        return df.sort_index()

    def _setup_plot_params(self) -> None:
        fidelity_templates = _FIDELITY_TEMPLATES_BY_FORMAT.get(self.data_format) or {}
        all_templates = {**_BASE_KPI_TEMPLATES, **fidelity_templates}
        self.kpi_kwds = {
            key: {"label": kpi.label, "unit": kpi.unit, "sort": kpi.sort}
            for key, kpi in all_templates.items()
        }
        self.param_kwds = {
            "rat_type": {"label": "RAT Type"},
            "dapp_control_period_sf": {"label": "dApp Control Period [subframe]"},
            "xapp_control_period_f": {"label": "xApp Control Period [frame]"},
            "k": {"label": "Maximum parallel UEs per TTI/slot"},
            "dapp_algo_ue_selection": {"label": "dApp UE Selection Algorithm"},
            "xapp_algo_rbg_bounding": {"label": "xApp RBG bounding Algorithm"},
        }
        self.algo_kwds = {
            "dapp_algo_ue_selection": {
                "ibRandom": {"color": tc.vibrant[6], "label": "ib Random"},
                "Random": {"color": tc.vibrant[6], "label": "Random"},
                "ibRoundRobin": {"color": tc.vibrant[5], "label": "ib Round Robin"},
                "RoundRobin": {"color": tc.vibrant[5], "label": "Round Robin"},
                "ibLearningNeuralLinUCB": {
                    "color": tc.high_contrast[2],
                    "label": "ib NeuralLinUCB",
                },
                "LearningNeuralLinUCB": {
                    "color": tc.high_contrast[2],
                    "label": "NeuralLinUCB",
                },
                "ibTopKCqi": {"color": tc.light[0], "label": "ib Top-K: CQI"},
                "ibTopKBufferLevel": {
                    "color": tc.light[1],
                    "label": "ib Top-K: Buffer Level",
                },
                "ibTopKPropFairness": {
                    "color": tc.light[2],
                    "label": "ib Top-K: Prop. Fairn.",
                },
                "ibTopKCriticality": {
                    "color": tc.light[3],
                    "label": "ib Top-K: Criticality",
                },
                "ibTopKMultiCriteria": {
                    "color": tc.light[4],
                    "label": "ib Top-K: Multi-Criteria",
                },
                "TopKCqi": {"color": tc.light[0], "label": "Top-K: CQI"},
                "TopKBufferLevel": {
                    "color": tc.light[1],
                    "label": "Top-K: Buffer Level",
                },
                "TopKPropFairness": {
                    "color": tc.light[2],
                    "label": "Top-K: Prop. Fairn.",
                },
                "TopKCriticality": {
                    "color": tc.light[3],
                    "label": "Top-K: Criticality",
                },
                "TopKMultiCriteria": {
                    "color": tc.light[4],
                    "label": "Top-K: Multi-Criteria",
                },
                "ibGreedyCqi": {"color": tc.vibrant[0], "label": "ib Greedy: CQI"},
                "ibGreedyBufferLevel": {
                    "color": tc.vibrant[1],
                    "label": "ib Greedy: Buffer Level",
                },
                "ibGreedyPropFairness": {
                    "color": tc.vibrant[2],
                    "label": "ib Greedy: Prop. Fairn.",
                },
                "ibGreedyCriticality": {
                    "color": tc.vibrant[3],
                    "label": "ib Greedy: Criticality",
                },
                "ibGreedyMultiCriteria": {
                    "color": tc.vibrant[4],
                    "label": "ib Greedy: Multi-Criteria",
                },
                "GreedyCqi": {"color": tc.vibrant[0], "label": "Greedy: CQI"},
                "GreedyBufferLevel": {
                    "color": tc.vibrant[1],
                    "label": "Greedy: Buffer Level",
                },
                "GreedyPropFairness": {
                    "color": tc.vibrant[2],
                    "label": "Greedy: Prop. Fairn.",
                },
                "GreedyCriticality": {
                    "color": tc.vibrant[3],
                    "label": "Greedy: Criticality",
                },
                "GreedyMultiCriteria": {
                    "color": tc.vibrant[4],
                    "label": "Greedy: Multi-Criteria",
                },
                "ibDpCqi": {"color": tc.dark[0], "label": "ib DP: CQI"},
                "ibDpBufferLevel": {
                    "color": tc.dark[1],
                    "label": "ib DP: Buffer Level",
                },
                "ibDpPropFairness": {
                    "color": tc.dark[2],
                    "label": "ib DP: Prop. Fairn.",
                },
                "ibDpCriticality": {"color": tc.dark[3], "label": "ib DP: Criticality"},
                "ibDpMultiCriteria": {
                    "color": tc.dark[4],
                    "label": "ib DP: Multi-Criteria",
                },
                "DpCqi": {"color": tc.dark[0], "label": "DP: CQI"},
                "DpBufferLevel": {"color": tc.dark[1], "label": "DP: Buffer Level"},
                "DpPropFairness": {"color": tc.dark[2], "label": "DP: Prop. Fairn."},
                "DpCriticality": {"color": tc.dark[3], "label": "DP: Criticality"},
                "DpMultiCriteria": {"color": tc.dark[4], "label": "DP: Multi-Criteria"},
            },
            "xapp_algo_rbg_bounding": {
                "Bypass": {"color": tc.muted[0], "label": "Bypass"},
                "Clamp": {"color": tc.muted[2], "label": "Clamp"},
                "ibLearningPPO": {"color": tc.muted[7], "label": "ib PPO"},
                "LearningPPO": {"color": tc.muted[7], "label": "PPO"},
            },
        }
        self.big_fontsize = 15
        self.small_fontsize = 13

    def _save_subplot(self, file_name, plot_fn, title=None, figsize=(5, 5)) -> None:
        fig_single, ax_single = plt.subplots(figsize=figsize)
        plot_fn(ax_single)
        if title:
            ax_single.set_title(title, fontsize=self.big_fontsize, fontweight="bold")
        fig_single.savefig(file_name, dpi=150, bbox_inches="tight")
        plt.close(fig_single)

    def _cleanup(self) -> None:
        paths = {"dataset_dscv": os.path.join(".", "dataset_dscv")}
        for name, path in paths.items():
            if os.path.exists(path):
                try:
                    shutil.rmtree(path)
                except PermissionError as e:
                    raise PermissionError(
                        f"ib-ORAN Orchestrator: Could not remove {name} folder — "
                        f"file may be locked or permissions insufficient.\n{e}"
                    )
