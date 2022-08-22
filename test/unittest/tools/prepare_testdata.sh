#!/bin/bash
# Copyright (c) 2021 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This Script used to push test data to devices
# Usage:
# ./prepare_testdata.sh path
# path is the rootdir of ohos projects.
set -e

if [ $# -lt 1 ];then
    echo "Usage $0 <product name>"
    echo "example $0 rk3568"
    exit 1
fi

function hdc_shell_cmd() {
    # do nothing if there are not any arguments
    if [ $# -eq 0 ];then
        return;
    fi
    echo "Running command $@"
    hdc shell $@
}

function get_root_dir() {
    local cur_path=$(pwd)
    while [ "${cur_path}" != "" ]
    do
        cur_path=${cur_path%/*}
        if [ "${cur_path}" == "" ];then
            echo "[error] get code root dir fail"
            exit 1
        fi
        if [ "$(basename ${cur_path})" == "base" ]; then
            ohos_root=${cur_path%/*}
            return
        fi
    done
}

get_root_dir
product_name=$1

if [ ! -d "${ohos_root}/out/${product_name}" ]; then
    echo "product ${product_name} not exist"
    exit 1
fi

hdc target mount
sleep 0.2
hdc_shell_cmd "mount -o remount,rw /"
ut_target_path="/data/init_ut"
echo "Remove ${ut_target_path}"
hdc_shell_cmd "rm -rf ${ut_target_path}"
hdc_shell_cmd "rm /bin/init_unittest"

echo "Create ${ut_target_path}"
hdc_shell_cmd "umask 022"
hdc_shell_cmd "mkdir -p ${ut_target_path}"
hdc_shell_cmd "mkdir -p ${ut_target_path}/proc"

ohos_init="${ohos_root}/base/startup"

hdc_shell_cmd "mkdir -p ${ut_target_path}/coverage"
sleep 0.25

# copy file to test
hdc file send ${ohos_root}/out/${product_name}/tests/unittest/startup/init/init_unittest /data/init_ut/init_unittest
sleep 0.25
hdc_shell_cmd "cp /data/init_ut/init_unittest /bin/init_unittest"

hdc_shell_cmd "chmod 777 /data/init_ut/* -R"
sleep 0.2
hdc_shell_cmd "chmod 777 /bin/init_unittest"

hdc_shell_cmd "export GCOV_PREFIX=${ut_target_path}/coverage&&export GCOV_PREFIX_STRIP=15&&init_unittest"
sleep 0.2

if [ $? -ne 0 ]; then
    echo "Execute init_unittest in device failed. please check the log"
fi
echo "Running init unittests end..."
echo "Ready to generate coverage..."
pushd ${ohos_init}
rm -rf ./g.sh
rm -rf *.gcno
rm -rf *.gcda
echo "Copy .gcta files to ${ohos_init}}"

for file in $(hdc_shell_cmd ls /data/init_ut/coverage/*.gcda); do
    hdc file recv ${file}  ${ohos_init}/${file:23}
    chmod 777 ${ohos_init}/${file:23}
done


echo "Find out all gcno files and copy to ${ohos_init}"
find ${ohos_root}/out/${product_name}/obj/base/startup/ -name "*.gcno" -type f -exec cp {} . \;
if [ $? -ne 0 ]; then
    echo "find gcno failed."
    popd 2>&1 > /dev/null
    exit 1
fi

if [ ! -f "${ohos_init}/g.sh" ]; then
    echo "create g.sh"
    touch ${ohos_init}/g.sh
    echo "${ohos_root}/prebuilts/clang/ohos/linux-x86_64/llvm/bin/llvm-cov gcov \$@" > ${ohos_init}/g.sh
    chmod 755 ${ohos_init}/g.sh
fi

echo "Running command lcov"
lcov -d . -o "${ohos_init}/init_ut_tmp.info" -b . -c --gcov-tool ${ohos_init}/g.sh

if [ $? -ne 0 ]; then
    echo "Run command lcov failed"
    popd 2>&1 > /dev/null
fi

echo "Filter out don\'t cared dir"
lcov --remove init_ut_tmp.info "*foundation*" "*init/adapter/init_adapter.c*" "*third_party*" \
    "*device.c*" "*prebuilts*" "*test/unittest/*"  "*utils/native/*" "*utils/system/*" \
    "*init.c*" "*init_signal_handler.c*" "*ueventd.c*" \
    "*ueventd_device_handler.c*" "*ueventd_firmware_handler.c*" "*ueventd_socket.c*" -o ${ohos_init}/init_ut.info

genhtml -o ${HOME}/init_coverage init_ut.info
if [ $? -ne 0 ]; then
    echo "Run genhtml failed."
    popd 2>&1 > /dev/null
    exit 1
fi
echo "Clear tmp files"
rm -rf ./g.sh *.gcno *.gcda init_ut.info init_ut_tmp.info
hdc_shell_cmd "rm -rf ${ut_target_path}"
echo
echo "Generate init ut coverage done."
echo "Check coverage in ${HOME}/init_coverage."
echo
popd 2>&1 > /dev/null
