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

import sys
import argparse
import audit_log_analysis as audit_policy
import generate_code_from_policy as gen_policy


class MergePolicy:
    def __init__(self):
        self.cur_parse_item = ''
        self.arches = set()
        self.seccomp_policy_param = dict()

    def update_parse_item(self, line):
        item = line[1:]
        if item in gen_policy.supported_parse_item:
            self.cur_parse_item = item
            print('start deal with {}'.format(self.cur_parse_item))

    def parse_line(self, line):
        if not self.cur_parse_item :
            return
        line = line.replace(' ', '')
        pos = line.rfind(';')
        if pos < 0:
            for arch in self.arches:
                self.seccomp_policy_param.get(arch).value_function.get(self.cur_parse_item)(line)
        else:
            arches = line[pos + 1:].split(',')
            if arches[0] == 'all':
                arches = gen_policy.supported_architecture
            for arch in arches:
                self.seccomp_policy_param.get(arch).value_function.get(self.cur_parse_item)(line[:pos])

    @staticmethod
    def get_item_content(name_nr_table, item_str, itme_dict):
        syscall_name_dict = {}
        flag = False
        for arch in gen_policy.supported_architecture:
            func_name_to_nr = dict()
            for item in itme_dict.get(arch):
                if ':' in item:
                    func_name = item[:item.find(':')].strip()
                else:
                    func_name = item
                func_name_to_nr.update({item: name_nr_table.get(arch).get(func_name)})
            func_name_to_nr_list = sorted(func_name_to_nr.items(), key=lambda x : x[1])

            syscall_name_dict.update({arch: func_name_to_nr_list})
        for arch in gen_policy.supported_architecture:
            if syscall_name_dict.get(arch):
                flag = True
        if not flag:
            return ''
        content = '{}\n'.format(item_str)

        for func_name, _ in syscall_name_dict.get('arm64'):
            flag = False
            for func_name_arm, nr_arm in syscall_name_dict.get('arm'):
                if func_name == func_name_arm:
                    content = '{}{};all\n'.format(content, func_name)
                    syscall_name_dict.get('arm').remove((func_name, nr_arm))
                    flag = True
                    break
            if not flag:
                content = '{}{};arm64\n'.format(content, func_name)
        if (syscall_name_dict.get('arm')):
            content = '{}{};arm\n'.format(content, ';arm\n'.join(
                      [func_name for func_name, _ in syscall_name_dict.get('arm')]))
        if (syscall_name_dict.get('riscv64')):
            content = '{}{};riscv64\n'.format(content, ';riscv64\n'.join(
                      [func_name for func_name, _ in syscall_name_dict.get('riscv64')]))
        return content

    def parse_open_file(self, fp):
        for line in fp:
            line = line.strip()
            if not line:
                continue
            if line[0] == '#':
                continue
            if line[0] == '@':
                self.update_parse_item(line)
                continue
            if line[0] != '@' and self.cur_parse_item == '':
                continue
            self.parse_line(line)

    def parse_file(self, file_path):
        with open(file_path) as fp:
            self.parse_open_file(fp)

    def merge_policy(self, args):
        function_name_nr_table_dict = {}
        for file_name in args.src_files:
            file_name_tmp = file_name.split('/')[-1]
            if not file_name_tmp.lower().startswith('libsyscall_to_nr_'):
                continue
            gen_policy.gen_syscall_nr_table(file_name, function_name_nr_table_dict)

        for arch in gen_policy.supported_architecture:
            self.seccomp_policy_param.update(\
                {arch: gen_policy.SeccompPolicyParam(arch, function_name_nr_table_dict.get(arch))})

        for file_name in args.src_files:
            if file_name.lower().endswith('.policy'):
                self.parse_file(file_name)

        dict_priority = dict()
        dict_allow_list = dict()
        dict_priority_with_args = dict()
        dict_allow_list_with_args = dict()
        dict_blocklist = dict()

        for arch in gen_policy.supported_architecture:
            dict_priority.update({arch: self.seccomp_policy_param.get(arch).priority})
            dict_allow_list.update({arch: self.seccomp_policy_param.get(arch).allow_list})
            dict_priority_with_args.update({arch: self.seccomp_policy_param.get(arch).priority_with_args})
            dict_allow_list_with_args.update({arch: self.seccomp_policy_param.get(arch).allow_list_with_args})
            dict_blocklist.update({arch: self.seccomp_policy_param.get(arch).blocklist})

        content = self.get_item_content(function_name_nr_table_dict, "@priority", dict_priority)
        content += self.get_item_content(function_name_nr_table_dict, "@allowList", dict_allow_list)
        content += self.get_item_content(function_name_nr_table_dict, "@priorityWithArgs", dict_priority_with_args)
        content += self.get_item_content(function_name_nr_table_dict, "@allowListWithArgs", dict_allow_list_with_args)
        content += self.get_item_content(function_name_nr_table_dict, "@blockList", dict_blocklist)
        audit_policy.gen_output_file(args.filter_name, content)


def main():
    parser = argparse.ArgumentParser(
      description='Generates a seccomp-bpf policy')
    parser.add_argument('--src-files', type=str, action='append',
                        help=('input libsyscall_to_nr files and policy filse\n'))

    parser.add_argument('--filter-name',  type=str,
                        help='Name of seccomp bpf array generated by this script')

    args = parser.parse_args()

    generator = MergePolicy()
    generator.merge_policy(args)


if __name__ == '__main__':
    sys.exit(main())
