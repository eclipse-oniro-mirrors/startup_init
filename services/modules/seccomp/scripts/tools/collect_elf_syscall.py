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


import os
import sys
import argparse
import subprocess
import stat
import libc_static_analysis as gen_libc
import audit_log_analysis as audit_policy
import generate_code_from_policy as gen_policy


#modified the path of objdump and readelf path
def get_obj_dump_path():
    obj_dump_path = ''
    return obj_dump_path


def get_read_elf_path():
    read_elf_path = ''
    return read_elf_path


def create_needed_file(elf_path, locate_path, cmd, suffix):
    if locate_path[-1] != '/':
        locate_path = '{}/'.format(locate_path)
    elf_file_name = elf_path.split('/')[-1].split('.')[0] + suffix
    target_path = '{}{}'.format(locate_path, elf_file_name)
    flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC
    modes = stat.S_IWUSR | stat.S_IRUSR | stat.S_IWGRP | stat.S_IRGRP
    with os.fdopen(os.open(target_path, flags, modes), 'w') as output_file:
        process = subprocess.Popen(cmd.split(' '), stdout=output_file)
        process.communicate(timeout=3)
    return target_path


def generate_libc_asm(target_cpu, elf_path, locate_path):
    if target_cpu == 'arm':
        cmd_obj_dump = 'arm-linux-musleabi-objdump'
    elif target_cpu == 'arm64':
        cmd_obj_dump = 'aarch64-linux-musl-objdump'
    elif target_cpu == 'riscv64':
        cmd_obj_dump = 'riscv64-linux-musl-objdump'
    else:
        raise ValueError("target cpu error")

    cmd = '{} -d {}'.format(cmd_obj_dump, elf_path)
    return create_needed_file(elf_path, locate_path, cmd, '.asm')


def get_lib_path(elf_path, elf_name, cmd_extra):
    grep_unstrip = ' | grep unstripped | grep -v _x64 {}'.format(cmd_extra)

    if elf_name == 'libc++.so':
        grep_unstrip = '| grep aarch64-linux'
    cmd = 'find {} -name {}{}'.format(elf_path, elf_name, grep_unstrip) 
    result_list = os.popen(cmd).read().split('\n')
    result = result_list[0].strip()
    for item in result_list:
        item = item.strip()
        if len(item) > len(result):
            result = item
    return result


def extract_elf_name(elf_path):
    cmd = '{} -d {} | grep \'Shared library\''.format(get_read_elf_path(), elf_path)
    result = os.popen(cmd).read().strip()
    elf_name = set()
    for item in result.split('\n'):
        name = item[item.find('[') + 1: item.find(']')]
        if name == 'libc.so' or name == '':
            continue
        elf_name.add(name)
    
    return elf_name


def extract_undef_name(elf_path):
    cmd = '{} -sW {} | grep UND'.format(get_read_elf_path(), elf_path)
    result = os.popen(cmd).read().strip()
    func_name = set()
    for item in result.split('\n'):
        name = item[item.find('UND') + 3:].strip()
        if name != '':
            func_name.add(name)

    return func_name


def collect_elf(elf_path, elf_name):
    elf_list = set(elf_name)
    elf_path_list = set()
    current_elf_name = set(elf_name)
    while current_elf_name:
        elf_path_list_tmp = set()
        elf_list |= current_elf_name
        for lib_name in current_elf_name:
            elf_path_list_tmp.add(get_lib_path(elf_path, lib_name, ''))

        current_elf_name.clear()
        for lib_path in elf_path_list_tmp:
            current_elf_name |= extract_elf_name(lib_path)
        elf_path_list |= elf_path_list_tmp
        elf_path_list_tmp.clear()

    return elf_path_list


def collect_undef_func_name(elf_path_list):
    func_name = set()
    for elf_path in elf_path_list:
        func_name |= extract_undef_name(elf_path)

    return func_name


def collect_syscall(undef_func_name, libc_func_map):
    syscall_nr = set()

    for libc_func in libc_func_map:
        for func_name in undef_func_name:
            if func_name == libc_func.func_name:
                syscall_nr |= libc_func.nr
                break

    return syscall_nr


def get_item_content(arch, nr_set, name_nr_table):
    func_name_list = list()
    for nr in sorted(list(nr_set)):
        func_name_list.append(name_nr_table.get(arch).get(nr))
    content = '@allowList\n{};{}\n'.format(';{}\n'.format(arch).join(func_name_list), arch)

    return content


def get_und_func_except_libc(elf_path, libc_func_list):
    und_func = extract_undef_name(elf_path)
    und_func_except_libc = [item for item in und_func if item not in libc_func_list]
    return und_func_except_libc


def get_und_libc_func(elf_path, libc_func_list):
    und_func = extract_undef_name(elf_path)
    und_func_only_libc = [item for item in und_func if item in libc_func_list]
    return und_func_only_libc


def create_disassemble_file(elf_path, locate_path, section):
    cmd = '{} -d --section={} {}'.format(get_obj_dump_path(), section, elf_path)
    return create_needed_file(elf_path, locate_path, cmd, section + '.asm')


def remove_disassemble_file(path):
    file_list = [item for item in os.listdir(path) if item.endswith('.asm')]
    subprocess.call(['rm'] + file_list)


class FuncCallee:
    def __init__(self, func_name):
        self.func_name = func_name
        self.func_callee = set()

    def print_info(self):
        print('{} call function {}'.format(self.func_name, self.func_callee))


def parse_line(fp):
    func_list = []
    func_call_tmp = None
    for line in fp:
        if '>:' in line:
            if func_call_tmp:
                func_list.append(func_call_tmp)
            func_call_tmp = FuncCallee(line[line.find('<') + 1: line.find('>:')])
            continue

        if '<' in line and ">" in line:
            func_name_callee = line[line.find('<') + 1: line.find('>')]
            if '@plt' in func_name_callee:
                func_call_tmp.func_callee.add(func_name_callee[:func_name_callee.find('@plt')])
            elif '+0x' in func_name_callee:
                continue
            else:
                func_call_tmp.func_callee.add(func_name_callee)
    func_list.append(func_call_tmp)
    return func_list


def parse_text_asm_file(elf_asm_path):
    func_list = list()
    with open(elf_asm_path) as fp:
        func_list = parse_line(fp)

    return func_list


def generate_libc_dict(libc_func):
    libc_dict = dict()
    for item in libc_func:
        libc_dict.update({item: set({item})})

    return libc_dict


def add_caller(callee, func_map):
    caller = set()
    for item in func_map:
        if callee in item.func_callee:
            caller.add(item.func_name)
    return caller


def update_libc_func_caller(caller_list, func_map):
    caller_list_old_len = -1
    caller_list_new_len = len(caller_list)
    current_caller_list = caller_list

    while caller_list_old_len != caller_list_new_len:
        caller_list_old_len = caller_list_new_len
        add_current_caller_list = set()
        for callee in current_caller_list:
            add_current_caller_list |= add_caller(callee, func_map)
        current_caller_list = add_current_caller_list
        caller_list |= add_current_caller_list
        caller_list_new_len = len(caller_list)

    return caller_list


def generate_libc_func_map(libc_dict, func_map):
    for key in libc_dict.keys():
        update_libc_func_caller(libc_dict.get(key), func_map)


def get_lib_func_to_other_func_maps(elf_path, libc_func_list):
    libc_func = get_und_libc_func(elf_path, libc_func_list)
    elf_asm_path = create_disassemble_file(elf_path, '.', '.text')
    func_map = parse_text_asm_file(elf_asm_path)
    libc_dict = generate_libc_dict(libc_func)
    generate_libc_func_map(libc_dict, func_map)
    return libc_dict


def extract_libc_func(callee_und_func_name, libc_func_maps):
    libc_func = set()
    for func_name in callee_und_func_name:
        for key in libc_func_maps.keys():
            if func_name in libc_func_maps.get(key):
                libc_func.add(key)
    return libc_func


def get_function_name_nr_table(src_syscall_path):
    function_name_nr_table_dict = {}
    for file_name in src_syscall_path:
        file_name_tmp = file_name.split('/')[-1]
        if not file_name_tmp.lower().startswith('libsyscall_to_nr_'):
            continue
        gen_policy.gen_syscall_nr_table(file_name, function_name_nr_table_dict)

    return function_name_nr_table_dict


def collect_concrete_syscall(args):
    if args.target_cpu == 'arm64':
        arch_str = 'aarch64-linux'
    elif args.target_cpu == 'arm':
        arch_str = 'arm-linux'
    elif args.target_cpu == 'riscv64':
        arch_str = 'riscv64-linux'
    libc_path = get_lib_path(args.src_elf_path, 'libc.so', ' | grep ' + arch_str)
    libc_asm_path = generate_libc_asm(args.target_cpu, libc_path, '.')

    # get the map of libc function to syscall nr used by the function
    libc_func_map = gen_libc.get_syscall_map(args.target_cpu, args.src_syscall_path, libc_asm_path)

    libc_func_used = set()
    # get libc function list
    libc_func_list = [item.func_name for item in libc_func_map]

    for elf_name in args.elf_name:
        elf_name_path = get_lib_path(args.src_elf_path, elf_name, '')
        # get libc function symbols used by the elf files
        libc_func_used |= set(get_und_libc_func(elf_name_path, libc_func_list))
    current_elf_name_list = args.elf_name
    while len(current_elf_name_list) != 0:
        for elf_name in current_elf_name_list:
            elf_name_path = get_lib_path(args.src_elf_path, elf_name, '')
            deps_elf_name_list = extract_elf_name(elf_name_path)
            callee_und_func_name = get_und_func_except_libc(elf_name_path, libc_func_list)

            for deps_elf_name in deps_elf_name_list:
                deps_elf_path = get_lib_path(args.src_elf_path, deps_elf_name, '')
                # get the direct caller and indirect caller of libc function 
                libc_func_maps = get_lib_func_to_other_func_maps(deps_elf_path, libc_func_list)
                libc_func_used |= extract_libc_func(callee_und_func_name, libc_func_maps)
        current_elf_name_list = deps_elf_name_list
    syscall_nr_list = collect_syscall(libc_func_used, libc_func_map)

    nr_to_func_dict = dict()
    function_name_nr_table_dict = get_function_name_nr_table(args.src_syscall_path)
    audit_policy.converse_fuction_name_nr(nr_to_func_dict, function_name_nr_table_dict)
    content = get_item_content(args.target_cpu, syscall_nr_list, nr_to_func_dict)

    audit_policy.gen_output_file(args.filter_name, content)
    remove_disassemble_file('.')


def main():
    parser = argparse.ArgumentParser(
      description='Generates a seccomp-bpf policy')
    parser.add_argument('--src-elf-path', type=str,
                        help='the drectory of the elf file')
    parser.add_argument('--elf-name', action='append',
                        help='path to syscall to nr files')
    parser.add_argument('--src-syscall-path', type=str, action='append',
                        help=('path to syscall to nr files\n'))
    parser.add_argument('--target-cpu', type=str,
                        help='input arm or arm64 or riscv64')
    parser.add_argument('--filter-name', type=str,
                        help=('consist of output file name\n'))

    args = parser.parse_args()
    if args.target_cpu not in gen_policy.supported_architecture:
        raise ValueError("target_cpu must int {}".format(gen_policy.supported_architecture))

    collect_concrete_syscall(args)


if __name__ == '__main__':
    sys.exit(main())
