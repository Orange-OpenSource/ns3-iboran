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
import json
import joblib
import numpy as np


class DoLog:
    def __init__(self, script_dir=None, folder_name="logs", log_name="log_no_name"):
        self.script_dir = script_dir
        self.folder_name = folder_name
        self.log_name = log_name
        self.log_filename = None

        if script_dir is None:
            self.script_dir = os.path.dirname(os.path.abspath(__file__))

        self.setup_dir()

    def setup_dir(self):
        log_folder = os.path.join(self.script_dir, self.folder_name)
        os.makedirs(log_folder, exist_ok=True)
        self.log_filename = os.path.join(log_folder, self.log_name)

    def exists(self):
        return os.path.isfile(self.log_filename)

    def overwrite(self, content):
        with open(self.log_filename, "w") as log_file:
            log_file.write(content)

    def append(self, content):
        with open(self.log_filename, "a") as log_file:
            log_file.write(content)

    def remove(self):
        if os.path.isfile(self.log_filename):
            os.remove(self.log_filename)

    def json_dump(self, content):
        with open(self.log_filename, "w") as file:
            json.dump(content, file, indent=4, default=self._json_serializable)

    def json_load(self):
        with open(self.log_filename, "r") as file:
            return json.load(file)

    def joblib_dump(self, content):
        joblib.dump(content, self.log_filename)

    def joblib_load(self):
        return joblib.load(self.log_filename)

    def _json_serializable(self, obj):
        if isinstance(obj, np.integer):
            return int(obj)
        if isinstance(obj, np.floating):
            return float(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        raise TypeError(f"Type {type(obj)} not serializable")
