#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# Copyright (c) 2023 Huawei Device Co., Ltd.
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
import sys
import os
import audit_log_analysis as audit_policy
import generate_code_from_policy as gen_policy



def parse_line(fp):
    func_name_set = set()
    for line in fp:
        line = line.strip()
        func_name = line[:line.find('(')]
        if ' ' in func_name:
            func_name = func_name.split(' ')[-1].strip()
        func_name_set.add(func_name)

    return func_name_set


def get_item_content(arch, func_name_set, name_nr_table):
    func_name_to_nr = dict()
    for func_name in func_name_set:
        if func_name.startswith('arm_'):
            func_name = func_name[len('arm_'):]
        if func_name in name_nr_table.get(arch).keys():
            func_name_to_nr.update({func_name: name_nr_table.get(arch).get(func_name)})

    func_name_to_nr_list = sorted(func_name_to_nr.items(), key=lambda x : x[1])
    content = '@allowList\n'
    func_name_list = [func_name for func_name, _ in func_name_to_nr_list]
    
    content = '{}{};{}\n'.format(content, ';{}\n'.format(arch).join(func_name_list), arch)

    return content


def parse_file(file_name):
    with open(file_name) as f:
        return parse_line(f)


def parse_strace_log_to_policy(args):
    file_list = extract_file_from_path(args.src_path)
    function_name_nr_table_dict = {}
    for file_name in file_list:
        file_name_tmp = file_name.split('/')[-1]
        if not file_name_tmp.lower().startswith('libsyscall_to_nr_'):
            continue
        gen_policy.gen_syscall_nr_table(file_name, function_name_nr_table_dict)

    func_name_set = set()
    for file_name in file_list:
        if '.strace.log' in file_name.lower():
            func_name_set |= parse_file(file_name)

    content = get_item_content(args.target_cpu, func_name_set, function_name_nr_table_dict)
    audit_policy.gen_output_file(args.filter_name, content)


def extract_file_from_path(dir_path):
    file_list = []
    for path in dir_path:
        if path[-1] == '/':
            print('input dir path can not end with /')
            return []

        if os.path.isdir(path):
            # get file list
            file_list_tmp = os.listdir(path)
            file_list += ['{}/{}'.format(path, item) for item in file_list_tmp]

    return file_list


def main():
    parser = argparse.ArgumentParser(
      description='Generates a seccomp-bpf policy')
    parser.add_argument('--src-path', action='append',
                        help='the path of directory which includes strace log and libsyscall_to_nr files')
    parser.add_argument('--target-cpu', type=str,
                        help='input arm or arm64 or riscv64')
    parser.add_argument('--filter-name', type=str,
                        help=('consist of output file name\n'))

    args = parser.parse_args()
    if args.target_cpu not in gen_policy.supported_architecture:
        raise ValueError("target_cpu must int {}".format(gen_policy.supported_architecture))
    parse_strace_log_to_policy(args)


if __name__ == '__main__':
    sys.exit(main())
