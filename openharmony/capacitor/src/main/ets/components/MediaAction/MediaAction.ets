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

import { ArkTsAttribute, NativeAttribute } from '../PluginGlobal';
import { common } from '@kit.AbilityKit';
import { camera, cameraPicker } from '@kit.CameraKit';
import { fileIo,fileUri, picker } from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { photoAccessHelper } from '@kit.MediaLibraryKit';
import { promptAction } from '@kit.ArkUI';
import { imageCompress } from '../ImageCompress/ImageCompressEx';
import { getAllExifProperties } from '../ImageCompress/ExifInfo'
import { fileIo as fs} from '@kit.CoreFileKit';
import capacitor from 'libcapacitor.so'
import { dataSharePredicates } from '@kit.ArkData';
import { image } from '@kit.ImageKit';
import { util } from '@kit.ArkTS';
import * as ErrorCode from '../ImageCompress/ErrorCode'
import { ArkTsAttributeCameraEx, ArkTsAttributeCameraGallery, PhotoFileInfo } from './CameraDefine';


export async function startCameraPicker(pageIndex:NativeAttribute,cameraDirection:string,
  allowEditing:boolean,saveToGallery:boolean): Promise<ArkTsAttributeCameraEx>{

  let context = getContext() as common.Context;
  try {

    const cameraPositionMap: Record<string, camera.CameraPosition> = {
      'FRONT': camera.CameraPosition.CAMERA_POSITION_FRONT,
      'REAR': camera.CameraPosition.CAMERA_POSITION_BACK
    };

    // 获取摄像头位置，默认后置
    const cameraPosition = cameraPositionMap[cameraDirection] ||
    camera.CameraPosition.CAMERA_POSITION_BACK;

    let pickerProfile: cameraPicker.PickerProfile = {
      cameraPosition: cameraPosition
    };

    if (!saveToGallery && !allowEditing) {
      let pathDir = context.cacheDir; // 应用沙箱目录
      let fileName = `${new Date().getTime()}`; // 唯一文件名
      let filePath = `${pathDir}/${fileName}.tmp`;
      try {
        // 关键：创建文件并确保具备写入权限
        fileIo.createRandomAccessFileSync(filePath, fileIo.OpenMode.CREATE);
      } catch (error) {
        let err = error as BusinessError;
        console.error(`create file failed. error code: ${err.code}`);
      }

      let uri = fileUri.getUriFromPath(filePath); // 转换为合法URI
      pickerProfile.saveUri = uri;
    }

    if(pageIndex.pageValue == 0) {
      let pickerResult: cameraPicker.PickerResult = await cameraPicker.pick(context, [cameraPicker.PickerMediaType.PHOTO], pickerProfile);
      console.log("the pick pickerResult is:" + JSON.stringify(pickerResult));

      if (pickerResult.resultCode == ErrorCode.CameraFail) {
        return new Promise((resolve, reject) => {
          resolve({content: "content", result: [], code: ErrorCode.CameraFail});
        });
      }

      if (!pickerResult.resultUri || pickerResult.resultUri === '') {
        // 用户取消了拍照，返回空结果
        return new Promise((resolve, reject) => {
          resolve({content: "content", result: [], code: ErrorCode.UserCancel });
        });
      }

      let galleryResult: ArkTsAttributeCameraEx = {
        content:"content",
        result:[],
        code:0
      }

      if(allowEditing) {
        galleryResult = await selectPhotoPicket(0, 1);
        //openEditor(pageIndex,pickerResult.resultUri);
      }

      return new Promise((resolve, reject) => {
        let result:Array<string> = new Array<string>();
        if (galleryResult.result.length > 0) {
          result.push(galleryResult.result[0]);
        } else {
          // 用户取消了图库选择，返回原始拍照结果
          result.push(pickerResult.resultUri);
        }

        resolve({content: "content", result: result});
      });
    } else {
      let pickerResult: cameraPicker.PickerResult = await cameraPicker.pick(context, [cameraPicker.PickerMediaType.VIDEO], pickerProfile);
      console.log("the pick pickerResult is:" + JSON.stringify(pickerResult));
      return new Promise((resolve, reject) => {
        let result:Array<string> = new Array<string>();
        result.push(pickerResult.resultUri);
        resolve({content: "content", result: result});
      });
    }
  } catch (error) {
    let err = error as BusinessError;
    console.error(`the pick call failed. error code: ${err.code}`);
  }
  return new Promise((resolve, reject) => {
    resolve({content: "content", result: []});
  });
}

//选择音频文件
export function selectAudioPicket(pageIndex:NativeAttribute): Promise<ArkTsAttribute> {
  let context = getContext() as common.Context;
  let audioSelectOptions = new picker.AudioSelectOptions();
  audioSelectOptions.maxSelectNumber = Number(pageIndex.pageArgs); // 选择媒体文件的最大数目
  let audioPicker = new picker.AudioViewPicker(context);
  return new Promise((resolve, reject) => {
    try {
      audioPicker.select(audioSelectOptions, (err: BusinessError, audioSelectResult: Array<string>) => {
        if (err) {
          console.error('AudioViewPicker.select failed with err: ' + JSON.stringify(err));
          resolve({content: "", result: []});
          return;
        }
        console.info('AudioViewPicker.select successfully, audioSelectResult uri: ' + JSON.stringify(audioSelectResult));
        resolve({content: "content", result: audioSelectResult});
      });
    } catch (error) {
      let err: BusinessError = error as BusinessError;
      console.error('AudioViewPicker failed with err: ' + JSON.stringify(err));
      resolve({content: "", result: []});
    }
  });
}

async function uriGetAssets(uri:string, fetchOption: photoAccessHelper.FetchOptions, context: common.UIAbilityContext): Promise<photoAccessHelper.PhotoAsset | null> {
  let phAccessHelper = photoAccessHelper.getPhotoAccessHelper(context);
  let fetchResult: photoAccessHelper.FetchResult<photoAccessHelper.PhotoAsset> =
    await phAccessHelper.getAssets(fetchOption);
  // 得到URI对应的PhotoAsset对象，读取文件的部分信息
  const asset: photoAccessHelper.PhotoAsset = await fetchResult.getFirstObject();
  return new Promise((resolve, reject) => {
    try {
      resolve(asset);
    } catch (error) {
      console.error(`uriGetAssets failed with err, code is ${error.code}, message is ${error.message}`);
      resolve(null);
    }
  });
}

//使用photoPicker选择媒体文件
export function selectPhotoPicket(type:number, maximumImagesCount:number): Promise<ArkTsAttributeCameraEx> {
  const photoSelectOptions = new photoAccessHelper.PhotoSelectOptions();
  if(type == 0)
    photoSelectOptions.MIMEType = photoAccessHelper.PhotoViewMIMETypes.IMAGE_TYPE; // 过滤选择媒体文件类型为IMAGE
  else if(type == 1)
    photoSelectOptions.MIMEType = photoAccessHelper.PhotoViewMIMETypes.VIDEO_TYPE; // 过滤选择媒体文件类型为VIDEO
  else
    photoSelectOptions.MIMEType = photoAccessHelper.PhotoViewMIMETypes.IMAGE_VIDEO_TYPE; // 过滤选择媒体文件类型为VIDEO
  photoSelectOptions.maxSelectNumber = maximumImagesCount; // 选择媒体文件的最大数目

  let uris: Array<string> = [];
  const photoViewPicker = new photoAccessHelper.PhotoViewPicker();
  return new Promise((resolve, reject) => {
    photoViewPicker.select(photoSelectOptions).then((photoSelectResult: photoAccessHelper.PhotoSelectResult) => {
      uris = photoSelectResult.photoUris;
      console.info('photoViewPicker.select to file succeed and uris are:' + uris);
      resolve({content: "content", result: uris ,code: 0});
    }).catch((err: BusinessError) => {
      console.error(`Invoke photoViewPicker.select failed, code is ${err.code}, message is ${err.message}`);
      resolve({content: "", result: [], code: ErrorCode.PhotosFail});
    });
  });
}


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


export async function SelectMedia(pageIndex:NativeAttribute) {

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
  };

  const jsonRoot = JSON.parse(pageIndex.pageArgs) as Record<string, string | object | undefined>;

  const method = typeof jsonRoot["method"] === "string" ? (jsonRoot["method"] as string) : "";
  compress = parseMaybeJSON<compressArg>(jsonRoot["compressArgs"], compress);
  operation  = parseMaybeJSON<operationArg>(jsonRoot["operationArgs"], operation);


  let result: ArkTsAttributeCameraEx = await selectPhotoPicket(pageIndex.pageValue, operation.Limit);
  let uriArray: Array<string> = result.result;
  if (uriArray.length == 0) {
    let resultsErr:ArkTsAttributeCameraGallery = {
      content: method,
      result: [],
      code: ErrorCode.UserCancel
    }
    capacitor.onArkTsResult(JSON.stringify(resultsErr), pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

  if(pageIndex.pageValue == 0) {
    if (compress.Quality < 100) {
      if (compress.CameraImageCompress) {
        if (compress.CameraCompressShowToast != "") {
          let duration: number = uriArray.length * 1000;
          console.info("压缩图片,处理中...");
        }
      } else {
        let duration: number = uriArray.length * 1000;
        console.info("压缩图片,处理中...");
      }
    }
  }

  let filePaths: Array<string> = new Array<string>();
  let fileInfos: PhotoFileInfo[] = new Array();

  for (let i = 0; i < uriArray.length; i++) {
    if (uriArray[i].lastIndexOf(".") < 0) {
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
    if (pageIndex.pageValue == 0) { //0选照片进行了压缩，1选视频
      if (compress.Quality < 100) {
        filePath = await imageCompress(resourceFile, filePath, compress.CompressImageSize,
          operation.Width,operation.Height,"image/jpeg",operation.CorrectOrientation,compress.Quality);
      } else {
        let file: fs.File = fs.openSync(filePath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
        fs.copyFileSync(resourceFile.fd, file.fd);
        fs.closeSync(file);
      }
      if (filePath != "") {
        filePaths.push(filePath);
      }
    } else {
      //视频C++侧转换为path，ArkTs侧无法转换
      filePaths.push(uriArray[i]);
    }
    fileInfos.push({
      path: filePath,
      exif: exifInfo
    });
    fs.closeSync(resourceFile);
  }

  let results:ArkTsAttributeCameraGallery = {
    content: method,
    result:fileInfos,
    code:0
  }

  capacitor.onArkTsResult(JSON.stringify(results), pageIndex.pageObject, pageIndex.pageWebTag);
}

export async function MediaIndex(pageIndex:NativeAttribute) {
  if(pageIndex.pageValue == 0) { //getMedias
    let options:object = JSON.parse(pageIndex.pageArgs);
    let selectMode:number = options?.["selectMode"]?options?.["selectMode"]:101;//101=picker image and video , 100=image , 102=video
    let maxSelectCount:number = options?.["maxSelectCount"]?options?.["maxSelectCount"]:40;//default 40
    let type:number = 0;
    if(selectMode == 100) {
      type = 0;
    } else if(selectMode == 102) {
      type = 1;
    } else if(selectMode == 101) {
      type= 2;
    } else {
      type = 0;
    }
    let result: ArkTsAttributeCameraEx = await selectPhotoPicket(type, maxSelectCount);
    let uriArray: Array<string> = result.result;
    if (uriArray.length == 0) {
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }

    let medias: Array<string> = new Array<string>();
    for (let i = 0; i < uriArray.length; i++) {
      if (uriArray[i].lastIndexOf(".") < 0) {
        return;
      }
      const context: common.UIAbilityContext = getContext() as common.UIAbilityContext;
      let predicates: dataSharePredicates.DataSharePredicates = new dataSharePredicates.DataSharePredicates();
      // 配置查询条件，使用PhotoViewPicker选择图片返回的uri进行查询
      predicates.equalTo('uri', uriArray[i]);
      let fetchOption: photoAccessHelper.FetchOptions = {
        fetchColumns: [photoAccessHelper.PhotoKeys.WIDTH, photoAccessHelper.PhotoKeys.HEIGHT, photoAccessHelper.PhotoKeys.TITLE, photoAccessHelper.PhotoKeys.DURATION,photoAccessHelper.PhotoKeys.SIZE],
        predicates: predicates
      };
      let asset:photoAccessHelper.PhotoAsset|null = await uriGetAssets(uriArray[i], fetchOption, context);
      if (asset != null && asset.photoType == photoAccessHelper.PhotoType.IMAGE) { //0选照片1选视频
          let media:object = new Object();
          media["mediaType"] = "image";
          media["uri"] = uriArray[i];
          media["size"] = asset.get(photoAccessHelper.PhotoKeys.SIZE);
          //media["path"]在C++侧添加，ArkTS侧无法获取path值
          medias.push(JSON.stringify(media));
      } else if(asset != null && asset.photoType == photoAccessHelper.PhotoType.VIDEO){
        let media:object = new Object();
        media["mediaType"] = "video";
        media["uri"] = uriArray[i];
        media["size"] = asset.get(photoAccessHelper.PhotoKeys.SIZE);
        //media["path"]在C++侧添加，ArkTS侧无法获取path值
        medias.push(JSON.stringify(media));
      }
    }
    result.content = "getMedias";
    result.result = medias;
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  }

  if(pageIndex.pageValue == 3) {
    let media:object = JSON.parse(pageIndex.pageArgs);
    if(media?.["uri"]) {
      const context: common.UIAbilityContext = getContext() as common.UIAbilityContext;
      let predicates: dataSharePredicates.DataSharePredicates = new dataSharePredicates.DataSharePredicates();
      // 配置查询条件，使用PhotoViewPicker选择图片返回的uri进行查询
      predicates.equalTo('uri', media?.["uri"]);
      let fetchOption: photoAccessHelper.FetchOptions = {
        fetchColumns: [photoAccessHelper.PhotoKeys.WIDTH, photoAccessHelper.PhotoKeys.HEIGHT, photoAccessHelper.PhotoKeys.TITLE, photoAccessHelper.PhotoKeys.DURATION,photoAccessHelper.PhotoKeys.SIZE],
        predicates: predicates
      };
      let asset:photoAccessHelper.PhotoAsset|null = await uriGetAssets(media?.["uri"], fetchOption, context);
      asset!.getThumbnail(async (err, pixelMap) => {
        if(err == undefined) {
          const imagePacker = image.createImagePacker();
          const packOpts: image.PackingOption = { format: 'image/jpeg', quality: 100 };
          const arrayBuffer = await imagePacker.packToData(pixelMap, packOpts);

          const uint8Array = new Uint8Array(arrayBuffer);
          const base64Helper = new util.Base64Helper();
          let base64 = base64Helper.encodeToStringSync(uint8Array);
          let result: ArkTsAttribute = { content: "extractThumbnail", result: [base64] };
          capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
        }

      });
    }
  }

  if(pageIndex.pageValue ==4) {
    let media:object = JSON.parse(pageIndex.pageArgs);
    let uri:string = media?.["uri"];
    let quality:number = media?.["quality"]?media?.["quality"]:50;
    promptAction.showToast({ message: '压缩图片，请稍后', duration: 1000 });
    const context: common.UIAbilityContext = getContext() as common.UIAbilityContext;
    let predicates: dataSharePredicates.DataSharePredicates = new dataSharePredicates.DataSharePredicates();
    predicates.equalTo('uri', uri);
    let fetchOption: photoAccessHelper.FetchOptions = {
      fetchColumns: [photoAccessHelper.PhotoKeys.WIDTH, photoAccessHelper.PhotoKeys.HEIGHT, photoAccessHelper.PhotoKeys.TITLE, photoAccessHelper.PhotoKeys.DURATION,photoAccessHelper.PhotoKeys.SIZE],
      predicates: predicates
    };
    let asset:photoAccessHelper.PhotoAsset|null = await uriGetAssets(uri, fetchOption, context);
    if(asset != null && asset.photoType == photoAccessHelper.PhotoType.IMAGE) {
      let compressSize:number = asset.get(photoAccessHelper.PhotoKeys.SIZE) as number;
      compressSize = compressSize*quality/100;

      let extName: string = uri.substring(uri.lastIndexOf("."));
      let resourceFile: fs.File = fs.openSync(uri, fs.OpenMode.READ_ONLY);
      const context: common.UIAbilityContext = getContext() as common.UIAbilityContext;
      let cacheDir = context.cacheDir;
      let fileName: number = new Date().getTime();
      let filePath: string = `${cacheDir}/${fileName}${extName}`;
      filePath = await imageCompress(resourceFile, filePath, compressSize);
      let stat = await fs.statSync(filePath);
      let obj:object = new Object();
      obj["path"] = filePath;
      obj["size"] = stat.size;
      obj["name"] = asset.get(photoAccessHelper.PhotoKeys.TITLE);
      let result: ArkTsAttribute = { content: "compressImage", result: [JSON.stringify(obj)] };
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
    }
  }

  if(pageIndex.pageValue == 5) {
    let media = JSON.parse(pageIndex.pageArgs) as Array<string>;
    if(media.length < 2) {
      return;
    }
    let uri = media[0];
    let key:string = media[1];
    const context: common.UIAbilityContext = getContext() as common.UIAbilityContext;
    let predicates: dataSharePredicates.DataSharePredicates = new dataSharePredicates.DataSharePredicates();
    predicates.equalTo('uri', uri);
    let fetchColumns:string[] = [photoAccessHelper.PhotoKeys.WIDTH, photoAccessHelper.PhotoKeys.HEIGHT, photoAccessHelper.PhotoKeys.TITLE, photoAccessHelper.PhotoKeys.DURATION,photoAccessHelper.PhotoKeys.SIZE];
    if(fetchColumns.indexOf(key) !== -1) {
      fetchColumns.push(key);
    }
    let fetchOption: photoAccessHelper.FetchOptions = {
      fetchColumns: fetchColumns,
      predicates: predicates
    };
    let asset:photoAccessHelper.PhotoAsset|null = await uriGetAssets(uri, fetchOption, context);
    if(asset != null && asset.photoType == photoAccessHelper.PhotoType.IMAGE) {
      let strValue = asset.get(key);
      let result: ArkTsAttribute = { content: "getExifForKey", result: [strValue+""] };
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
    }
  }
}