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
// 导入相关模块包
import { image } from '@kit.ImageKit';
import { BusinessError } from '@kit.BasicServicesKit';

/**
 * 获取图片所有EXIF信息
 * @param imageSource ImageSource实例
 * @returns 包含所有EXIF信息的对象或错误信息
 */
export async function getAllExif(imageSource: image.ImageSource): Promise<Record<string, string>> {
  if (!imageSource) {
    throw new Error('ImageSource is undefined');
  }

  try {
    // 获取所有支持的EXIF键值
    const keys: image.PropertyKey[] = [
      image.PropertyKey.BITS_PER_SAMPLE,
      image.PropertyKey.ORIENTATION,
      image.PropertyKey.IMAGE_LENGTH,
      image.PropertyKey.IMAGE_WIDTH,
      image.PropertyKey.GPS_LATITUDE,
      // 此处添加其他需要获取的EXIF键值（需参考官方支持范围）
    ];

    const exifData: Record<string, string> = {};

    // 循环读取每个键值对应的EXIF信息
    for (const key of keys) {
      const options: image.ImagePropertyOptions = {
        index: 0,
        defaultValue: 'N/A'
      };
      const value = await imageSource.getImageProperty(key, options);
      exifData[key.toString()] = value;
    }

    return exifData;
  } catch (error) {
    const err = error as BusinessError;
    throw new Error(`读取EXIF失败: ${err.code}, ${err.message}`);
  }
}
