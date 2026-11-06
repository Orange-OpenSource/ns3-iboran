# Simulation data

This document describes the data that feeds an ib-ORAN simulation and the outputs of a run. Paths are relative to `contrib/iboran`.

## Inputs

### Payload formats

UEs carry payloads in the format set with `--data_format`:

| Format | Payload |
|---|---|
| `IMAGE` | Labeled images, each showing one object ID |
| `EMBEDDING` | Embeddings of the labeled images, extracted with the model set by `--embedder` |

When the embeddings of a dataset are missing, the first run with `EMBEDDING` extracts and stores them.

> [!NOTE]
> The `VIDEO` and `TEXT` formats are under development.

### Datasets

Datasets are stored in [`notebook/dataset/`](../notebook/dataset), one folder per dataset, and selected with `--dataset_id`. Their filename patterns are set in the `datasets` section of [`iboranConfig.yaml`](../model/iboran_campaign/iboranConfig.yaml).

| ID | Description | Included |
|---|---|---|
| `toy` | Sample dataset of 10 object IDs, for validating the framework | Yes |
| `market-1501` | Person re-identification benchmark of bounding-box images detected across several outdoor camera views | No |

To use Market-1501, download it from the source listed in [THIRD-PARTY.txt](../THIRD-PARTY.txt), which also gives its license terms and citation, and place its contents in `notebook/dataset/market-1501/`.

### Embedding models

The embedding models are downloaded to `notebook/models/emb/` on first use.

| Model | Description |
|---|---|
| [`CLIP`](https://huggingface.co/docs/transformers/en/model_doc/clip) | Multimodal model trained by contrastive learning on image-text pairs, which maps images and text into a shared feature space |
| [`TransReID`](https://github.com/damo-cv/TransReID) | Transformer model for person and vehicle re-identification, which adds side information such as camera ID and view angle to patch-level self-attention |

### Object IDs and intents

Each UE is assigned one object ID drawn uniformly from `--oid_range`, seeded by `--rng_run`. The intent of each link direction is drawn from the same range with the same seed. A UE is intent-relevant on a link when its object ID matches the intent of that link. A narrow range keeps at least one intent-relevant UE per link in most replications. A link with no intent-relevant UE reports a [`SatisfactionScore`](kpis.md#intent-based-kpis) of NaN, which campaign averages skip.

## Outputs

| Output | Location | Written |
|---|---|---|
| Run statistics and KPI summary | Console | With `--verbose=true` |
| KPIs of the run (see [KPIs](kpis.md)) | `notebook/logs/kpi.json` | Always |
| Merged run configuration, read by ns-3 | `notebook/logs/runConfig.yaml` | Always |
| dApp control log | `notebook/logs/` | With `--save_dapp_log=true` |
| Flow monitor statistics | `notebook/.flowstats/iboran-flowstats.xml` | Always |
| Recovered payloads | `notebook/dataset_dscv/<dataset>/` | Always |
| NetAnim animation (see [visualization](visualization.md)) | `notebook/netanim/iboran-<rat>-anim.xml` | Always |

### Cached results

A [campaign](usage.md#campaign) caches the KPI records of each run in `notebook/logs/run_data/` and reuses them on the next call with the same settings. Delete this folder and `notebook/logs/checkpoints/` after any change to the simulator code, or the RAN model.
