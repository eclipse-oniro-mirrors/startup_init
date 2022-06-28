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

def DecodeCfgLine(data):
    data.replace('\n', '').replace('\r', '')
    data = data.strip()
    if (len(data) == 0 or data[0] == '#'):
        return "", ""
    strs = data.split('=')
    if len(strs) <= 1:
        return "", ""
    return strs[0].strip(), strs[1].strip()

def GetParamFromCfg(cfgName):
    dict = {}
    with open(cfgName) as afile:
        data = afile.readline()
        while data:
            name, value = DecodeCfgLine(data)
            if len(name) != 0 and len(value) != 0:
                dict[name] = value
                print("sample file name={%s %s}"%(name, value))
            data = afile.readline()
    return dict

def DecodeCodeLine(data):
    data.replace('\n', '').replace('\r', '')
    data = data.strip()
    if (not data.startswith("PARAM_MAP")):
        return "", ""
    dataLen = len(data)
    data = data[len("PARAM_MAP") + 1 :  dataLen - 1]
    data = data.strip()
    strs = data.split(',')
    if len(strs) <= 1:
        return "", ""
    return strs[0].strip(), data[len(strs[0]) + 1: ].strip()

def GetParamFromCCode(codeName):
    dict = {}
    with open(codeName, "r+") as afile:
        data = afile.readline()
        while data:
            name, value = DecodeCodeLine(data)
            if len(name) != 0 and len(value) != 0:
                dict[name] = value
            data = afile.readline()
        afile.truncate(0)
    return dict

def WriteMapToCode(codeName, dict):
    try:
        f = open(codeName, 'w')
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
        for name, value in  dict.items():
            if (value.startswith("\"")):
                str = "    PARAM_MAP({0}, {1})".format(name, value)
                f.write(str + os.linesep)
            else:
                str = "    PARAM_MAP({0}, \"{1}\")".format(name, value)
                f.write(str + os.linesep)
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
        print("Error: open or write file %s fail"%{codeName})
    else:
        f.close()
    return 0

def AddToCodeDict(codeDict, cfgDict, high = True):
    for name, value in  cfgDict.items():
        # check if name exit
        hasKey = name in codeDict #codeDict.has_key(name)
        if hasKey and high:
            codeDict[name] = value
        elif not hasKey:
            codeDict[name] = value
    return codeDict

def main():
    parser = argparse.ArgumentParser(
    description='A common change param.para file to h.')
    parser.add_argument(
        '--source',
        help='The source to change.',
        required=True)
    parser.add_argument(
        '--dest_dir',
        help='Path that the source should be changed to.',
        required=True)
    parser.add_argument(
        '--priority',
        help='If priority is 1, replace the parameter if it exist.',
        required=True)
    args = parser.parse_args()

    out_dir = args.dest_dir
    if not os.path.exists(out_dir):
        os.makedirs(out_dir, exist_ok=True)
    print(out_dir)

    source = args.source
    assert os.path.exists(source)

    srcDict = GetParamFromCfg(source)
    dst = out_dir + "param_cfg.h"

    if os.path.exists(dst):
        dstDict = GetParamFromCCode(dst)
    else:
        dstDict = {}

    dstDict = AddToCodeDict(dstDict, srcDict, args.priority == "1")
    WriteMapToCode(dst, dstDict)
    return 0


if __name__ == '__main__':
    sys.exit(main())
