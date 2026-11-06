# Market-1501

This folder is where ib-ORAN looks for the Market-1501 dataset. The dataset is not included in this repository, because its license does not allow redistribution.

> [!IMPORTANT]
> Download Market-1501 yourself before running ib-ORAN with `--dataset_id="market-1501"`. Its terms allow research, non-commercial use only.

## Download

Download the dataset from its [original source](https://zheng-lab-anu.github.io/Project/project_reid.html) or from the [Kaggle mirror](https://www.kaggle.com/datasets/pengcw1/market-1501). Source, license terms and citation are listed in [THIRD-PARTY.txt](../../../THIRD-PARTY.txt).

## Placement

Copy the `.jpg` images directly into this folder, not into subfolders. ib-ORAN finds each image by its Market-1501 file name, which starts with the object ID and the camera ID, for example `0002_c1s1_000451_03.jpg`.

Everything in this folder except this file is ignored by Git, so the images are never committed.

See [simulation data](../../../doc/simulation-data.md#datasets) for how datasets are selected and used.
