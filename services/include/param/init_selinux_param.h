/* Copyright (c) 2021 北京万里红科技有限公司
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BASE_STARTUP_INIT_SELINUX_PARAM_H
#define BASE_STARTUP_INIT_SELINUX_PARAM_H

#ifdef __cplusplus
# if __cplusplus
extern "C" {
# endif // __cplusplus
#endif // __cplusplus

# define SECON_STR_IN_CFG      ("secon")
// https://github.com/xelerance/Openswan/blob/86dff2b/include/pluto/state.h#L222
# define MAX_SECON_LEN         (257)

#ifdef __cplusplus
# if __cplusplus
}
# endif // __cplusplus
#endif // __cplusplus

#endif // BASE_STARTUP_INIT_SELINUX_PARAM_H
