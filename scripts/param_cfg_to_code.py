#!/usr/bin/env python
# -*- coding: utf-8 -*-

#
# Copyright (c) 2022 Huawei Device Co., Ltd.
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
#

import argparse
import os
import sys


def decode_cfg_line(data):
    data.replace('\n', '').replace('\r', '')
    data = data.strip()
    if (len(data) == 0 or data[0] == '#'):
        return "", ""
    strs = data.split('=')
    if len(strs) <= 1:
        return "", ""
    return strs[0].strip(), strs[1].strip()


def get_param_from_cfg(cfg_name):
    data_dict = {}
    with open(cfg_name) as afile:
        data = afile.readline()
        while data:
            name, value = decode_cfg_line(data)
            if len(name) != 0 and len(value) != 0:
                data_dict[name] = value
                print("sample file name={%s %s}" % (name, value))
            data = afile.readline()
    return data_dict


def decode_code_line(data):
    data.replace('\n', '').replace('\r', '')
    data = data.strip()
    if (not data.startswith("PARAM_MAP")):
        return "", ""
    data_len = len(data)
    data = data[len("PARAM_MAP") + 1 :  data_len - 1]
    data = data.strip()
    strs = data.split(',')
    if len(strs) <= 1:
        return "", ""
    return strs[0].strip(), data[len(strs[0]) + 1:].strip()


def get_param_from_c_code(code_name):
    data_dict = {}
    with open(code_name, "r+") as afile:
        data = afile.readline()
        while data:
            name, value = decode_code_line(data)
            if len(name) != 0 and len(value) != 0:
                data_dict[name] = value
            data = afile.readline()
        afile.truncate(0)
    return data_dict


def write_map_to_code(code_name, data_dict):
    try:
        with open(code_name, "w") as f:
            # start with 0
            f.seek(0)
            # write file header
            f.write('#ifndef PARAM_LITE_DEF_CFG_' + os.linesep)
            f.write('#define PARAM_LITE_DEF_CFG_' + os.linesep)
            f.write('#include <stdint.h>' + os.linesep + os.linesep)
            f.write('#ifdef __cplusplus' + os.linesep)
            f.write('#if __cplusplus' + os.linesep)
            f.write('extern "C" {' + os.linesep)
            f.write('#endif' + os.linesep)
            f.write('#endif' + os.linesep + os.linesep)

            #define struct
            f.write('typedef struct Node_ {' + os.linesep)
            f.write('    const char *name;' + os.linesep)
            f.write('    const char *value;' + os.linesep)
            f.write('} Node;' + os.linesep  + os.linesep)
            f.write('#define PARAM_MAP(name, value) {(const char *)#name, (const char *)#value},')
            f.write(os.linesep  + os.linesep)
            # write data
            f.write('static Node g_paramDefCfgNodes[] = {' + os.linesep)
            for name, value in  data_dict.items():
                if (value.startswith("\"")):
                    tmp_str = "    PARAM_MAP({0}, {1})".format(name, value)
                    f.write(tmp_str + os.linesep)
                else:
                    tmp_str = "    PARAM_MAP({0}, \"{1}\")".format(name, value)
                    f.write(tmp_str + os.linesep)
            f.write('};' + os.linesep + os.linesep)

            #end
            f.write('#ifdef __cplusplus' + os.linesep)
            f.write('#if __cplusplus' + os.linesep)
            f.write('}' + os.linesep)
            f.write('#endif' + os.linesep)
            f.write('#endif' + os.linesep)
            f.write('#endif // PARAM_LITE_DEF_CFG_' + os.linesep)
            f.write(os.linesep)
            f.truncate()
    except IOError:
        print("Error: open or write file %s fail" % {code_name})
    return 0


def add_to_code_dict(code_dict, cfg_dict, high=True):
    for name, value in  cfg_dict.items():
        # check if name exit
        has_key = name in code_dict
        if has_key and high:
            code_dict[name] = value
        elif not has_key:
            code_dict[name] = value
    return code_dict


def main():
    parser = argparse.ArgumentParser(
    description='A common change param.para file to h.')
    parser.add_argument(
        '--source',
        action='append',
        help='The source to change.',
        required=True)
    parser.add_argument(
        '--dest_dir',
        help='Path that the source should be changed to.',
        required=True)
    args = parser.parse_args()

    out_dir = args.dest_dir
    if not os.path.exists(out_dir):
        os.makedirs(out_dir, exist_ok=True)
    print("out_dir {}".format(out_dir))

    for source in args.source:
        print("source {}".format(out_dir))
        if not os.path.exists(source):
            raise FileNotFoundError

        src_dict = get_param_from_cfg(source)
        dst = "".join([out_dir, "param_cfg.h"])

        if os.path.exists(dst):
            dst_dict = get_param_from_c_code(dst)
        else:
            dst_dict = {}

        dst_dict = add_to_code_dict(dst_dict, src_dict, False)
        write_map_to_code(dst, dst_dict)
    return 0


if __name__ == '__main__':
    sys.exit(main())
