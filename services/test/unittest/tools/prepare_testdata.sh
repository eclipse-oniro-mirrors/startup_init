#! /bin/bash
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

if [ $# -lt 1 ];then
    echo "Usage $0 <ohos project rootpath>"
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

function hdc_push_cmd() {
    # do nothing if there are not any arguments
    if [ $# -ne 2 ];then
        return;
    fi
    echo "Pushing resouces to device"
    hdc file send $@
}

hdc target mount

ut_target_path="/data/init_ut"
echo "Remove ${ut_target_path}"
hdc_shell_cmd "rm -rf ${ut_target_path}"
hdc_shell_cmd "rm /bin/init_ut"

echo "Create ${ut_target_path}"
hdc_shell_cmd "umask 022"
hdc_shell_cmd "mkdir -p ${ut_target_path}"
hdc_shell_cmd "mkdir -p ${ut_target_path}/proc"

ohos_root="$1"
ohos_root=${ohos_root%%/}
ohos_init="${ohos_root}/base/startup/init_lite"
ohos_init_ut="${ohos_init}/services/test/unittest"

hdc_shell_cmd "mkdir -p ${ut_target_path}/coverage"
sleep 0.25

# copy file to test
hdc_shell_cmd "mkdir -p ${ut_target_path}/system"
hdc_shell_cmd "mkdir -p ${ut_target_path}/system/etc"
hdc_shell_cmd "mkdir -p ${ut_target_path}/system/etc/param"
hdc_shell_cmd "mkdir -p ${ut_target_path}/system/etc/param/ohos_const"
hdc_shell_cmd "mkdir -p ${ut_target_path}/vendor"
hdc_shell_cmd "mkdir -p ${ut_target_path}/vendor/etc"
hdc_shell_cmd "mkdir -p ${ut_target_path}/vendor/etc/param"

hdc_shell_cmd "cp ${ohos_root}/base/startup/init_lite/services/test/unittest/test_data/system/etc/param/ohos_const/* ${ut_target_path}/system/etc/param/ohos_const/"
hdc_push_cmd ${ohos_root}/base/startup/init_lite/services/test/unittest/test_data/system/etc/param/ohos.para ${ut_target_path}/system/etc/param/ohos.para
hdc_push_cmd ${ohos_root}/base/startup/init_lite/services/test/unittest/test_data/system/etc/param/ohos.para.dac ${ut_target_path}/system/etc/param/ohos.para.dac
hdc_push_cmd ${ohos_root}/base/startup/init_lite/services/test/unittest/test_data/system/etc/param/ohos.para.selinux ${ut_target_path}/system/etc/param/ohos.para.selinux

hdc_push_cmd ${ohos_root}/base/startup/init_lite/services/test/unittest/test_data/trigger_test.cfg  /data/init_ut/trigger_test.cfg
hdc_push_cmd ${ohos_root}/base/startup/init_lite/services/test/unittest/test_data/proc/cmdline  /data/init_ut/proc/cmdline

sleep 0.25
hdc file send ${ohos_root}/out/ohos-arm-release/exe.unstripped/tests/unittest/startup/init/init_ut /data/init_ut/init_ut
sleep 0.25
hdc_shell_cmd "cp /data/init_ut/init_ut /bin/init_ut"

hdc_shell_cmd "chmod 777 /data/init_ut/* -R"
hdc_shell_cmd "chmod 777 /bin/init_ut"

hdc_shell_cmd "export GCOV_PREFIX=${ut_target_path}/coverage&&export GCOV_PREFIX_STRIP=14&&init_ut"

if [ $? -ne 0 ]; then
    echo "Execute init_ut in device failed. please check the log"
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
find ${ohos_root}/out/ohos-arm-release/obj/ -name "*.gcno" -type f -exec cp {} . \;
if [ $? -ne 0 ]; then
    echo "find gcno failed."
    popd 2>&1 > /dev/null
    exit 1
fi

if [ ! -f ${ohos_init}/g.sh ]; then
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
lcov --remove init_ut_tmp.info "*third_party*" "*prebuilts*" "*test/unittest/*"  -o ${ohos_init}/init_ut.info

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
