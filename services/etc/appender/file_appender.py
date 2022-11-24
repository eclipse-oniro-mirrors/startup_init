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
import stat
import json


def parse_args(args):
    from scripts.util import build_utils  # noqa: E402

    args = build_utils.expand_file_args(args)

    parser = optparse.OptionParser()
    build_utils.add_depfile_option(parser)
    parser.add_option('--output', help='fixed para file')
    parser.add_option('--source-file', help='source para file')
    parser.add_option('--append-file', action="append", type="string", dest="files", help='appended files')
    parser.add_option('--append-line', action="append", type="string", dest="lines", help='appended lines')

    options, _ = parser.parse_args(args)
    return options


def append_files(target_f, options):
    # Read source file
    with open(options.source_file, 'r') as source_f:
        source_contents = source_f.read()

    target_f.write(source_contents)
    if options.files:
        for append_f in options.files:
            with open(append_f, 'r') as src:
                target_f.write(src.read())
    if options.lines:
        for line in options.lines:
            target_f.write(line)
            target_f.write("\n")


def main(args):
    sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
        os.pardir, os.pardir, os.pardir, os.pardir, "build"))
    from scripts.util import build_utils  # noqa: E402

    options = parse_args(args)

    if options.files:
        depfile_deps = ([options.source_file] + options.files)
    else:
        depfile_deps = ([options.source_file])

    with os.fdopen(os.open(options.output, os.O_RDWR | os.O_CREAT, stat.S_IWUSR | stat.S_IRUSR), 'w') as target_f:
        append_files(target_f, options)

    build_utils.write_depfile(options.depfile,
                options.output, depfile_deps, add_pydeps=False)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
