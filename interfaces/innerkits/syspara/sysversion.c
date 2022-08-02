/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "sysversion.h"

#include <stdlib.h>
#include <string.h>

#include "beget_ext.h"
#include "param_comm.h"
#include "securec.h"

/* *
 * Major(M) version number.
 */
static int g_majorVersion = 2;

/* *
 * Senior(S) version number.
 */
static int g_seniorVersion = 2;

/* *
 * Feature(F) version number.
 */
static int g_featureVersion = 0;

/* *
 * Build(B) version number.
 */
static int g_buildVersion = 0;

static void GetVersions(void)
{
    static int versionInited = 0;
    if (versionInited) {
        return;
    }
    const char *fullName = GetFullName_();
    if (fullName == NULL) {
        return;
    }
    const char *tmp = strstr(fullName, "-");
    if (tmp == NULL) {
        return;
    }
    tmp++; // skip "-"
    int ret = sscanf_s(tmp, "%d.%d.%d.%d", &g_majorVersion, &g_seniorVersion, &g_featureVersion, &g_buildVersion);
    BEGET_LOGV("fullName %s %d.%d.%d.%d ret %d",
        fullName, g_majorVersion, g_seniorVersion, g_featureVersion, g_buildVersion, ret);
    if (ret == 4) { // 4 parameters
        versionInited = 1;
    }
}

/* *
 * Obtains the major (M) version number, which increases with any updates to the overall architecture.
 * <p>The M version number monotonically increases from 1 to 99.
 *
 * @return Returns the M version number.
 * @since 4
 */
int GetMajorVersion(void)
{
    GetVersions();
    return g_majorVersion;
}

/* *
 * Obtains the senior (S) version number, which increases with any updates to the partial
 * architecture or major features.
 * <p>The S version number monotonically increases from 0 to 99.
 *
 * @return Returns the S version number.
 * @since 4
 */
int GetSeniorVersion(void)
{
    GetVersions();
    return g_seniorVersion;
}

/* *
 * Obtains the feature (F) version number, which increases with any planned new features.
 * <p>The F version number monotonically increases from 0 or 1 to 99.
 *
 * @return Returns the F version number.
 * @since 3
 */
int GetFeatureVersion(void)
{
    GetVersions();
    return g_featureVersion;
}

/* *
 * Obtains the build (B) version number, which increases with each new development build.
 * <p>The B version number monotonically increases from 0 or 1 to 999.
 *
 * @return Returns the B version number.
 * @since 3
 */
int GetBuildVersion(void)
{
    GetVersions();
    return g_buildVersion;
}