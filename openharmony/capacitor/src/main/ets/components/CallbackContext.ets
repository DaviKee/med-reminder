/*
 * Copyright (c) 2025 Huawei Device, Inc. Ltd. and <马弓手>.
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

import { MessageStatus, PluginResult } from "./PluginResult"
import capacitor from 'libcapacitor.so';

interface CbcAttribute{
  callbackId:string;
  status:number;
  messageType:number;
  message:string;
  keepCallback:boolean;
}
export class CallbackContext {
  private webTag:string;
  private objectName:string;
  private callbackId:string;

  constructor(webTag:string, objectName:string, callbackId:string){
    this.webTag = webTag;
    this.objectName = objectName;
    this.callbackId = callbackId;
  }

  sendPluginResult(pluginResult:PluginResult) {
    let result: CbcAttribute = { callbackId: this.callbackId, status: pluginResult.getStatus(), messageType:pluginResult.getMessageType(), message:pluginResult.getMessage(), keepCallback:pluginResult.getKeepCallback()};
    capacitor.onArkTsResult(JSON.stringify(result), this.objectName, this.webTag);
  }

  successByJson(message:object, keepCallback?:boolean):void {
    let pluginResult:PluginResult = PluginResult.createByJson(MessageStatus.OK, message);
    if(keepCallback) {
      pluginResult.setKeepCallback(true);
    }
    this.sendPluginResult(pluginResult);
  }
  successByJsonString(message:string, keepCallback?:boolean):void {
    let pluginResult:PluginResult = PluginResult.createByJsonString(MessageStatus.OK, message);
    if(keepCallback) {
      pluginResult.setKeepCallback(true);
    }
    this.sendPluginResult(pluginResult);
  }
  successByString(message:string, keepCallback?:boolean):void {
    let pluginResult:PluginResult = PluginResult.createByString(MessageStatus.OK, message);
    if(keepCallback) {
      pluginResult.setKeepCallback(true);
    }
    this.sendPluginResult(pluginResult);
  }
  successByArrayBuffer(message:Uint8Array, keepCallback?:boolean):void {
    let pluginResult:PluginResult = PluginResult.createByArrayBuffer(MessageStatus.OK, message);
    if(keepCallback) {
      pluginResult.setKeepCallback(true);
    }
    this.sendPluginResult(pluginResult);
  }
  successByBinaryString(message:Uint8Array, keepCallback?:boolean):void {
    let pluginResult:PluginResult = PluginResult.createByBinaryString(MessageStatus.OK, message);
    if(keepCallback) {
      pluginResult.setKeepCallback(true);
    }
    this.sendPluginResult(pluginResult);
  }
  successByNumber(message:number, keepCallback?:boolean):void {
    let pluginResult:PluginResult = PluginResult.createByNumber(MessageStatus.OK, message);
    if(keepCallback) {
      pluginResult.setKeepCallback(true);
    }
    this.sendPluginResult(pluginResult);
  }
  success(keepCallback?:boolean):void {
    let pluginResult:PluginResult = PluginResult.create(MessageStatus.OK);
    if(keepCallback) {
      pluginResult.setKeepCallback(true);
    }
    this.sendPluginResult(pluginResult);
  };
  successByBoolean(message:boolean, keepCallback?:boolean):void {
    let pluginResult:PluginResult = PluginResult.createByBoolean(MessageStatus.OK, message);
    if(keepCallback) {
      pluginResult.setKeepCallback(true);
    }
    this.sendPluginResult(pluginResult);
  }
  successByNull(keepCallback?:boolean):void {
    let pluginResult:PluginResult = PluginResult.createByNull(MessageStatus.OK);
    if(keepCallback) {
      pluginResult.setKeepCallback(true);
    }
    this.sendPluginResult(pluginResult);
  }
  successByMultipart(pluginResults:Array<PluginResult>):void {
    for(let i=0; i<pluginResults.length;i++) {
      if(i != pluginResults.length-1) {
        pluginResults[i].setKeepCallback(true);
      } else {
        pluginResults[i].setKeepCallback(false);
      }
      this.sendPluginResult(pluginResults[i]);
    }
  }
  errorByJson(message:object):void {
    let pluginResult:PluginResult = PluginResult.createByJson(MessageStatus.ERROR, message);
    this.sendPluginResult(pluginResult);
  }
  errorByJsonString(message:string):void {
    let pluginResult:PluginResult = PluginResult.createByJsonString(MessageStatus.ERROR, message);
    this.sendPluginResult(pluginResult);
  }
  errorByString(message:string):void {
    let pluginResult:PluginResult = PluginResult.createByString(MessageStatus.ERROR, message);
    this.sendPluginResult(pluginResult);
  }
  errorByNumber(message:number):void {
    let pluginResult:PluginResult = PluginResult.createByNumber(MessageStatus.ERROR, message);
    this.sendPluginResult(pluginResult);
  }
  getWebTag():string {
    return this.webTag;
  }
}
