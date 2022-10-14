/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <cstdlib>
#include <iostream>
#include <string>

#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "system_ability_load_callback_stub.h"

using namespace OHOS;
using namespace std;

class OnDemandLoadCallback : public SystemAbilityLoadCallbackStub {
public:
    void OnLoadSystemAbilitySuccess(int32_t systemAbilityId, const sptr<IRemoteObject>& remoteObject) override;
    void OnLoadSystemAbilityFail(int32_t systemAbilityId) override;
};

void OnDemandLoadCallback::OnLoadSystemAbilitySuccess(int32_t systemAbilityId,
    const sptr<IRemoteObject>& remoteObject)
{
    cout << "OnLoadSystemAbilitySuccess systemAbilityId:" << systemAbilityId << " IRemoteObject result:" <<
        ((remoteObject != nullptr) ? "succeed" : "failed") << endl;
}

void OnDemandLoadCallback::OnLoadSystemAbilityFail(int32_t systemAbilityId)
{
    cout << "OnLoadSystemAbilityFail systemAbilityId:" << systemAbilityId << endl;
}

int main(int argc, char * const argv[])
{
    std::map<string, int> saService = {
        {"updater_sa", UPDATE_DISTRIBUTED_SERVICE_ID},
        {"softbus_server", SOFTBUS_SERVER_SA_ID},
    };
    int parameterNum = 2;
    if ((argc != parameterNum) || (argv[1] == nullptr)) {
        cout << "Invalid parameter" << endl;
        return 0;
    }
    const string name = argv[1];
    int abilityId = 0;
    std::map<string, int>::iterator item = saService.find(name);
    if (item != saService.end()) {
        cout << "sa service name " << item->first << "ability id " << item->second << endl;
        abilityId = item->second;
    } else {
        cout << "Invalid sa service name" << endl;
        return 0;
    }

    sptr<OnDemandLoadCallback> loadCallback_ = new OnDemandLoadCallback();
    sptr<ISystemAbilityManager> sm = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (sm == nullptr) {
        cout << "GetSystemAbilityManager samgr object null!" << endl;
        return 0;
    }
    int32_t result = sm->LoadSystemAbility(abilityId, loadCallback_);
    if (result != ERR_OK) {
        cout << "systemAbilityId:" << abilityId << " load failed, result code:" << result << endl;
        return 0;
    }
    return 0;
}

