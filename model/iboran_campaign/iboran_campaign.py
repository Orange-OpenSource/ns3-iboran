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
from IPython.display import display
from itertools import product
from tqdm import tqdm
import tol_colors as tc
import shutil
import os

from model.iboran_campaign.iboran_orchestrator import IbOranOrchestrator
from model.iboran_interfaces.iboran_shm_structure import *
from model.utils.do_log import DoLog

LINKS = ["UPLINK", "DOWNLINK"]


class IbOranCampaign:
    def __init__(
        self,
        run_cwd,
        run_env,
        mc_iterations=1,
        show_flowstats="false",
        show_iqa_diagnosis="false",
        save_dapp_log="false",
    ):
        self.run_cwd = run_cwd
        self.run_env = run_env
        self.mc_iterations = mc_iterations
        self.show_flowstats = show_flowstats
        self.show_iqa_diagnosis = show_iqa_diagnosis
        self.save_dapp_log = save_dapp_log

        self.orchestrator = IbOranOrchestrator()
        self._reset_variables()
        self._setup_plot_params()

    def _reset_variables(self):
        self.results = []

    def run(
        self,
        dom_rat_type,
        dom_dapp_control_period_sf,
        dom_xapp_control_period_f,
        dom_k,
        dom_dapp_algo_ue_selection,
        dom_xapp_algo_rbg_bounding,
    ):
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

        self.df_raw = pd.DataFrame(self.results)

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
    ):
        records_logger = DoLog(
            script_dir=".",
            folder_name="logs/run_logs",
            log_name=f"{rat_type}{dapp_control_period_sf}{xapp_control_period_f}"
            f"{k}{dapp_algo_ue_selection}{xapp_algo_rbg_bounding}{mc_idx}.joblib",
        )
        if records_logger.exists():
            saved_records = records_logger.joblib_load()
            return saved_records

        self._cleanup()

        subprocess.run(
            [
                "python3",
                "iboran_run.py",
                f"--rat_type={rat_type}",
                f"--dapp_control_period_sf={dapp_control_period_sf}",
                f"--xapp_control_period_f={xapp_control_period_f}",
                f"--k={k}",
                f"--dapp_algo_ue_selection={dapp_algo_ue_selection}",
                f"--xapp_algo_rbg_bounding={xapp_algo_rbg_bounding}",
                f"--show_flowstats={self.show_flowstats}",
                f"--show_iqa_diagnosis={self.show_iqa_diagnosis}",
                f"--save_dapp_log={self.save_dapp_log}",
            ],
            capture_output=True,
            text=True,
            cwd=self.run_cwd,
            env=self.run_env,
        )

        kpi = DoLog(".", "logs", "kpi.json").json_load()

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
                    key: kpi[key][link]
                    for key in self.orchestrator.kpi.keys()
                    if isinstance(kpi[key], dict)
                },
                **{
                    key: kpi[key]
                    for key in self.orchestrator.kpi.keys()
                    if not isinstance(kpi[key], dict)
                },
            }
            for link in LINKS
        ]

        records_logger.joblib_dump(records)
        return records

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
        which_rat: str | list[str] = [LTE, NR],
        append_title: str = "",
        save_dir: str | None = None,
    ):
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
        base_mask = self.df[not_per] == base_algos[not_per]
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
                                marker=("o" if algo.startswith("iB") else "s"),
                                color=self.algo_kwds[algo]["color"],
                                label=self.algo_kwds[algo]["label"],
                            )
                        ax.set_ylabel(
                            self.kpi_kwds[_kpi]["label"], fontsize=self.small_fontsize
                        )
                        ax.set_xlabel(
                            self.param_kwds[against]["label"],
                            fontsize=self.small_fontsize,
                        )
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
                            title=f'{"Uplink" if link == "UPLINK" else "Downlink"}',
                        )

            plt.tight_layout()
            plt.show()

    def plot_bar_kpi_per_algo(
        self,
        per: str,
        which_kpi: list[str],
        which_dapp_algo_ue_selection: str | list[str],
        which_xapp_algo_rbg_bounding: str | list[str],
        which_rat: str | list[str] = [LTE, NR],
        append_title: str = "",
        save_dir: str | None = None,
    ):
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
        base_mask = self.df[not_per] == base_algos[not_per]

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

                    dl_df, dl_algos = link_data["DOWNLINK"][kpi]
                    ul_df, ul_algos = link_data["UPLINK"][kpi]

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
                    fidelity_cols = [
                        "StructuralFidelity",
                        "ContentFidelity",
                        "SemanticFidelity",
                    ]
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
        which_rat: str | list[str] = [LTE, NR],
        append_title: str = "",
        decimals: int = 3,
    ):
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
        base_mask = self.df[not_per] == base_algos[not_per]

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
                        (self.kpi_kwds[kpi]["label"], stat)
                        for kpi, stat in df_algo.columns
                    ],
                    names=["kpi", "stat"],
                )
                df_algo.columns.names = ["KPI", "Statistics"]
                df_algo = df_algo.reindex(["UPLINK", "DOWNLINK"])

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

                    styler.set_caption(f"Algo — {self._algo_label(algo)}")
                    return styler

                styled = df_algo.style.pipe(style_table)
                display(styled)

    def plot_table_gain_loss_kpi_per_algo(
        self,
        per: str,
        which_kpi: list[str],
        which_dapp_algo_ue_selection: str | list[str],
        which_xapp_algo_rbg_bounding: str | list[str],
        ref_algo: str,
        decimals: int = 2,
        which_rat: str | list[str] = [LTE, NR],
        append_title: str = "",
    ):
        base_algos = {
            "dapp_algo_ue_selection": which_dapp_algo_ue_selection,
            "xapp_algo_rbg_bounding": which_xapp_algo_rbg_bounding,
        }
        if per not in base_algos:
            raise ValueError(
                f"'{per}' invalid. Must be one of {list(base_algos.keys())}."
            )

        not_per = next(k for k in base_algos if k != per)
        base_mask = self.df[not_per] == base_algos[not_per]

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

            gain_df = self._compute_gain_loss(stats, ref_algo, which_kpi, compare_algos)

            gain_df.columns.names = ["KPI", "Algorithm"]
            gain_df.index.name = None
            gain_df = gain_df.reindex(["UPLINK", "DOWNLINK"])

            gain_df.columns = pd.MultiIndex.from_tuples(
                [(self.kpi_kwds[kpi]["label"], algo) for kpi, algo in gain_df.columns],
                names=["KPI", "Algorithm"],
            )

            gain_df = gain_df.reindex(
                columns=pd.MultiIndex.from_tuples(
                    [
                        (self.kpi_kwds[kpi]["label"], self._algo_label(a))
                        for kpi in which_kpi
                        for a in compare_algos
                        if (self.kpi_kwds[kpi]["label"], self._algo_label(a))
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

                ref_label = self._algo_label(ref_algo)
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

    def _draw_bar_kpi(self, ax, depict_df, per, kpi, algos_sorted):
        algos_pos = range(len(algos_sorted))
        for pos_idx, algo in zip(algos_pos, algos_sorted):
            df_algo = depict_df[depict_df[per] == algo]
            edgecolor = "white" if algo.startswith("iB") else "black"
            if not kpi == "IntentSatisfactionScore":
                ax.bar(
                    pos_idx,
                    df_algo[kpi].mean(),
                    yerr=[
                        [np.abs(df_algo[kpi].mean() - np.min(df_algo[kpi]))],
                        [df_algo[kpi].std()],
                    ],
                    color=self.algo_kwds[algo]["color"],
                    label=self.algo_kwds[algo]["label"],
                    edgecolor=edgecolor,
                    linewidth=0.5,
                    hatch="\\" if algo.startswith("iB") else "",
                )
            else:
                structural_fid = 0.3 * df_algo["StructuralFidelity"].mean()
                content_fid = 0.3 * df_algo["ContentFidelity"].mean()
                semantic_fid = 0.2 * df_algo["SemanticFidelity"].mean()
                ax.bar(
                    pos_idx,
                    structural_fid,
                    alpha=0.8,
                    color=self.algo_kwds[algo]["color"],
                    capsize=5,
                    label="Structural Fidelity",
                    edgecolor=edgecolor,
                    linewidth=1.5,
                    hatch="///",
                )
                ax.bar(
                    pos_idx,
                    content_fid,
                    bottom=structural_fid,
                    alpha=0.8,
                    color=self.algo_kwds[algo]["color"],
                    capsize=5,
                    label="Content Fidelity",
                    edgecolor=edgecolor,
                    linewidth=1.5,
                    hatch="",
                )
                ax.bar(
                    pos_idx,
                    semantic_fid,
                    bottom=content_fid + structural_fid,
                    alpha=0.8,
                    color=self.algo_kwds[algo]["color"],
                    capsize=5,
                    label="Semantic Fidelity",
                    edgecolor=edgecolor,
                    linewidth=1.5,
                    hatch="//",
                )
                handles, legend_labels = ax.get_legend_handles_labels()
                ax.legend(
                    handles[2::-1],
                    legend_labels[2::-1],
                    fontsize=self.small_fontsize,
                    loc="lower center",
                )

        ax.set_ylabel(self.kpi_kwds[kpi]["label"], fontsize=self.small_fontsize)
        ax.set_xticks(list(algos_pos))
        labels = [self.algo_kwds[algo]["label"] for algo in algos_sorted]
        ax.set_xticklabels(
            labels, rotation=45, ha="right", fontsize=self.small_fontsize
        )

    def _algo_label(self, algo: str) -> str:
        return self.algo_kwds.get(algo, {}).get("label", algo)

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
                    row[(kpi, self._algo_label(algo))] = delta
            records.append(row)

        df = pd.DataFrame(records).set_index("link")
        df.columns = pd.MultiIndex.from_tuples(df.columns, names=["kpi", "algo"])
        return df.sort_index()

    def _setup_plot_params(self):
        self.kpi_kwds = {
            "PDR": {"label": "Packet Delivery Ratio [%]", "sort": "desc"},
            "Throughput": {"label": "Throughput [kbps]", "sort": "desc"},
            "Latency": {"label": "Latency [ms]", "sort": "asc"},
            "Jitter": {"label": "Jitter [ms]", "sort": "asc"},
            "IntentSatisfactionScore": {
                "label": "Intent Satisfaction Score",
                "sort": "desc",
            },
            "FidelityScore": {"label": "Fidelity Score", "sort": "desc"},
            "SemanticFidelity": {"label": "Semantic Fidelity", "sort": "desc"},
            "ContentFidelity": {"label": "Content Fidelity", "sort": "desc"},
            "StructuralFidelity": {
                "label": "Structural Fidelity",
                "sort": "desc",
            },
            "PRBAllocationRatio": {
                "label": "PRB Allocation Ratio",
                "sort": "asc",
            },
            "DecisionTimeMs": {"label": "Decision Time [ms]", "sort": "asc"},
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
            "iBRandom": {"color": tc.vibrant[6], "label": "IB Random"},
            "Random": {"color": tc.vibrant[6], "label": "Random"},
            "iBRoundRobin": {"color": tc.vibrant[5], "label": "IB R. Robin"},
            "RoundRobin": {"color": tc.vibrant[5], "label": "R. Robin"},
            "iBTopKCqi": {"color": tc.light[0], "label": "IB Top-K: CQI"},
            "iBTopKBufferLevel": {
                "color": tc.light[1],
                "label": "IB Top-K: Buffer Level",
            },
            "iBTopKPropFairness": {
                "color": tc.light[2],
                "label": "IB Top-K: Prop. Fairn.",
            },
            "iBTopKCriticality": {
                "color": tc.light[3],
                "label": "IB Top-K: Criticality",
            },
            "iBTopKMultiCriteria": {
                "color": tc.light[4],
                "label": "IB Top-K: Multi-Criteria",
            },
            "TopKCqi": {"color": tc.light[0], "label": "Top-K: CQI"},
            "TopKBufferLevel": {"color": tc.light[1], "label": "Top-K: Buffer Level"},
            "TopKPropFairness": {"color": tc.light[2], "label": "Top-K: Prop. Fairn."},
            "TopKCriticality": {"color": tc.light[3], "label": "Top-K: Criticality"},
            "TopKMultiCriteria": {
                "color": tc.light[4],
                "label": "Top-K: Multi-Criteria",
            },
            "iBGreedyCqi": {"color": tc.vibrant[0], "label": "IB Greedy: CQI"},
            "iBGreedyBufferLevel": {
                "color": tc.vibrant[1],
                "label": "IB Greedy: Buffer Level",
            },
            "iBGreedyPropFairness": {
                "color": tc.vibrant[2],
                "label": "IB Greedy: Prop. Fairn.",
            },
            "iBGreedyCriticality": {
                "color": tc.vibrant[3],
                "label": "IB Greedy: Criticality",
            },
            "iBGreedyMultiCriteria": {
                "color": tc.vibrant[4],
                "label": "IB Greedy: Multi-Criteria",
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
            "iBDpCqi": {"color": tc.dark[0], "label": "IB DP: CQI"},
            "iBDpBufferLevel": {"color": tc.dark[1], "label": "IB DP: Buffer Level"},
            "iBDpPropFairness": {"color": tc.dark[2], "label": "IB DP: Prop. Fairn."},
            "iBDpCriticality": {"color": tc.dark[3], "label": "IB DP: Criticality"},
            "iBDpMultiCriteria": {
                "color": tc.dark[4],
                "label": "IB DP: Multi-Criteria",
            },
            "DpCqi": {"color": tc.dark[0], "label": "DP: CQI"},
            "DpBufferLevel": {"color": tc.dark[1], "label": "DP: Buffer Level"},
            "DpPropFairness": {"color": tc.dark[2], "label": "DP: Prop. Fairn."},
            "DpCriticality": {"color": tc.dark[3], "label": "DP: Criticality"},
            "DpMultiCriteria": {"color": tc.dark[4], "label": "DP: Multi-Criteria"},
            "Bypass": {"color": tc.pale[0], "label": "Bypass"},
        }
        self.big_fontsize = 15
        self.small_fontsize = 13

    def _save_subplot(self, file_name, plot_fn, title=None, figsize=(5, 5)):
        fig_single, ax_single = plt.subplots(figsize=figsize)
        plot_fn(ax_single)
        if title:
            ax_single.set_title(title, fontsize=self.big_fontsize, fontweight="bold")
        fig_single.savefig(file_name, dpi=150, bbox_inches="tight")
        plt.close(fig_single)

    def _cleanup(self):
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
