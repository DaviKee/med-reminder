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
import { ArkTsAttribute, cordovaWebTagToObjectGlobe, NativeAttribute } from "../PluginGlobal";
import capacitor from 'libcapacitor.so';
import {PromptActionClass} from './PromptActionClass'
import { MainPage } from "../MainPage";
import { ComponentContent } from "@kit.ArkUI";

export function ExecArtTs(pageIndex:NativeAttribute):void {
  capacitor.onArkTsResult(JSON.stringify({ ExecArtTs:"success" }), pageIndex.pageObject, pageIndex.pageWebTag);
}

export function ExecArtTsAsync(pageIndex:NativeAttribute):void {
  capacitor.onArkTsResult(JSON.stringify({ ExecArtTsAsync:"success" }), pageIndex.pageObject, pageIndex.pageWebTag);
}
