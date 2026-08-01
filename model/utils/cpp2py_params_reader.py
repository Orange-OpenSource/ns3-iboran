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
import time

from model.utils.file_store import FileStore


class Cpp2PyParamsReader:
    _FOLDER = "logs"
    _FILE = "cpp2PyParams.json"

    def __init__(self, base_dir: str = "."):
        self.base_dir = base_dir
        self._params: dict | None = None

    def reload(self, timeout: float = 30.0, interval: float = 0.01) -> dict:
        """Force a fresh read from disk, waiting for the file to appear if needed."""
        path = os.path.join(self.base_dir, self._FOLDER, self._FILE)
        self._wait_for_file(path, timeout=timeout, interval=interval)
        self._params = FileStore(self.base_dir, self._FOLDER, self._FILE).json_load()
        return self._params

    @staticmethod
    def _wait_for_file(
        path: str, timeout: float = 30.0, interval: float = 0.01
    ) -> None:
        start = time.time()
        while not os.path.exists(path):
            if time.time() - start > timeout:
                raise TimeoutError(
                    f"Timed out after {timeout}s waiting for {path} to be created. "
                    "Check that the ns-3 binary compiled and started successfully."
                )
            time.sleep(interval)

    def _get_params(self) -> dict:
        if self._params is None:
            self.reload()
        return self._params

    @property
    def common(self) -> dict:
        return self._get_params()["common"]

    @property
    def ue_num(self) -> int:
        return self._get_params()["common"]["ueNum"]

    @property
    def rat_type(self) -> str:
        return self._get_params()["ratType"]

    def get(self, key: str, section: str = "common"):
        return self._get_params()[section][key]
