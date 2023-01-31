#!/usr/bin/env python3
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

import sys
import argparse
import textwrap
import re
import os
import stat

supported_parse_item = ['arch', 'labelName', 'priority', 'allowList', 'blockList', 'priorityWithArgs',\
    'allowListWithArgs', 'headFiles', 'selfDefineSyscall', 'returnValue', 'mode']

function_name_nr_table_dict = {}

BPF_JGE = 'BPF_JUMP(BPF_JMP|BPF_JGE|BPF_K, {}, {}, {}),'
BPF_JGT = 'BPF_JUMP(BPF_JMP|BPF_JGT|BPF_K, {}, {}, {}),'
BPF_JEQ = 'BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, {}, {}, {}),'
BPF_JSET = 'BPF_JUMP(BPF_JMP|BPF_JSET|BPF_K, {}, {}, {}),'
BPF_JA = 'BPF_JUMP(BPF_JMP|BPF_JA, {}, 0, 0),'
BPF_LOAD = 'BPF_STMT(BPF_LD|BPF_W|BPF_ABS, {}),'
BPF_LOAD_MEM = 'BPF_STMT(BPF_LD|BPF_MEM, {}),'
BPF_ST = 'BPF_STMT(BPF_ST, {}),'
BPF_AND = 'BPF_STMT(BPF_ALU|BPF_AND|BPF_K, {}),'
BPF_RET_VALUE = 'BPF_STMT(BPF_RET|BPF_K, {}),'

operation = ['<', '<=', '!=', '==', '>', '>=', '&']

ret_str_to_bpf = {
    'KILL_PROCESS': 'SECCOMP_RET_KILL_PROCESS',
    'KILL_THREAD': 'SECCOMP_RET_KILL_THREAD',
    'LOG' : 'SECCOMP_RET_LOG',
    'ALLOW': 'SECCOMP_RET_ALLOW'
}

mode_str = {
    'DEFAULT': 0,
    'ONLY_CHECK_ARGS': 1
}


def is_hex_digit(s):
    try:
        int(s, 16)
        return True

    except ValueError:
        return False


def str_convert_to_int(s):
    number = -1
    digit_flag = False

    if s.isdigit() :
        number = int(s)
        digit_flag = True

    elif is_hex_digit(s):
        number = int(s, 16)
        digit_flag = True

    return number, digit_flag


class SeccompPolicyParam:
    def __init__(self, arch):
        self.arch = arch
        self.priority = set()
        self.allow_list = set()
        self.blocklist = set()
        self.priority_with_args = set()
        self.allow_list_with_args = set()
        self.head_files = set()
        self.self_define_syscall = set()
        self.final_allow_list = set()
        self.final_priority = set()
        self.final_priority_with_args = set()
        self.final_allow_list_with_args = set()
        self.return_value = ''
        self.mode = 'DEFAULT'
        self.function_name_nr_table = function_name_nr_table_dict.get(arch)
        self.value_function = {
            'priority': self.update_priority,
            'allowList': self.update_allow_list,
            'blockList': self.update_blocklist,
            'allowListWithArgs': self.update_allow_list_with_args,
            'priorityWithArgs': self.update_priority_with_args,
            'headFiles': self.update_head_files,
            'selfDefineSyscall': self.update_self_define_syscall,
            'returnValue': self.update_return_value,
            'mode': self.update_mode
        }

    def clear_list(self):
        self.priority.clear()
        self.allow_list.clear()
        self.blocklist.clear()
        self.allow_list_with_args.clear()
        self.priority_with_args.clear()
        if self.mode == 'ONLY_CHECK_ARGS':
            self.final_allow_list.clear()
            self.final_priority.clear()

    def function_name_to_nr(self, function_name_list):
        return set(self.function_name_nr_table[function_name] for function_name \
        in function_name_list if function_name in self.function_name_nr_table)

    def is_function_name_exist(self, function_name):
        if function_name in self.function_name_nr_table:
            return True
        else:
            print('[ERROR] {} not exsit in {} function_name_nr_table Table'.format(function_name, self.arch))
            return False

    def update_priority(self, function_name):
        if self.is_function_name_exist(function_name):
            self.priority.add(function_name)
            return True
        return False

    def update_allow_list(self, function_name):
        if self.is_function_name_exist(function_name):
            self.allow_list.add(function_name)
            return True
        return False

    def update_blocklist(self, function_name):
        if self.is_function_name_exist(function_name):
            self.blocklist.add(function_name)
            return True
        return False

    def update_priority_with_args(self, function_name_with_args):
        function_name = function_name_with_args[:function_name_with_args.find(':')]
        function_name = function_name.strip()
        if self.is_function_name_exist(function_name):
            self.priority_with_args.add(function_name_with_args)
            return True
        return False

    def update_allow_list_with_args(self, function_name_with_args):
        function_name = function_name_with_args[:function_name_with_args.find(':')]
        function_name = function_name.strip()
        if self.is_function_name_exist(function_name):
            self.allow_list_with_args.add(function_name_with_args)
            return True
        return False

    def update_head_files(self, head_files):
        if len(head_files) > 2 and (head_files[0] == '\"' and head_files[-1] == '\"') or \
            (head_files[0] == '<' and head_files[-1] == '>'):
            self.head_files.add(head_files)
            return True

        print('[ERROR] {} is not legal by headFiles format'.format(head_files))
        return False

    def update_self_define_syscall(self, self_define_syscall):
        nr, digit_flag = str_convert_to_int(self_define_syscall)
        if digit_flag and nr not in self.function_name_nr_table.values():
            self.self_define_syscall.add(nr)
            return True

        print("[ERROR] {} is not a number or {} is already used by ohter \
            syscall".format(self_define_syscall, self_define_syscall))
        return False

    def update_return_value(self, return_str):
        if return_str in ret_str_to_bpf:
            self.return_value = return_str
            return True

        print('[ERROR] {} not in [KILL_RPOCESS, KILL_THREAD, LOG]'.format(return_str))
        return False

    def update_mode(self, mode):
        if mode in mode_str.keys():
            self.mode = mode
            return True
        print('[ERROR] {} not in [DEFAULT, ONLY_CHECK_ARGS]'.format(mode_str))
        return False

    def update_final_list(self):
        #remove duplicate function_name
        self.priority_with_args = set(item
                                      for item in self.priority_with_args
                                      if item[:item.find(':')] not in self.blocklist)
        priority_function_name_list_with_args = set(item[:item.find(':')] for item in self.priority_with_args)
        self.allow_list_with_args = set(item
                                        for item in self.allow_list_with_args
                                        if item[:item.find(':')] not in self.blocklist and
                                        item[:item.find(':')] not in priority_function_name_list_with_args)
        function_name_list_with_args = set(item[:item.find(':')] for item in self.allow_list_with_args)

        self.final_allow_list |= self.allow_list - self.blocklist - self.priority - function_name_list_with_args - \
                                    priority_function_name_list_with_args
        self.final_priority |= self.priority - self.blocklist - function_name_list_with_args - \
                                priority_function_name_list_with_args
        self.final_allow_list_with_args |= self.allow_list_with_args
        self.final_priority_with_args |= self.priority_with_args
        block_nr_list = self.function_name_to_nr(self.blocklist)
        self.self_define_syscall = self.self_define_syscall - block_nr_list
        self.clear_list()


class GenBpfPolicy:
    def __init__(self):
        self.arch = ''
        self.syscall_nr_range = []
        self.bpf_policy = []
        self.syscall_nr_policy_list = []
        self.function_name_nr_table = {}
        self.gen_mode = 0
        self.flag = True
        self.return_value = ''
        self.operate_func_table = {
            '<' : self.gen_bpf_lt,
            '<=': self.gen_bpf_le,
            '==': self.gen_bpf_eq,
            '!=': self.gen_bpf_ne,
            '>' : self.gen_bpf_gt,
            '>=': self.gen_bpf_ge,
            '&' : self.gen_bpf_set,
        }

    def update_arch(self, arch):
        self.arch = arch
        self.function_name_nr_table = function_name_nr_table_dict.get(arch)
        self.syscall_nr_range = []
        self.syscall_nr_policy_list = []

    def clear_bpf_policy(self):
        self.bpf_policy.clear()

    def get_gen_flag(self):
        return self.flag

    def set_gen_flag(self, flag):
        if flag:
            self.flag = True
        else:
            self.flag = False

    def set_gen_mode(self, mode):
        self.gen_mode = mode_str.get(mode)

    def set_return_value(self, return_value):
        if return_value not in ret_str_to_bpf:
            self.set_gen_mode(False)
            return

        self.return_value = return_value

    @staticmethod
    def gen_bpf_eq32(const_str, jt, jf):
        bpf_policy = []
        bpf_policy.append(BPF_JEQ.format(const_str + ' & 0xffffffff', jt, jf))
        return bpf_policy

    @staticmethod
    def gen_bpf_eq64(const_str, jt, jf):
        bpf_policy = []
        bpf_policy.append(BPF_JEQ.format('((unsigned long)' + const_str + ') >> 32', 0, jf + 2))
        bpf_policy.append(BPF_LOAD_MEM.format(0))
        bpf_policy.append(BPF_JEQ.format(const_str + ' & 0xffffffff', jt, jf))
        return bpf_policy

    def gen_bpf_eq(self, const_str, jt, jf):
        if self.arch == 'arm':
            return self.gen_bpf_eq32(const_str, jt, jf)
        elif self.arch == 'arm64':
            return self.gen_bpf_eq64(const_str, jt, jf)
        return []

    def gen_bpf_ne(self, const_str, jt, jf):
        return self.gen_bpf_eq(const_str, jf, jt)

    @staticmethod
    def gen_bpf_gt32(const_str, jt, jf):
        bpf_policy = []
        bpf_policy.append(BPF_JGT.format(const_str + ' & 0xffffffff', jt, jf))
        return bpf_policy

    @staticmethod
    def gen_bpf_gt64(const_str, jt, jf):
        bpf_policy = []
        number, digit_flag = str_convert_to_int(const_str)

        hight = int(number / (2**32))
        low = number & 0xffffffff

        if digit_flag and hight == 0:
            bpf_policy.append(BPF_JGT.format('((unsigned long)' + const_str + ') >> 32', jt + 2, 0))
        else:
            bpf_policy.append(BPF_JGT.format('((unsigned long)' + const_str + ') >> 32', jt + 3, 0))
            bpf_policy.append(BPF_JEQ.format('((unsigned long)' + const_str + ') >> 32', 0, jf + 2))

        bpf_policy.append(BPF_LOAD_MEM.format(0))
        bpf_policy.append(BPF_JGT.format(const_str + ' & 0xffffffff', jt, jf))

        return bpf_policy

    def gen_bpf_gt(self, const_str, jt, jf):
        if self.arch == 'arm':
            return self.gen_bpf_gt32(const_str, jt, jf)
        elif self.arch == 'arm64':
            return self.gen_bpf_gt64(const_str, jt, jf)
        return []

    def gen_bpf_le(self, const_str, jt, jf):
        return self.gen_bpf_gt(const_str, jf, jt)

    @staticmethod
    def gen_bpf_ge32(const_str, jt, jf):
        bpf_policy = []
        bpf_policy.append(BPF_JGE.format(const_str + ' & 0xffffffff', jt, jf))
        return bpf_policy

    @staticmethod
    def gen_bpf_ge64(const_str, jt, jf):
        bpf_policy = []
        number, digit_flag = str_convert_to_int(const_str)

        hight = int(number / (2**32))
        low = number & 0xffffffff

        if digit_flag and hight == 0:
            bpf_policy.append(BPF_JGT.format('((unsigned long)' + const_str + ') >> 32', jt + 2, 0))
        else:
            bpf_policy.append(BPF_JGT.format('((unsigned long)' + const_str + ') >> 32', jt + 3, 0))
            bpf_policy.append(BPF_JEQ.format('((unsigned long)' + const_str + ') >> 32', 0, jf + 2))
        bpf_policy.append(BPF_LOAD_MEM.format(0))
        bpf_policy.append(BPF_JGE.format(const_str + ' & 0xffffffff', jt, jf))
        return bpf_policy

    def gen_bpf_ge(self, const_str, jt, jf):
        if self.arch == 'arm':
            return self.gen_bpf_ge32(const_str, jt, jf)
        elif self.arch == 'arm64':
            return self.gen_bpf_ge64(const_str, jt, jf)
        return []

    def gen_bpf_lt(self, const_str, jt, jf):
        return self.gen_bpf_ge(const_str, jf, jt)

    @staticmethod
    def gen_bpf_set32(const_str, jt, jf):
        bpf_policy = []
        bpf_policy.append(BPF_JSET.format(const_str + ' & 0xffffffff', jt, jf))
        return bpf_policy

    @staticmethod
    def gen_bpf_set64(const_str, jt, jf):
        bpf_policy = []
        bpf_policy.append(BPF_JSET.format('((unsigned long)' + const_str + ') >> 32', jt + 2, 0))
        bpf_policy.append(BPF_LOAD_MEM.format(0))
        bpf_policy.append(BPF_JSET.format(const_str + ' & 0xffffffff', jt, jf))
        return bpf_policy

    def gen_bpf_set(self, const_str, jt, jf):
        if self.arch == 'arm':
            return self.gen_bpf_set32(const_str, jt, jf)
        elif self.arch == 'arm64':
            return self.gen_bpf_set64(const_str, jt, jf)
        return []

    @staticmethod
    def gen_bpf_valid_syscall_nr(syscall_nr, cur_size):
        bpf_policy = []
        bpf_policy.append(BPF_LOAD.format(0))
        bpf_policy.append(BPF_JEQ.format(syscall_nr, 0, cur_size))
        return bpf_policy

    def gen_range_list(self, syscall_nr_list):
        if len(syscall_nr_list) == 0:
            return
        self.syscall_nr_range.clear()

        syscall_nr_list_order = sorted(list(syscall_nr_list))
        range_temp = [syscall_nr_list_order[0], syscall_nr_list_order[0]]

        for i in range(len(syscall_nr_list_order) - 1):
            if syscall_nr_list_order[i + 1] != syscall_nr_list_order[i] + 1:
                range_temp[1] = syscall_nr_list_order[i]
                self.syscall_nr_range.append(range_temp)
                range_temp = [syscall_nr_list_order[i + 1], syscall_nr_list_order[i + 1]]

        range_temp[1] = syscall_nr_list_order[-1]
        self.syscall_nr_range.append(range_temp)

    def gen_policy_syscall_nr(self, min_index, max_index, cur_syscall_nr_range):
        middle_index = (int)((min_index + max_index + 1) / 2)

        if middle_index == min_index:
            self.syscall_nr_policy_list.append(cur_syscall_nr_range[middle_index][1] + 1)
            return
        else:
            self.syscall_nr_policy_list.append(cur_syscall_nr_range[middle_index][0])

        self.gen_policy_syscall_nr(min_index, middle_index - 1, cur_syscall_nr_range)
        self.gen_policy_syscall_nr(middle_index, max_index, cur_syscall_nr_range)

    def gen_policy_syscall_nr_list(self, cur_syscall_nr_range):
        if not cur_syscall_nr_range:
            return
        self.syscall_nr_policy_list.clear()
        self.syscall_nr_policy_list.append(cur_syscall_nr_range[0][0])
        self.gen_policy_syscall_nr(0, len(cur_syscall_nr_range) - 1, cur_syscall_nr_range)

    def calculate_step(self, index):
        for i in range(index + 1, len(self.syscall_nr_policy_list)):
            if self.syscall_nr_policy_list[index] < self.syscall_nr_policy_list[i]:
                step = i - index
                break
        return step - 1

    def nr_range_to_bpf_policy(self, cur_syscall_nr_range):
        self.gen_policy_syscall_nr_list(cur_syscall_nr_range)
        syscall_list_len  = len(self.syscall_nr_policy_list)

        if syscall_list_len == 0:
            return

        self.bpf_policy.append(BPF_JGE.format(self.syscall_nr_policy_list[0], 0, syscall_list_len))

        range_max_list = [k[1] for k in cur_syscall_nr_range]

        for i in range(1, syscall_list_len):
            if self.syscall_nr_policy_list[i] - 1 in range_max_list:
                self.bpf_policy.append(BPF_JGE.format(self.syscall_nr_policy_list[i], \
                                        syscall_list_len - i, syscall_list_len - i - 1))
            else:
                step = self.calculate_step(i)
                self.bpf_policy.append(BPF_JGE.format(self.syscall_nr_policy_list[i], step, 0))

        if self.syscall_nr_policy_list:
            self.bpf_policy.append(BPF_RET_VALUE.format('SECCOMP_RET_ALLOW'))

    def count_alone_range(self):
        cnt = 0
        for item in self.syscall_nr_range:
            if item[0] == item[1]:
                cnt = cnt + 1
        return cnt

    def gen_transverse_bpf_policy(self):
        if not self.syscall_nr_range:
            return
        cnt = self.count_alone_range()
        total_instruction_num = cnt + (len(self.syscall_nr_range) - cnt) * 2
        i = 0
        for item in self.syscall_nr_range:
            if item[0] == item[1]:
                if i == total_instruction_num - 1:
                    self.bpf_policy.append(BPF_JEQ.format(item[0], total_instruction_num - i - 1, 1))
                else:
                    self.bpf_policy.append(BPF_JEQ.format(item[0], total_instruction_num - i - 1, 0))
                i += 1
            else:
                self.bpf_policy.append(BPF_JGE.format(item[0], 0, total_instruction_num - i))
                i += 1
                if i == total_instruction_num - 1:
                    self.bpf_policy.append(BPF_JGE.format(item[1] + 1, 1, total_instruction_num - i - 1))
                else:
                    self.bpf_policy.append(BPF_JGE.format(item[1] + 1, 0, total_instruction_num - i - 1))
                i += 1

        self.bpf_policy.append(BPF_RET_VALUE.format('SECCOMP_RET_ALLOW'))

    def gen_bpf_policy(self, syscall_nr_list):
        self.gen_range_list(syscall_nr_list)
        range_size = (int)((len(self.syscall_nr_range) - 1) / 127) + 1
        alone_range_cnt = self.count_alone_range()
        if alone_range_cnt == len(self.syscall_nr_range):
            #Scattered distribution
            self.gen_transverse_bpf_policy()
            return

        if range_size == 1:
            self.nr_range_to_bpf_policy(self.syscall_nr_range)
        else:
            for i in range(0, range_size):
                if i == 0:
                    self.nr_range_to_bpf_policy(self.syscall_nr_range[-127 * (i + 1):])
                elif i == range_size - 1:
                    self.nr_range_to_bpf_policy(self.syscall_nr_range[:-127 * i])
                else:
                    self.nr_range_to_bpf_policy(self.syscall_nr_range[-127 * (i + 1): -127 * i])

    def load_arg(self, arg_id):
        # little endian
        bpf_policy = []
        if self.arch == 'arm':
            bpf_policy.append(BPF_LOAD.format(16 + arg_id * 8))
        elif self.arch == 'arm64':
            #low 4 bytes
            bpf_policy.append(BPF_LOAD.format(16 + arg_id * 8))
            bpf_policy.append(BPF_ST.format(0))
            #high 4 bytes
            bpf_policy.append(BPF_LOAD.format(20 + arg_id * 8))
            bpf_policy.append(BPF_ST.format(1))

        return bpf_policy

    def compile_atom(self, atom, cur_size):
        bpf_policy = []
        if len(atom) < 6:
            print('[ERROR] {} format ERROR '.format(atom))
            self.flag = False
            return bpf_policy

        if atom[0] == '(':
            bpf_policy += self.compile_mask_equal_atom(atom, cur_size)
        else:
            bpf_policy += self.compile_single_operation_atom(atom, cur_size)

        return bpf_policy
    
    def check_arg_str(self, arg_atom):
        arg_str = arg_atom[0:3]
        if arg_str != 'arg':
            print('[ERROR] format ERROR, {} is not equal to arg'.format(arg_atom))
            self.flag = False
            return -1, False

        arg_id = int(arg_atom[3])
        if arg_id not in range(6):
            print('[ERROR] arg num out of the scope 0~5')
            self.flag = False
            return -1, False

        return arg_id, True

    def check_operation_str(self, operation_atom):
        operation_str = operation_atom
        if operation_str not in operation:
            operation_str = operation_atom[0]
            if operation_str not in operation:
                print('[ERROR] operation not in [<, <=, !=, ==, >, >=, &]')
                self.flag = False
                return '', False
        return operation_str, True

    #gen bpf (argn & mask) == value
    @staticmethod
    def gen_mask_equal_bpf(arg_id, mask, value, cur_size):
        bpf_policy = []
        #high 4 bytes
        bpf_policy.append(BPF_LOAD.format(20 + arg_id * 8))
        bpf_policy.append(BPF_AND.format('((uint64_t)' + mask + ') >> 32'))
        bpf_policy.append(BPF_JEQ.format('((uint64_t)' + value + ') >> 32', 0, cur_size + 3))

        #low 4 bytes
        bpf_policy.append(BPF_LOAD.format(16 + arg_id * 8))
        bpf_policy.append(BPF_AND.format(mask))
        bpf_policy.append(BPF_JEQ.format(value, cur_size, cur_size + 1))

        return bpf_policy

    def compile_mask_equal_atom(self, atom, cur_size):
        bpf_policy = []
        left_brace_pos = atom.find('(')
        right_brace_pos = atom.rfind(')')
        inside_brace_content = atom[left_brace_pos + 1: right_brace_pos]
        outside_brace_content = atom[right_brace_pos + 1:]

        arg_res = self.check_arg_str(inside_brace_content[0:4])
        if not arg_res[1]:
            return bpf_policy

        operation_res_inside = self.check_operation_str(inside_brace_content[4:6])
        if operation_res_inside[0] != '&' or not operation_res_inside[1]:
            return bpf_policy

        mask = inside_brace_content[4 + len(operation_res_inside[0]):]

        operation_res_outside = self.check_operation_str(outside_brace_content[0:2])
        if operation_res_outside[0] != '==' or not operation_res_outside[1]:
            return bpf_policy

        value = outside_brace_content[len(operation_res_outside[0]):]

        return self.gen_mask_equal_bpf(arg_res[0], mask, value, cur_size)

    def compile_single_operation_atom(self, atom, cur_size):
        bpf_policy = []
        arg_res = self.check_arg_str(atom[0:4])
        if not arg_res[1]:
            return bpf_policy

        operation_res = self.check_operation_str(atom[4:6])
        if not operation_res[1]:
            return bpf_policy

        const_str = atom[4 + len(operation_res[0]):]

        if not const_str:
            return bpf_policy

        bpf_policy += self.load_arg(arg_res[0])
        bpf_policy += self.operate_func_table.get(operation_res[0])(const_str, 0, cur_size + 1)

        return bpf_policy

    def parse_args_with_condition(self, group):
        #the priority of && higher than ||
        atoms = group.split('&&')
        bpf_policy = []
        for atom in reversed(atoms):
            bpf_policy = self.compile_atom(atom, len(bpf_policy)) + bpf_policy
        return bpf_policy

    def parse_sub_group(self, group):
        bpf_policy = []
        group_info = group.split(';')
        operation_part = group_info[0]
        return_part = group_info[1]
        if not return_part.startswith('return'):
            self.set_gen_flag(False)
            return bpf_policy
        self.set_return_value(return_part[len('return'):])
        and_cond_groups = operation_part.split('||')
        for and_condition_group in and_cond_groups:
            bpf_policy += self.parse_args_with_condition(and_condition_group)
            bpf_policy.append(BPF_RET_VALUE.format(ret_str_to_bpf.get(self.return_value)))
        return bpf_policy

    def parse_else_part(self, else_part):
        return_value = else_part.split(';')[0][else_part.find('return') + len('return'):]
        self.set_return_value(return_value)

    def parse_args(self, function_name, line, skip):
        bpf_policy = []
        group_info  = line.split('else')
        else_part = group_info[-1]
        group = group_info[0].split('elif')
        for sub_group in group:
            bpf_policy += self.parse_sub_group(sub_group)
        self.parse_else_part(else_part)
        bpf_policy.append(BPF_RET_VALUE.format(ret_str_to_bpf.get(self.return_value)))
        syscall_nr = self.function_name_nr_table.get(function_name)
        #load syscall nr
        bpf_policy = self.gen_bpf_valid_syscall_nr(syscall_nr, len(bpf_policy) - skip)  + bpf_policy
        return bpf_policy

    def gen_bpf_policy_with_args(self, allow_list_with_args, mode, return_value):
        self.set_gen_mode(mode)
        skip = 0
        for line in allow_list_with_args:
            if self.gen_mode == 1 and line == list(allow_list_with_args)[-1]:
                skip = 2
            line = line.replace(' ', '')
            pos = line.find(':')
            function_name = line[:pos]

            left_line = line[pos + 1:]
            if not left_line.startswith('if'):
                continue

            self.bpf_policy += self.parse_args(function_name, left_line[2:], skip)

    def add_load_syscall_nr(self):
        self.bpf_policy.append(BPF_LOAD.format(0))

    def add_return_value(self, return_value):
        self.bpf_policy.append(BPF_RET_VALUE.format(ret_str_to_bpf.get(return_value)))

    def add_validate_arch(self, arches, skip_step):
        if not self.bpf_policy or not self.flag:
            return
        bpf_policy = []
        #load arch
        bpf_policy.append(BPF_LOAD.format(4))
        if len(arches) == 2:
            bpf_policy.append(BPF_JEQ.format('AUDIT_ARCH_AARCH64', 3, 0))
            bpf_policy.append(BPF_JEQ.format('AUDIT_ARCH_ARM', 0, 1))
            bpf_policy.append(BPF_JA.format(skip_step))
            bpf_policy.append(BPF_RET_VALUE.format('SECCOMP_RET_KILL_PROCESS'))
        elif 'arm' in arches:
            bpf_policy.append(BPF_JEQ.format('AUDIT_ARCH_ARM', 1, 0))
            bpf_policy.append(BPF_RET_VALUE.format('SECCOMP_RET_KILL_PROCESS'))
        elif 'arm64' in arches:
            bpf_policy.append(BPF_JEQ.format('AUDIT_ARCH_AARCH64', 1, 0))
            bpf_policy.append(BPF_RET_VALUE.format('SECCOMP_RET_KILL_PROCESS'))
        else:
            self.bpf_policy = []

        self.bpf_policy = bpf_policy + self.bpf_policy


class SeccompPolicyParser:
    def __init__(self):
        self.cur_parse_item = ''
        self.cur_arch = ''
        self.arches = set()
        self.bpf_generator = GenBpfPolicy()
        self.seccomp_policy_param_arm = None
        self.seccomp_policy_param_arm64 = None
        self.cur_policy_param = None

    def update_arch(self, arch):
        if arch in ['arm', 'arm64'] :
            self.cur_arch = arch
            print("[INFO] start deal with {} scope".format(self.cur_arch))
            self.arches.add(arch)
            if self.cur_arch == 'arm':
                self.cur_policy_param = self.seccomp_policy_param_arm
            elif self.cur_arch == 'arm64':
                self.cur_policy_param = self.seccomp_policy_param_arm64
        else:
            print('[ERROR] {} not in [arm arm64]'.format(arch))
            self.bpf_generator.set_gen_flag(False)

    def update_parse_item(self, line):
        item = line[1:]
        if item in supported_parse_item:
            self.cur_parse_item = item
            print('[INFO] start deal with {}'.format(self.cur_parse_item))

    def clear_file_syscall_list(self):
        self.seccomp_policy_param_arm.update_final_list()
        self.seccomp_policy_param_arm64.update_final_list()
        self.cur_parse_item = ''
        self.cur_arch = ''

    def parse_line(self, line):
        if not self.cur_parse_item :
            return
        if self.cur_parse_item == 'arch':
            self.update_arch(line)
        elif not self.cur_arch :
            return
        else:
            if not self.cur_policy_param.value_function.get(self.cur_parse_item)(line):
                self.bpf_generator.set_gen_flag(False)

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
            if line[0] != '@' and self.cur_arch == '' and self.cur_parse_item == '':
                continue
            self.parse_line(line)
        self.clear_file_syscall_list()

    def parse_file(self, file_path):
        with open(file_path) as fp:
            self.parse_open_file(fp)

    def gen_seccomp_policy_of_arch(self):
        if not self.cur_policy_param.return_value:
            print('[ERROR] return value not defined')
            self.bpf_generator.set_gen_flag(False)
            return

        #get final allow_list
        syscall_nr_allow_list = self.cur_policy_param.function_name_to_nr(self.cur_policy_param.final_allow_list) | \
            self.cur_policy_param.self_define_syscall
        syscall_nr_priority = self.cur_policy_param.function_name_to_nr(self.cur_policy_param.final_priority)
        self.bpf_generator.update_arch(self.cur_arch)

        #load syscall nr
        if syscall_nr_allow_list or syscall_nr_priority:
            self.bpf_generator.add_load_syscall_nr()
        self.bpf_generator.gen_bpf_policy(syscall_nr_priority)
        self.bpf_generator.gen_bpf_policy_with_args(self.cur_policy_param.final_priority_with_args, \
            self.cur_policy_param.mode, self.cur_policy_param.return_value)
        self.bpf_generator.gen_bpf_policy(syscall_nr_allow_list)
        self.bpf_generator.gen_bpf_policy_with_args(self.cur_policy_param.final_allow_list_with_args, \
            self.cur_policy_param.mode, self.cur_policy_param.return_value)

        self.bpf_generator.add_return_value(self.cur_policy_param.return_value)

    def gen_seccomp_policy(self):
        if 'arm64' in self.arches:
            self.update_arch('arm64')
            self.gen_seccomp_policy_of_arch()

        skip_step = len(self.bpf_generator.bpf_policy) + 1

        if 'arm' in self.arches:
            self.update_arch('arm')
            self.gen_seccomp_policy_of_arch()
        self.bpf_generator.add_validate_arch(self.arches, skip_step)

    def gen_output_file(self, args):
        if not self.bpf_generator.bpf_policy:
            print("[ERROR] bpf_policy is empty!")
            return

        header = textwrap.dedent('''\

            #include <linux/filter.h>
            #include <stddef.h>
            #include <linux/seccomp.h>
            #include <linux/audit.h>
            ''')
        extra_header = set()
        extra_header = self.seccomp_policy_param_arm.head_files | self.seccomp_policy_param_arm64.head_files
        extra_header_list =  ['#include ' + i for i in extra_header]

        array_name = textwrap.dedent('''

            const struct sock_filter {}[] = {{
            ''').format(args.bpfArrayName)

        footer = textwrap.dedent('''\

            }};

            const size_t {} = sizeof({}) / sizeof(struct sock_filter);
            ''').format(args.bpfArrayName + 'Size', args.bpfArrayName)

        content = header + '\n'.join(extra_header_list) + array_name + \
            '    ' + '\n    '.join(self.bpf_generator.bpf_policy) + footer

        
        flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC
        modes = stat.S_IWUSR | stat.S_IRUSR | stat.S_IWGRP | stat.S_IRGRP
        with os.fdopen(os.open(args.dstfile, flags, modes), 'w') as output_file:
            output_file.write(content)

    @staticmethod
    def filter_syscalls_nr(name_to_nr):
        syscalls = {}
        for syscall_name, nr in name_to_nr.items():
            if not syscall_name.startswith("__NR_") and not syscall_name.startswith("__ARM_NR_"):
                continue

            if syscall_name.startswith("__NR_arm_"):
                syscall_name = syscall_name[len("__NR_arm_"):]
            elif syscall_name.startswith("__NR_"):
                syscall_name = syscall_name[len("__NR_"):]
            elif syscall_name.startswith("__ARM_NR_"):
                syscall_name = syscall_name[len("__ARM_NR_"):]

            syscalls[syscall_name] = nr

        return syscalls

    def parse_syscall_file(self, file_name):
        const_pattern = re.compile(
            r'^\s*#define\s+([A-Za-z_][A-Za-z0-9_]+)\s+(.+)\s*$')
        mark_pattern = re.compile(r'\b[A-Za-z_][A-Za-z0-9_]+\b')
        name_to_nr = {}
        with open(file_name) as f:
            for line in f:
                k = const_pattern.match(line)
                if k is None:
                    continue
                try:
                    name = k.group(1)
                    nr = eval(mark_pattern.sub(lambda x: str(name_to_nr.get(x.group(0))),
                                            k.group(2)))

                    name_to_nr[name] = nr
                except(KeyError, SyntaxError, NameError, TypeError):
                    continue

        return self.filter_syscalls_nr(name_to_nr)

    def gen_syscall_nr_table(self, file_name):
        s = re.search(r"libsyscall_to_nr_([^/]+)", file_name)
        function_name_nr_table_dict[str(s.group(1))] = self.parse_syscall_file(file_name)
        if str(s.group(1)) not in function_name_nr_table_dict.keys():
            return False
        return True

    def gen_seccomp_policy_code(self, args):
        for file_name in args.srcfiles:
            file_name_tmp = file_name.split('/')[-1]
            if not file_name_tmp.lower().startswith('libsyscall_to_nr_'):
                continue
            if not self.gen_syscall_nr_table(file_name):
                return
        self.seccomp_policy_param_arm = SeccompPolicyParam('arm')
        self.seccomp_policy_param_arm64 = SeccompPolicyParam('arm64')
        for file_name in args.srcfiles:
            if file_name.lower().endswith('.policy'):
                self.parse_file(file_name)
        self.gen_seccomp_policy()
        if self.bpf_generator.get_gen_flag():
            self.gen_output_file(args)


def main():
    parser = argparse.ArgumentParser(
      description='Generates a seccomp-bpf policy')
    parser.add_argument('--srcfiles', type=str, action='append',
                        help=('The input files\n'))
    parser.add_argument('--dstfile',
                        help='The output path for the policy files')

    parser.add_argument('--bpfArrayName',  type=str,
                        help='Name of seccomp bpf array generated by this script')

    args = parser.parse_args()

    generator = SeccompPolicyParser()
    generator.gen_seccomp_policy_code(args)


if __name__ == '__main__':
    sys.exit(main())
