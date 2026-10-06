/*
 * The MIT License (MIT)
 * Copyright (C) 2026 Huawei Device Co., Ltd and iSoftStone Information Technology(Group)Co.,Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */
import { cordovaWebTagToObjectGlobe, NativeAttribute } from "../PluginGlobal";
import { Want } from "@kit.AbilityKit";
import { MainPage } from "../MainPage";
import { NormalizeError } from "../NormalizeError";

//重新启动
export function ReStartApp(pageIndex:NativeAttribute) {
  try {
    if (!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
      return;
    }
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
    let applicationContext =  mainPage.getUIContext().getHostContext()!.getApplicationContext().getApplicationContext();
    let want: Want = {
      bundleName: pageIndex.pageArgs,
      abilityName: 'EntryAbility'
    };
    try {
      applicationContext.restartApp(want);
    } catch (error) {
      console.error(`restartApp fail, error: ${JSON.stringify(error)}`);
    }
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`Failed to ReStartApp. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
  }
}
