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
import stat
import generate_code_from_policy as gen_policy


def parse_line(fp, arch_nr):
    arch_id_map = {
        '40000028': 'arm',
        'c00000b7': 'arm64',
        'c00000f3': 'riscv64'
    }
    for line in fp:
        line = line.strip()
        if 'audit' not in line or 'type=1326' not in line:
            continue

        pos = line.find(' syscall=')
        arch_id = line[line.find('arch=') + 5 : pos]
        syscall, _ = gen_policy.str_convert_to_int(line[pos + 9: line.find(' compat')])
        arch_nr.get(arch_id_map.get(arch_id)).add(syscall)


def get_item_content(name_nr_table, arch_nr_table):
    content = '@allowList\n'
    syscall_name_dict = {
        'arm': list(),
        'arm64': list(),
        'riscv64': list()
    }
    supported_architecture = ['arm64', 'arm', 'riscv64']
    for arch in supported_architecture:
        for nr in sorted(list(arch_nr_table.get(arch))):
            syscall_name = name_nr_table.get(arch).get(nr)
            if not syscall_name:
                raise ValueError('nr is not ilegal')
            syscall_name_dict.get(arch).append(syscall_name)

    for func_name in syscall_name_dict.get('arm64'):
        if func_name in syscall_name_dict.get('arm'):
            content =  '{}{};all\n'.format(content, func_name)
            syscall_name_dict.get('arm').remove(func_name)
        else:
            content = '{}{};arm64\n'.format(content, func_name)
    if syscall_name_dict.get('arm'):
        content = '{}{};arm\n'.format(content, ';arm\n'.join(
                  [func_name for func_name in syscall_name_dict.get('arm')]))
    if syscall_name_dict.get('riscv64'):
        content = '{}{};riscv64\n'.format(content, ';riscv64\n'.join(
                  [func_name for func_name in syscall_name_dict.get('riscv64')]))

    return content


def gen_output_file(filter_name, content):
    flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC
    modes = stat.S_IWUSR | stat.S_IRUSR | stat.S_IWGRP | stat.S_IRGRP
    with os.fdopen(os.open(filter_name + '.seccomp.policy', flags, modes), 'w') as output_file:
        output_file.write(content)


def parse_file(file_name, arch_nr):
    with open(file_name) as f:
        parse_line(f, arch_nr)


def converse_fuction_name_nr(dict_dst, dict_src):
    for arch in dict_src.keys():
        dict_dst.update({arch: dict()})

    for arch in dict_src.keys():
        for key, value in dict_src.get(arch).items():
            dict_dst.get(arch).update({value: key})
    return dict_dst


def parse_audit_log_to_policy(args):
    file_list = extract_file_from_path(args.src_path)
    function_name_nr_table_dict_tmp = {}
    function_name_nr_table_dict = {}
    arch_nr = {
        'arm': set(),
        'arm64': set(),
        'riscv64': set()
    }
    for file_name in file_list:
        file_name_tmp = file_name.split('/')[-1]
        if not file_name_tmp.lower().startswith('libsyscall_to_nr_'):
            continue
        function_name_nr_table_dict_tmp = gen_policy.gen_syscall_nr_table(file_name, function_name_nr_table_dict_tmp)

    converse_fuction_name_nr(function_name_nr_table_dict, function_name_nr_table_dict_tmp)

    for file_name in file_list:
        if file_name.lower().endswith('.audit.log'):
            parse_file(file_name, arch_nr)

    content = get_item_content(function_name_nr_table_dict, arch_nr)
    gen_output_file(args.filter_name, content)


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
                        help='path to syscall to nr files')
    parser.add_argument('--filter-name', type=str,
                        help=('The input files\n'))


    args = parser.parse_args()
    parse_audit_log_to_policy(args)


if __name__ == '__main__':
    sys.exit(main())
