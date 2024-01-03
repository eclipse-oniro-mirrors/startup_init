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
import generate_code_from_policy as gen_policy


class LibcFuncUnit:
    def __init__(self, arch, addr, func_name, nr):
        self.nr = set()
        self.nr |= nr
        self.func_name = func_name
        self.addr = addr
        self.use_function = set()
        self.arch = arch

    def merge_nr(self,  nr):
        self.nr |= nr

    def update_func_name(self, func_name):
        self.func_name = func_name

    def update_addr(self, addr):
        self.addr = addr

    def update_use_function(self, new_function):
        self.use_function.add(new_function)

    def print_info(self, name_nr_table_dict):
        keys = list(name_nr_table_dict.get(self.arch).keys())
        values = list(name_nr_table_dict.get(self.arch).values())
        nrs = [keys[values.index(nr_item)] for nr_item in self.nr]
        print('{}\t{}\t{} use function is {}'.format(self.addr, self.func_name, nrs, self.use_function))


def remove_head_zero(addr):
    pos = 0
    for ch in addr:
        if ch != '0':
            break
        pos += 1
    return addr[pos:]


def line_find_syscall_nr(line, nr_set, nr_last):
    nr = nr_last
    is_find_nr = False
    is_find_svc = True
    if ';' in line:
        nr_tmp, is_digit = gen_policy.str_convert_to_int(line[line.find('0x'):])
    else:
        nr_tmp, is_digit = gen_policy.str_convert_to_int(line[line.rfind('#') + 1:])
    if is_digit and 'movt' in line:
        nr = nr_tmp * 256 * 256
        return nr, is_find_nr, is_find_svc

    if is_digit and 'movw' in line:
        nr = nr + nr_tmp
        nr_tmp = nr
    nr = nr_tmp
    if is_digit:
        nr_set.add(nr)
        nr_tmp = 0
        nr = 0
        is_find_nr = True
        is_find_svc = False
    else:
        is_find_nr = False
        is_find_svc = False

    return nr, is_find_nr, is_find_svc


def get_direct_use_syscall_of_svc(arch, lines, func_list):
    is_find_nr = False
    is_find_svc = False
    nr_set = set()
    nr = 0
    if arch == 'arm':
        svc_reg = 'r7,'
        svc_reg1 = 'r7, '
    elif arch == 'arm64':
        svc_reg = 'x8,'
        svc_reg1 = 'w8,'
    elif arch == 'riscv64':
        svc_reg = 'x5,'
        svc_reg1 = 'x5,'
    for line in reversed(lines):
        line = line.strip()
        if not line:
            is_find_nr = False
            is_find_svc = False
            continue

        if not is_find_svc and ('svc\t' in line or 'svc ' in line):
            is_find_nr = False
            is_find_svc = True
            continue

        if  is_find_svc and 'mov' in line and (svc_reg in line or svc_reg1 in line):
            nr, is_find_nr, is_find_svc = line_find_syscall_nr(line, nr_set, nr)
            continue

        if is_find_nr  and line[-1] == ':':
            addr = line[:line.find(' ')]
            addr = remove_head_zero(addr)
            func_name = line[line.find('<') + 1: line.rfind('>')]
            func_list.append(LibcFuncUnit(arch, addr, func_name, nr_set))
            nr_set.clear()
            is_find_nr = False
            is_find_svc = False


def get_direct_use_syscall_of_syscall(arch, lines, func_list):
    is_find_syscall_nr = False
    is_find_syscall = False
    nr_tmp = set()
    addr_list = [func.addr for func in func_list]
    if arch == 'arm':
        syscall_reg = 'r0,'
        syscall_reg1 = 'r0,'
    elif arch == 'arm64':
        syscall_reg = 'x0,'
        syscall_reg1 = 'w0,'
    elif arch == 'riscv64':
        syscall_reg = 'x17,'
        syscall_reg1 = 'x17,'

    for line in reversed(lines):
        line = line.strip()
        if not line:
            is_find_syscall = False
            is_find_syscall_nr = False
            continue

        if not is_find_syscall and ('<syscall>' in line or '<__syscall_cp>' in line):
            is_find_syscall = True
            is_find_syscall_nr = False
            continue

        if is_find_syscall and 'mov' in line and (syscall_reg in line or syscall_reg1 in line):
            if ';' in line:
                nr, is_digit = gen_policy.str_convert_to_int(line[line.find('0x'):])
            else:
                nr, is_digit = gen_policy.str_convert_to_int(line[line.rfind('#') + 1:])
            if is_digit:
                nr_tmp.add(nr)
                is_find_syscall_nr = True
                is_find_syscall = False
            continue

        if is_find_syscall_nr  and line[-1] == ':':
            addr = line[:line.find(' ')]
            addr = remove_head_zero(addr)
            func_name = line[line.find('<') + 1: line.rfind('>')]

            try:
                inedx = addr_list.index(addr)
                func_list[inedx].merge_nr(nr_tmp)
            except(ValueError):
                func_list.append(LibcFuncUnit(arch, addr, func_name, nr_tmp))

            nr_tmp.clear()
            is_find_syscall_nr = False
            is_find_syscall = False


def get_direct_use_syscall(arch, lines):
    func_list = []
    get_direct_use_syscall_of_svc(arch, lines, func_list)
    get_direct_use_syscall_of_syscall(arch, lines, func_list)

    return func_list


def get_call_graph(arch, lines, func_list):
    is_find_function = False
    addr_list = [func.addr for func in func_list]
    for line in lines:
        line = line.strip()
        if not line:
            is_find_function = False
            continue
        if not is_find_function and '<' in line and '>:' in line:
            is_find_function = True
            caller_addr = line[:line.find(' ')]
            caller_addr = remove_head_zero(caller_addr)
            caller_func_name = line[line.find('<') + 1: line.rfind('>')]
            continue

        if is_find_function:
            line_info = line.split('\t')
            if len(line_info) < 4:
                continue

            if not ('b' in line_info[2] and '<' in line_info[3]):
                continue

            addr = line_info[3][:line_info[3].find(' ')]

            try:
                callee_inedx = addr_list.index(addr)
            except(ValueError):
                continue

            try:
                caller_inedx = addr_list.index(caller_addr)
                func_list[caller_inedx].merge_nr(func_list[callee_inedx].nr)
                func_list[caller_inedx].update_use_function(func_list[callee_inedx].func_name)
            except(ValueError):
                func_list.append(LibcFuncUnit(arch, caller_addr, caller_func_name, func_list[callee_inedx].nr))
                func_list[-1].update_use_function(func_list[callee_inedx].func_name)
                addr_list.append(caller_addr)


def parse_file(arch, file_name):
    with open(file_name) as fp:
        lines = fp.readlines()
        func_list = get_direct_use_syscall(arch, lines)
        func_list_old_len = len(func_list)
        func_list_new_len = -1

        while func_list_old_len != func_list_new_len:
            func_list_old_len = len(func_list)
            get_call_graph(arch, lines, func_list)
            func_list_new_len = len(func_list)

    return func_list


def get_syscall_map(arch, src_syscall_path, libc_path):
    function_name_nr_table_dict = {}
    for file_name in src_syscall_path:
        file_name_tmp = file_name.split('/')[-1]
        if not file_name_tmp.lower().startswith('libsyscall_to_nr_'):
            continue
        gen_policy.gen_syscall_nr_table(file_name, function_name_nr_table_dict)
    func_map = []
    if libc_path.lower().endswith('libc.asm'):
        func_map = parse_file(arch, libc_path)
    return func_map


def main():
    parser = argparse.ArgumentParser(
      description='Generates a seccomp-bpf policy')
    parser.add_argument('--src-syscall-path', type=str, action='append',
                        help=('The input files\n'))
    parser.add_argument('--libc-asm-path', type=str,
                        help=('The input files\n'))
    parser.add_argument('--target-cpu', type=str,
                        help=('The input files\n'))

    args = parser.parse_args()
    get_syscall_map(args.target_cpu, args.src_syscall_path, args.libc_asm_path)


if __name__ == '__main__':
    sys.exit(main())
