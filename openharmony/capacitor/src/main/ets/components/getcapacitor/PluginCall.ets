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
import capacitor from 'libcapacitor.so';

export  class PluginCall {
  private webTag:string;
  private objectName:string;
  private actionName:string;
  private callbackId:string;
  private jsonArgs:object;
  private keepAlive:boolean;
  private response?:object;
  private success?:boolean;
  private code?:string;
  private message?:string;
  constructor(webTag:string, objectName:string, actionName:string, callbackId:string, jsonArgs:object) {
    this.webTag = webTag;
    this.objectName = objectName;
    this.actionName = actionName;
    this.callbackId = callbackId;
    this.jsonArgs = jsonArgs;
    this.keepAlive = false;
  }

  getWebTag():string {
    return this.webTag;
  }

  getActionName():string {
    return this.actionName;
  }

  getObjectName():string {
    return this.objectName;
  }
  getCallbackId():string {
    return this.callbackId;
  }

  getJsonArgs():object {
    return this.jsonArgs;
  }

  successCallback(result?:object):void {
    this.success = true;
    if (result) {
      this.response = result;
    }
    capacitor.onArkTsResult(this.toString(), this.objectName, this.webTag);
  }

  errorCallback(error?:object):void {
    this.success = false;
    if (error) {
      this.response = error;
    }
    capacitor.onArkTsResult(this.toString(), this.objectName, this.webTag);
  }
  
  resolve(result?:object):void {
    this.successCallback(result);
  }
  
  reject(error?:object):void {
    this.errorCallback(error);
  }
  
  rejectMessage(code:string, msg:string, data?:object):void {
    this.success = false;
    this.code = code;
    this.message = msg;
    if(data) {
      this.response = data;
    }
    capacitor.onArkTsResult(this.toString(), this.objectName, this.webTag);
  }

  getString(key:string, defaultString?:string):string {
    if(this.jsonArgs?.[key] !== undefined) {
      return this.jsonArgs[key];
    }
    return defaultString ? defaultString : "";
  }
  
  getNumber(key:string, defaultNumber?:number):number {
    if(this.jsonArgs?.[key] !== undefined) {
      return this.jsonArgs[key];
    }
    return defaultNumber ? defaultNumber : 0;
  }

  getObject(key:string, defaultObject?:object):object {
    if(this.jsonArgs?.[key] !== undefined) {
      return this.jsonArgs[key];
    }
    return defaultObject ? defaultObject : new Object();
  }

  getArray(key:string, defaultArray?:Array<object>):Array<object> {
    if(this.jsonArgs?.[key] !== undefined) {
      return this.jsonArgs[key];
    }
    return defaultArray ? defaultArray : new Array<object>();
  }

  getBoolean(key:string, defaultBoolean?:boolean):boolean {
    if(this.jsonArgs?.[key] !== undefined) {
      return this.jsonArgs[key];
    }
    return defaultBoolean ? defaultBoolean : false;
  }

  save():void {
    this.keepAlive = true;
  }

  isKeepAlive():boolean {
    return this.keepAlive;
  }

  setKeepAlive(keepAlive:boolean):void {
    this.keepAlive = keepAlive;
  }

  toString():string {
    let retObj:object = new Object();
    retObj["webTag"] = this.webTag;
    retObj["pluginId"] = this.objectName;
    retObj["methodName"] = this.actionName;
    retObj["callbackId"] = this.callbackId;
    retObj["success"] = this.success;
    retObj["keepAlive"] = this.keepAlive;
    if(this.response) {
      retObj["response"] = this.response;
    }
    if(this.code) {
      retObj["code"] = this.code;
    }
    if(this.message) {
      retObj["message"] = this.message;
    }
    
    return JSON.stringify(retObj);
  }
}