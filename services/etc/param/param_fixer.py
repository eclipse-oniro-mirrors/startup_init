#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2021 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import optparse
import os
import sys
import json

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, os.pardir, os.pardir, os.pardir, "build"))
from scripts.util import build_utils  # noqa: E402

def parse_args(args):
    args = build_utils.expand_file_args(args)

    parser = optparse.OptionParser()
    build_utils.add_depfile_option(parser)
    parser.add_option('--output', help='fixed para file')
    parser.add_option('--source-file', help='source para file')
    parser.add_option('--extra', help='extra params')

    options, _ = parser.parse_args(args)
    return options

def parse_params(line, contents):
    line = line.strip()
    pos = line.find('=')
    if pos <= 0:
        return
    name = line[:pos]
    value = line[pos+1:]
    name = name.strip()
    value = value.strip()
    contents[name] = value

def parse_extra_params(extras, contents):
    lines = extras.split(" ")
    for line in lines:
        line = line.strip()
        parse_params(line, contents)

def fix_para_file(options):
    contents = {}

    # Read source file
    with open(options.source_file, 'r') as f:
        lines = f.readlines()
        for line in lines:
            line = line.strip()
            # Strip comments
            if line.startswith('#') or not line:
                continue
            parse_params(line, contents)

    if options.extra:
        parse_extra_params(options.extra, contents)

    with open(options.output, 'w') as f:
        for key in contents:
            f.write(key + "=" + contents[key] + '\n')

def main(args):
    options = parse_args(args)

    depfile_deps = ([options.source_file])

    build_utils.call_and_write_depfile_if_stale(
        lambda: fix_para_file(options),
        options,
        input_paths=depfile_deps,
        output_paths=([options.output]),
        force=False,
        add_pydeps=False)

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
