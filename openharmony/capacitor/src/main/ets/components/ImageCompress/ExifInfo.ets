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
import image from '@ohos.multimedia.image';
import fs from '@ohos.file.fs';
import hilog from '@ohos.hilog';
import { fileUri } from '@kit.CoreFileKit';

const TAG = 'AppExif';
const DOMAIN = 0x00001;

/**
 * 读取选项
 */
export interface ReadExifOptions {
  /** 要读取的属性列表，不传则读取所有 */
  properties?: string[];
}

/**
 * 从URI读取EXIF信息（返回JSON字符串）
 * @param uri 图片URI
 * @param options 读取选项
 * @returns Promise<string> JSON字符串
 */
export async function readExifFromUri(uri: string, options?: ReadExifOptions): Promise<string> {
  const imageSource = image.createImageSource(uri);
  const exifData = await extractExifInfo(imageSource, options?.properties);
  await imageSource.release();

  // 使用JSON.stringify转换为JSON字符串
  return JSON.stringify(exifData);
}

/**
 * 从文件路径读取EXIF信息（返回JSON字符串）
 * @param filePath 文件路径
 * @param options 读取选项
 * @returns Promise<string> JSON字符串
 */
export async function readExifFromFile(filePath: string, options?: ReadExifOptions): Promise<string> {
  const file = fs.openSync(filePath, fs.OpenMode.READ_ONLY);
  const imageSource = image.createImageSource(file.fd);

  const exifData = await extractExifInfo(imageSource, options?.properties);

  await imageSource.release();
  fs.closeSync(file);

  // 使用JSON.stringify转换为JSON字符串
  return JSON.stringify(exifData);
}

/**
 * 从ArrayBuffer读取EXIF信息（返回JSON字符串）
 * @param buffer ArrayBuffer
 * @param options 读取选项
 * @returns Promise<string> JSON字符串
 */
export async function readExifFromBuffer(buffer: ArrayBuffer, options?: ReadExifOptions): Promise<string> {
  const imageSource = image.createImageSource(buffer);
  const exifData = await extractExifInfo(imageSource, options?.properties);
  await imageSource.release();

  // 使用JSON.stringify转换为JSON字符串
  return JSON.stringify(exifData);
}

/**
 * 获取单个EXIF属性（返回JSON字符串）
 * @param source URI或文件路径
 * @param key 属性键
 * @returns Promise<string> JSON字符串
 */
export async function getExifProperty(source: string, key: string): Promise<string> {
  const imageSource = image.createImageSource(source);
  const value = await imageSource.getImageProperty(key);
  await imageSource.release();

  const result: Record<string, string> = {};
  result[key] = value;

  // 使用JSON.stringify转换为JSON字符串
  return JSON.stringify(result);
}

/**
 * 修改EXIF属性
 * @param filePath 文件路径
 * @param key 属性键
 * @param value 属性值
 * @returns Promise<void>
 */
export async function modifyExifProperty(filePath: string, key: string, value: string): Promise<void> {
  const file = fs.openSync(filePath, fs.OpenMode.READ_WRITE);
  const imageSource = image.createImageSource(file.fd);
  await imageSource.modifyImageProperty(key, value);
  await imageSource.release();
  fs.closeSync(file);
}

/**
 * 批量修改EXIF属性（从JSON字符串）
 * @param filePath 文件路径
 * @param jsonStr JSON字符串
 * @returns Promise<void>
 */
export async function modifyExifProperties(filePath: string, jsonStr: string): Promise<void> {
  const file = fs.openSync(filePath, fs.OpenMode.READ_WRITE);
  const imageSource = image.createImageSource(file.fd);

  // 使用JSON.parse解析JSON字符串
  const properties = JSON.parse(jsonStr) as Record<string, string>;
  const keys = Object.keys(properties);

  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    const value = properties[key];
    if (value !== undefined && value !== null) {
      await imageSource.modifyImageProperty(key, value);
    }
  }

  await imageSource.release();
  fs.closeSync(file);
}

/**
 * 获取所有EXIF属性（返回JSON字符串）
 * @param source URI或文件路径
 * @returns Promise<string> JSON字符串
 */
export async function getAllExifProperties(source: fs.File): Promise<string> {

  const imageSource = image.createImageSource(source.fd);
  const allKeys = getAllExifKeys();
  const result: Record<string, string> = {};

  for (let i = 0; i < allKeys.length; i++) {
    const key = allKeys[i];
    let info: string = '';
    let options: image.ImagePropertyOptions = { index: 0, defaultValue: 'This key has no value!' };
    try {
      let value = await imageSource.getImageProperty(key, options);
      hilog.info(DOMAIN, TAG, '数据key: %{public}s - 数据value:%{public}s',key,value);
      if (value) {
        result[key] = value;
      }
    } catch (error) {
      info =
        `Failed to get the value of the ${key} with error: ${error}.`;
      console.error(info);
      hilog.info(DOMAIN, TAG, '失败 %{public}s', info);
      return info; // 获取key值失败时返回错误信息
    }
  }
  await imageSource.release();
  // 使用JSON.stringify转换为JSON字符串
  return JSON.stringify(result);
}

/**
 * 检查图片是否包含EXIF信息
 * @param source URI或文件路径
 * @returns Promise<boolean>
 */
export async function hasExifInfo(source: string): Promise<boolean> {
  const imageSource = image.createImageSource(source);
  const value = await imageSource.getImageProperty('DateTime');
  await imageSource.release();
  return value !== null && value !== undefined && value.length > 0;
}

/**
 * 提取EXIF信息为对象
 */
async function extractExifInfo(imageSource: image.ImageSource, properties?: string[]): Promise<Record<string, string>> {
  const keys = properties !== null && properties !== undefined ? properties : getAllExifKeys();
  const result: Record<string, string> = {};

  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    const value = await imageSource.getImageProperty(key);
    if (value) {
      result[key] = value;
    }
  }

  return result;
}

/**
 * 获取所有EXIF键
 */
function getAllExifKeys(): string[] {
  return [
    'Orientation', 'DateTime', 'DateTimeOriginal', 'Make', 'Model',
    'ExposureTime', 'FNumber', 'ISOSpeedRatings', 'WhiteBalance',
    'FocalLength', 'Flash', 'GPSLatitude', 'GPSLongitude', 'GPSAltitude',
    'ImageWidth', 'ImageLength', 'Software', 'Artist', 'Copyright',
    'ImageDescription', 'SubSecTime', 'PixelXDimension', 'PixelYDimension',
    'ExposureBiasValue', 'MeteringMode', 'LightSource', 'SceneCaptureType',
    'DigitalZoomRatio', 'Contrast', 'Saturation', 'Sharpness'
  ];
}

/**
 * 转换方向值为文本
 * @param orientation 方向值
 * @returns 文本描述
 */
export function getOrientationText(orientation: string): string {
  if (!orientation) return '未知';

  if (orientation === '1') return '正常';
  if (orientation === '2') return '水平翻转';
  if (orientation === '3') return '旋转180度';
  if (orientation === '4') return '垂直翻转';
  if (orientation === '5') return '逆时针90度+水平翻转';
  if (orientation === '6') return '顺时针90度';
  if (orientation === '7') return '顺时针90度+水平翻转';
  if (orientation === '8') return '逆时针90度';

  return orientation;
}

/**
 * 转换白平衡值为文本
 * @param whiteBalance 白平衡值
 * @returns 文本描述
 */
export function getWhiteBalanceText(whiteBalance: string): string {
  if (!whiteBalance) return '未知';
  return whiteBalance === '0' ? '自动' : '手动';
}

/**
 * 转换闪光灯值为文本
 * @param flash 闪光灯值
 * @returns 文本描述
 */
export function getFlashText(flash: string): string {
  if (!flash) return '未知';

  if (flash === '0') return '未闪光';
  if (flash === '1') return '闪光';
  if (flash === '5') return '闪光(无回闪检测)';
  if (flash === '7') return '闪光(有回闪检测)';
  if (flash === '9') return '强制闪光';
  if (flash === '16') return '未闪光(强制)';
  if (flash === '24') return '自动(未闪光)';
  if (flash === '25') return '自动(闪光)';

  return flash;
}