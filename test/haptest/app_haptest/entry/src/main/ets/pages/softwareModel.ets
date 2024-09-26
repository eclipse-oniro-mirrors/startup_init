/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

import deviceInfo from '@ohos.deviceInfo';
import router from '@ohos.router';

@Entry
@Component
struct Index {
  @State DispalyText: string = '';
  @State TitleText: string = '显示内容';

  build() {
    Column() {
      Text()
        .width('100%')
        .height(100)
        .fontSize(24)
        .fontWeight(FontWeight.Bold)
        .margin({ left: '30vp'})

      Column() {
        Text(this.TitleText)
          .fontColor('#99182431')
          .fontSize($r('sys.float.ohos_id_text_size_sub_title3'))
          .margin({ top: '30vp', left: '24vp', bottom: '9.5vp' })
      }
      .width('100%')
      .height('60vp')
      .alignItems(HorizontalAlign.Start)

      Column() {
        Column() {
          Scroll() {
            Text(this.DispalyText)
              .fontColor('#182431')
              .fontSize($r('sys.float.ohos_id_text_size_body1'))
              .lineHeight('19vp')
              .textAlign(TextAlign.Start)
              .width('100%')
          }
          .margin({ top: '8vp', left: '16vp', right: '16vp', bottom: '21vp' })
        }
        .height('350vp')
        .backgroundColor('#FFFFFF')
        .borderRadius($r('sys.float.ohos_id_corner_radius_default_l'))
        .margin({ left: '12vp', right: '12vp' })
        .align(Alignment.TopStart)
      }
      .width('100%')
      .height('139vp')
      .alignItems(HorizontalAlign.Center)

      Row() {
        Button('CheckSysIntegrity', { type: ButtonType.Capsule, stateEffect: true })
          .borderRadius(8)
          .backgroundColor(0x317aff)
          .width('40%')
          .height('7%')
          .margin({ top: '10vp', left: '16vp', right: '16vp', bottom: '21vp' })
          .onClick(() => {
            this.TitleText = '软件型号';
            this.DispalyText = deviceInfo.softwareModel;
          })
        Button('CheckUrlThreat', { type: ButtonType.Capsule, stateEffect: true })
          .borderRadius(8)
          .backgroundColor(0x317aff)
          .width('40%')
          .height('7%')
          .margin({ top: '10vp', left: '16vp', right: '16vp', bottom: '21vp' })
          .onClick(() => {
            this.TitleText = '设备类型';
            this.DispalyText = deviceInfo.deviceType;
          })
      }
      .height('600vp')
      .width('100%')
    }
    .height('100%')
    .backgroundColor('#F1F3F5')
  }
}