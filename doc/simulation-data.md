# Simulation Data Guide
 
This document details the input datasets feeding the simulator based on different data formats as well as the discovered output after a simulation run.


## Input

ib-ORAN accepts datasets formatted according to the configured payload format:

- `IMAGE` → Labeled images, each containing a specific object ID
- `EMBEDDING` → Embeddings of the labeled images, generated automatically based on the selected model upon a primary simulation run (if they are missing)

ib-ORAN supports the following image datasets, stored in the [`/notebook/dataset`](/notebook/dataset):

- `toy` → A sample dataset for validating the simulation framework, containing 10 object IDs. Included in this repository.
- `market-1501` → A benchmark person re-identification dataset consisting of automatically detected bounding-box images captured across multiple overlapping outdoor camera views to evaluate object ID matching across disjoint surveillance feeds. Not included in this repository — see [`THIRD-PARTY.txt`](/THIRD-PARTY.txt) for source, license terms, and citation, and place downloaded contents in the `market-1501` folder inside the dataset repository.

> **Note:** Additional payload formats are currently under development.

### Embedding Extraction

ib-ORAN supports the following embedding extraction models (automatically fetched online to `/notebook/models/emb/` the first time):

- `CLIP` → A multimodal framework trained via contrastive learning on image-text pairs to embed visual and textual concepts into a shared feature space for zero-shot image classification and retrieval (see [CLIP Model](https://huggingface.co/docs/transformers/en/model_doc/clip)).
- `TransReID` → A pure Transformer architecture tailored for person and vehicle re-identification that leverages patch-level self-attention alongside side-information embeddings (such as camera ID and view angle) to extract robust, invariant feature representations (see [TransReID Model](https://github.com/damo-cv/TransReID)).


## Output

Each simulation run produces the following outputs:

| Output | Location |
|---|---|
| Performance metrics logs | Console / log files |
| Recovered payload dataset | [`/notebook/dataset_dscv`](/notebook/dataset_dscv) |
| NetAnim XML animation file | [`/notebook/netanim/`](/notebook/netanim) |
| Real-time console flow stats | Console *(optional)* |
| Detailed dApp / xApp control logs | [`/notebook/logs/`](/notebook/logs) *(optional)* |
