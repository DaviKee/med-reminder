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

import { startCameraPicker } from '../MediaAction/MediaAction';
import { ArkTsAttribute, NativeAttribute } from '../PluginGlobal';
import { promptAction } from '@kit.ArkUI';
import { fileIo as fs} from '@kit.CoreFileKit';
import { abilityAccessCtrl, common, Permissions} from '@kit.AbilityKit';
import { imageCompress } from './ImageCompressEx';
import { getAllExifProperties } from './ExifInfo'
import * as ErrorCode from './ErrorCode'

import capacitor from 'libcapacitor.so'
import { ArkTsAttributeCameraEx, ArkTsAttributeCameraGallery, PhotoFileInfo } from '../MediaAction/CameraDefine';
import { checkPermissionGrant, reqPermissionsFromUser } from '../Permission/Permission';

interface compressArg {
  Quality: number;
  CameraImageCompress: boolean;
  CompressImageSize:number;
  CameraCompressShowToast: string;
}

interface operationArg {
  AllowEditing: boolean;
  SaveToGallery: boolean;
  Width:number;
  Height: number;
  CorrectOrientation:boolean,
  Limit:number;
  Direction:string
}

// 安全解析工具：输入可能是 string/object/undefined
function parseMaybeJSON<T>(val:string | object | undefined, fallback: T): T {
  if (typeof val === "string") {
    try { return JSON.parse(val) as T; } catch { return fallback; }
  }
  if (val && typeof val === "object") {
    return val as T;
  }
  return fallback;
}

export async function StartCamera(pageIndex:NativeAttribute) {

  let compress: compressArg = {
    Quality: 50,
    CameraImageCompress: false,
    CompressImageSize: 4 * 1024 * 1024,
    CameraCompressShowToast: ""
  };

  let operation: operationArg = {
    AllowEditing:false,
    SaveToGallery:false,
    Width:0,
    Height:0,
    CorrectOrientation:true,
    Limit:0,
    Direction:'REAR'
  };

  const jsonRoot = JSON.parse(pageIndex.pageArgs) as Record<string, string | object | undefined>;

  const method = typeof jsonRoot["method"] === "string" ? (jsonRoot["method"] as string) : "";
  compress = parseMaybeJSON<compressArg>(jsonRoot["compressArgs"], compress);
  operation  = parseMaybeJSON<operationArg>(jsonRoot["operationArgs"], operation);

  let grantStatus: abilityAccessCtrl.GrantStatus = await checkPermissionGrant('ohos.permission.CAMERA');
  if(grantStatus !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
    let context = getContext() as common.UIAbilityContext;
    let grantRequestStatus = await reqPermissionsFromUser(['ohos.permission.CAMERA'], context, pageIndex);

    if(grantRequestStatus !== 1) {
      let ret: ArkTsAttributeCameraEx = {
        content: method,
        result: [],
        code: ErrorCode.NoCameraPermission
      };
      capacitor.onArkTsResult(JSON.stringify(ret), pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }
  }

  let result: ArkTsAttributeCameraEx | undefined = await startCameraPicker(pageIndex,operation.Direction,
    operation.AllowEditing,operation.SaveToGallery);

  let uriArray: Array<string> = result.result;
  if (uriArray.length == 0 || uriArray[0] == "") {
    let ret:ArkTsAttributeCameraEx = {
      content : method,
      result: [],
      code: result.code
    };
    capacitor.onArkTsResult(JSON.stringify(ret), pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

  if(pageIndex.pageValue == 0 || pageIndex.pageValue == 2) {
    if (compress.Quality < 100) {
      if (compress.CameraImageCompress) {
        if (compress.CameraCompressShowToast != "") {
          let duration: number = uriArray.length * 1000;
          console.info("照片压缩,处理中...");
        }
      } else {
        let duration: number = uriArray.length * 1000;
        console.info("照片压缩,处理中...");
      }
    }
  }

  let filePaths: Array<string> = new Array<string>();
  let fileInfos: PhotoFileInfo[] = [];

  for (let i = 0; i < uriArray.length; i++) {
    if (uriArray[i].lastIndexOf(".") < 0) {
      console.log("uriArray:"+uriArray[i]);
      return;
    }

    let extName: string = uriArray[i].substring(uriArray[i].lastIndexOf("."));
    let resourceFile: fs.File | null = null;
    try {
      resourceFile = fs.openSync(uriArray[i], fs.OpenMode.READ_ONLY);
    } catch (err) {
      let ret:ArkTsAttributeCameraGallery = {
        content : method,
        result: [],
        code:ErrorCode.FileFail
      };
      capacitor.onArkTsResult(JSON.stringify(ret), pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }
    let exifInfo = await getAllExifProperties(resourceFile);
    const context: common.UIAbilityContext = getContext() as common.UIAbilityContext;
    let cacheDir = context.cacheDir;
    let fileName: number = new Date().getTime();
    let filePath: string = `${cacheDir}/${fileName}${i}${extName}`;
    if (pageIndex.pageValue == 0 || pageIndex.pageValue == 2) { //0选照片1选视频
      if (compress.Quality < 100) {
        filePath = await imageCompress(resourceFile, filePath, compress.CompressImageSize,operation.Width,
          operation.Height,"image/jpeg",operation.CorrectOrientation,compress.Quality);
      } else {
        let file: fs.File = fs.openSync(filePath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
        fs.copyFileSync(resourceFile.fd, file.fd);
        fs.closeSync(file);
      }
      if (filePath != "") {
        filePaths.push(filePath);
      }
    } else {
      let file: fs.File = fs.openSync(filePath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
      fs.copyFileSync(resourceFile.fd, file.fd);
      fs.closeSync(file);
      filePaths.push(filePath);
    }

    fileInfos.push({
      path:filePath,
      exif:exifInfo
    });

    fs.closeSync(resourceFile);
  }

  let results:ArkTsAttributeCameraGallery = {
    content:method,
    result:fileInfos,
    code:0
  }

  capacitor.onArkTsResult(JSON.stringify(results), pageIndex.pageObject, pageIndex.pageWebTag);
}

