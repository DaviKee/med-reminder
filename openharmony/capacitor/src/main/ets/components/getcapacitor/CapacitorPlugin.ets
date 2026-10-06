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
import { HashMap } from "@kit.ArkTS";
import { NormalizeError } from "../NormalizeError";
import { PluginCall } from "./PluginCall";
import { abilityAccessCtrl, common, Permissions } from "@kit.AbilityKit";
import { checkPermissionGrant, reqPermissionsFromUser } from "../Permission/Permission";
import { cordovaWebTagToObjectGlobe } from "../PluginGlobal";
import { MainPage } from "../MainPage";
import { CordovaWebView } from "../CordovaWebView";
import { CordovaPreferences } from "../CordovaPreferences";
import { CordovaInterface } from "../CordovaInterface";

export class PluginMethod {
  static RETURN_PROMISE:string = "promise";
  static RETURN_CALLBACK:string = "callback";
  static RETURN_NONE:string = "none";
}


type PluginMethodFunc = (call: PluginCall) => void;

export class FunctionPair {
  returnType:string;
  function:PluginMethodFunc;
  constructor(returnType:string, fun:PluginMethodFunc) {
    this.returnType = returnType;
    this.function = fun;
  }
}
export  class CapacitorPlugin {
  protected webId?:string;
  protected cordovaWebView?: CordovaWebView;
  protected preferences?:CordovaPreferences;
  protected cordovaInterface?: CordovaInterface;
  protected serviceName?: string;
  //HashMap<webId, HashMap<插件名称, 实例对象>>，插件内可以通过此属性获取所有webview的自定义插件，不包含系统系统自带插件
  protected mapWebIdToCustomPlugins?:HashMap<string, HashMap<string, CapacitorPlugin>>;
  //插件内通过此属性获取所有webview的cordovaWebView,可以控制所有webview的属性
  protected mapWebIdToWebView?:HashMap<string, CordovaWebView>;
  //插件内通过此属性获取webview的CordovaInterface，可以控制所有webview的页面路由栈
  protected mapWebIdToInterface?:HashMap<string, CordovaInterface>;
  //webId和webTag的对应关系，webId可以在父组件中自定义，webTag为cordova内部使用，自动生成，不能修改。
  protected mapWebIdToWebTag?:HashMap<string, string>;

  protected mapPluginMethod:HashMap<string, FunctionPair> = new HashMap<string, FunctionPair>();
  protected mapEventListeners:HashMap<string, Array<PluginCall> > = new HashMap<string, Array<PluginCall>>();
  protected mapRetainedEventArguments:HashMap<string, Array<object> > = new HashMap<string, Array<object>>();
  protected permissions:Array<Permissions> = [];

  privateInitialize(webId:string,
    serviceName: string,
    preferences:CordovaPreferences,
    cordovaInterface: CordovaInterface,
    cordovaWebView:CordovaWebView,
    mapWebIdToCustomPlugins:HashMap<string, HashMap<string, CapacitorPlugin>>,
    mapWebIdToWebView:HashMap<string, CordovaWebView>,
    mapWebIdToInterface:HashMap<string, CordovaInterface>,
    mapWebIdToWebTag:HashMap<string, string>): void {
    this.webId = webId;
    this.serviceName = serviceName;
    this.preferences = preferences;
    this.cordovaInterface = cordovaInterface;
    this.cordovaWebView = cordovaWebView;
    this.mapWebIdToCustomPlugins = mapWebIdToCustomPlugins;
    this.mapWebIdToWebView = mapWebIdToWebView;
    this.mapWebIdToInterface = mapWebIdToInterface;
    this.mapWebIdToWebTag = mapWebIdToWebTag;
  }

  registerMethod(methodName: string, func: PluginMethodFunc, returnType: string = PluginMethod.RETURN_PROMISE): void {
    try{
      this.mapPluginMethod.set(methodName, new FunctionPair(returnType, func));
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to onWatchIndexPageUpdate. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  registerPermission(permissions:Array<Permissions>) {
    this.permissions = permissions;
  }
    
  callMethod(methodName:string, call:PluginCall) {
    try {
      if(methodName == "addListener") {
        this.addListener(call);
      }

      if(methodName == "removeListener") {
        this.removeListener(call);
      }

      if(methodName == "removeAllListeners") {
        this.removeAllListeners(call);
      }

      if(methodName == "checkPermissions") {
        this.checkPermissions(call);
      }

      if(methodName == "requestPermissions") {
        this.requestPermissions(call);
      }

      if(this.mapPluginMethod.hasKey(methodName)) {
        const method = this.mapPluginMethod.get(methodName);
        try {
          method.function(call);
        } catch (error) {
          console.error(`Method ${methodName} failed: ${error.message}`);
        }
      }

    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to callMethod. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  getAllMethods():Array<object> {
    let arrayObj:Array<object> = new Array<object>();
    try {
      this.mapPluginMethod.forEach((value, key)=>{
        let obj:object = new Object();
        obj[key as string] = value?.returnType;
        arrayObj.push(obj);
      })
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to getAllMethods. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
    return arrayObj;
  }

  load():void{
    return;
  }

  protected addEventListener(strEventName:string, call:PluginCall):void {
    try {
      if(this.mapEventListeners.hasKey(strEventName)) {
        this.mapEventListeners.get(strEventName).push(call);
      } else {
        this.mapEventListeners.set(strEventName, Array.of(call));
      }
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to addEventListener. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  protected removeEventListener(strEventName:string, call:PluginCall):void {
    try {
      if(this.mapEventListeners.hasKey(strEventName)) {
        let index = this.mapEventListeners.get(strEventName).findIndex(eventCall => eventCall.getCallbackId() == call.getCallbackId());
        if(index != -1) {
          this.mapEventListeners.get(strEventName).splice(index, 1);
        }
      }
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to addEventListener. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  notifyListeners(strEventName:string, data:object, retainUntilConsumed?:boolean):void {
    try {
      if(this.mapEventListeners.hasKey(strEventName)) {
        this.mapEventListeners.get(strEventName).forEach((call)=>{
          call.setKeepAlive(true);
          call.resolve(data);
        })
      } else if(retainUntilConsumed){
        if(this.mapRetainedEventArguments.hasKey(strEventName)) {
          this.mapRetainedEventArguments.get(strEventName).push(data);
        } else {
          this.mapRetainedEventArguments.set(strEventName, Array.of(data));
        }
      }
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to notifyListeners. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  hasListeners(eventName:string):boolean {
    try {
      return this.mapEventListeners.hasKey(eventName);
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to hasListeners. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
    return false;
  }

  sendRetainedArgumentsForEvent(eventName:string):void {
    try {
      if(this.mapRetainedEventArguments.hasKey(eventName)) {
        this.mapRetainedEventArguments.get(eventName).forEach((obj)=>{
          this.notifyListeners(eventName, obj);
        })
        this.mapRetainedEventArguments.remove(eventName);
      }
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to sendRetainedArgumentsForEvent. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  addListener(call:PluginCall):void {
    let eventName = call.getString("eventName");
    if(eventName != "") {
      call.setKeepAlive(true);
      this.addEventListener(eventName, call);
    }
  }

  removeListener(call:PluginCall):void {
    let eventName = call.getString("eventName");
    let callbackId = call.getString("callbackId");
    let saveCall:PluginCall = new PluginCall(call.getWebTag(), call.getObjectName(), call.getActionName(), callbackId, new Object());
    if(eventName != "") {
      this.removeEventListener(eventName, saveCall);
    }
  }

  removeAllListeners(call:PluginCall):void {
    try {
      this.mapEventListeners.clear();
      call.resolve();
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to removeAllListeners. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  async checkPermissions(call: PluginCall): Promise<void> {
    let grantResult:object = new Object();
    for(let i=0; i<this.permissions.length; i++) {
      let grantStatus: abilityAccessCtrl.GrantStatus = await checkPermissionGrant(this.permissions[i]);
      if(grantStatus === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        grantResult[this.permissions[i]] = "GRANTED";
      } else {
        grantResult[this.permissions[i]] = "DENIED";
      }
    }
    call.resolve(grantResult);
  }
  
  async requestPermissions(call: PluginCall): Promise<void> {
    try {
      if (!cordovaWebTagToObjectGlobe.hasKey(call.getWebTag())) {
        return;
      }
      let mainPage = cordovaWebTagToObjectGlobe.get(call.getWebTag()) as MainPage;
      let context = mainPage.getUIContext().getHostContext() as common.UIAbilityContext
      let grantResult:object = new Object();
      for(let i=0; i<this.permissions.length; i++) {
        let permission:Array<Permissions> = new Array<Permissions>();
        permission.push(this.permissions[i]);
        let grantStatus = await reqPermissionsFromUser(permission, context);
        grantResult[this.permissions[i]] = grantStatus==1?"GRANTED":"DENIED";
      }
      call.resolve(grantResult);
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to removeAllListeners. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
    return;
  }

  async checkPermission(permission: Permissions): Promise<boolean> {
    let grantStatus: abilityAccessCtrl.GrantStatus = await checkPermissionGrant(permission);
    if(grantStatus === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
      return true;
    } else {
      return false;
    }
  }

  async requestPermission(context:common.UIAbilityContext, permission: Permissions): Promise<boolean> {
    let permissions:Array<Permissions> = new Array<Permissions>();
    permissions.push(permission);
    let grantStatus = await reqPermissionsFromUser(permissions, context);
    return grantStatus==1?true:false;
  }
  
  handleOnStart():void{

  }
  
  handleOnPageStart():void {
    
  }
  
  handleOnEnd():void{

  }

  handleOnResume():void{

  }

  handleOnPause():void{

  }

  handleOnDestroy():void{

  }

}