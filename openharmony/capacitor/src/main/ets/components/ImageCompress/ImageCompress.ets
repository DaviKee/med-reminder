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

import { image } from '@kit.ImageKit';
import { fileIo as fs} from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { NormalizeError } from '../NormalizeError';

/**
 * packing压缩后的图片保存为文件
 * @param sourcePixelMap：原始待压缩图片的PixelMap
 * @param filePath：文件路径
 * @Param packOpts：配置选项
 * @returns fileSize：返回图片大小
 */
async function packing(sourcePixelMap: image.PixelMap, filePath:string, packOpts : image.PackingOption): Promise<number> {
  const imagePackerApi = image.createImagePacker();
  let file:fs.File|null = null;
  try {
    file = fs.openSync(filePath, fs.OpenMode.CREATE | fs.OpenMode.TRUNC | fs.OpenMode.READ_WRITE);
    await imagePackerApi.packToFile(sourcePixelMap, file.fd, packOpts);
    let fileStat = fs.statSync(filePath);
    return fileStat.size;
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`current auth trust level is not supported. Code is ${cordovaError.code}, message is ${cordovaError.message}`);
    return 0;
  } finally {
    if(file != null) {
      try {
        fs.closeSync(file.fd);
      }catch (error) {
        const cordovaError = NormalizeError(error);
        console.error(`Failed to fs.closeSync. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
      }
    }
  }
}

/**
 * 二分方式循环压缩
 * @param sourcePixelMap：原始待压缩图片的PixelMap
 * @param filePath：文件路径
 * @Param packOpts：配置选项
 * @param maxCompressedImageByte：压缩目标图像字节长度
 * @returns compressedImageData：返回二分packing压缩后的图片数据
 */
async function packingImage(sourcePixelMap: image.PixelMap, filePath:string, packOpts : image.PackingOption, maxCompressedImageByte: number): Promise<number> {
  // 图片质量参数范围为0-100，这里以10为最小二分单位创建用于packing二分图片质量参数的数组。
  let imageQuality: number = 0;
  const packingArray: number[] = [];
  const DICHOTOMY_ACCURACY = 10;
  // 性能知识点: 如果对图片压缩质量要求不高，建议调高最小二分单位dichotomyAccuracy，减少循环，提升packing压缩性能。
  for (let i = 0; i <= 100; i += DICHOTOMY_ACCURACY) {
    packingArray.push(i);
  }
  let fileSize = 0;
  let left = 0; // 定义二分搜索范围的左边界
  let right = packingArray.length - 1; // 定义二分搜索范围的右边界
  // 二分压缩图片
  while (left <= right) {
    const mid = Math.floor((left + right) / 2); // 定义二分搜索范围的中间位置
    imageQuality = packingArray[mid]; // 获取二分中间位置的图片质量值
    packOpts.quality = imageQuality;
    // 根据传入的图片质量参数进行packing压缩，返回压缩后的图片文件流数据。
    fileSize = await packing(sourcePixelMap, filePath, packOpts);
    // 判断查找一个尽可能接近但不超过压缩目标的压缩大小
    if (fileSize <= maxCompressedImageByte) {
      // 二分目标值在右半边，继续在更高的图片质量参数（即mid + 1）中搜索
      left = mid + 1;
      // 判断mid是否已经二分到最后，如果二分完了，退出
      if (mid === packingArray.length - 1) {
        break;
      }
      // 获取下一次二分的图片质量参数（mid+1）压缩的图片文件流数据
      packOpts.quality = packingArray[mid + 1];
      fileSize = await packing(sourcePixelMap, filePath, packOpts);
      // 判断用下一次图片质量参数（mid+1）压缩的图片大小是否大于指定图片的压缩目标大小。如果大于，说明当前图片质量参数（mid）压缩出来的
      // 图片大小最接近指定图片的压缩目标大小。传入当前图片质量参数mid，得到最终目标图片压缩数据。
      if (fileSize > maxCompressedImageByte) {
          packOpts.quality = packingArray[mid];
          fileSize = await packing(sourcePixelMap, filePath, packOpts);
        break;
      }
    } else {
      // 目标值不在当前范围的右半部分，将搜索范围的右边界向左移动，以缩小搜索范围并继续在下一次迭代中查找左半部分。
      right = mid - 1;
    }
  }
  return fileSize;
}

/**
 * 获取选择角度
 * @param imageSource：图片的imageSource
 * @returns rotate：返回旋转角度
 */
async function imageRotate(imageSource : image.ImageSource): Promise<number> {
  try {
    let options: image.ImagePropertyOptions = { index: 0, defaultValue: 'This key has no value!' };
    const data: string = await imageSource.getImageProperty(image.PropertyKey.ORIENTATION, options);
    const orientationMap: Record<string, number> = {
      "Top-left": 0,// 无需旋转
      "Top-right": 90,// 顺时针旋转90度
      "Bottom-right": 180,// 顺时针旋转180度
      "Bottom-left": 270,// 顺时针旋转270度（或逆时针90度）
      "Left-top": 270,// 顺时针旋转270度（或逆时针90度）+ 水平翻转
      "Right-top": 90,// 顺时针旋转90度 + 水平翻转
      "Right-bottom": 90,// 顺时针旋转90度 + 水平翻转（或等价操作）
      "Left-bottom": 270// 顺时针旋转270度 + 水平翻转（或等价操作）
    };
    return orientationMap[data] || 0;
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`current auth trust level is not supported. Code is ${cordovaError.code}, message is ${cordovaError.message}`);
    return 0;
  }
}

/**
 * 压缩图片入口函数
 * @param resourceFile：图片文件
 * @param filePath：压缩后保存的文件路径
 * @Param fileSize：压缩图片的目标大小
 * @param targetWidth：压缩后的目标宽度，为保持图片不变形，会进行调整
 * @param targetHeight：压缩后的目标高度，为保持图片不变形，会进行调整
 * @param targetFormat：压缩后图片的格式
 * @param correctOrientation：确保图片为正确方向
 * @returns filePath：返回压缩后保存图片的路径
 */
export async function imageCompress(resourceFile:fs.File, filePath:string, fileSize:number, targetWidth?:number, targetHeight?:number, targetFormat?:string, correctOrientation?:boolean): Promise<string>{
  //创建ImageSource实例
  const IMAGE_QUALITY = 0;
  const maxCompressedImageSize = fileSize;//4M
  //第一步:创建一个imageSource
  const imageSource : image.ImageSource = image.createImageSource(resourceFile.fd);
  //第二步：获取图像信息
  const imageInfo:image.ImageInfo = imageSource.getImageInfoSync(0);
  const decodingOptions: image.DecodingOptions = {
    editable: true, // 是否可编辑。当取值为false时，图片不可二次编辑，如crop等操作将失败。
    desiredPixelFormat: 3, // 解码的像素格式。3表示RGBA_8888。
  }

  //第三步：调整图片宽高
  if(targetWidth || targetHeight) {
    let width = imageInfo.size.width;
    let height = imageInfo.size.height;
    if(targetWidth && targetWidth > 0) {
      targetHeight = targetWidth * height/width;
      decodingOptions.desiredSize = {
        height:targetHeight,
        width:targetWidth
      }
    } else if(targetHeight && targetHeight > 0) {
      targetWidth = width * targetHeight /height;
      decodingOptions.desiredSize = {
        height:targetHeight,
        width:targetWidth
      }
    }
  }

  //第四步：设置旋转角度
  if(correctOrientation) {
    let rotate:number = await imageRotate(imageSource);
    decodingOptions.rotate = rotate;
  }
  //第五步：设置format
  let format = "image/jpeg";
  if(targetFormat) {
    format = targetFormat;
  }

  let packOpts : image.PackingOption = { format:format, quality:IMAGE_QUALITY };
  return new Promise((resolve, reject) => {
    imageSource.createPixelMap(decodingOptions).then(async (sourcePixelMap: image.PixelMap) => {
      // 通过PixelMap进行编码。compressedImageData为打包获取到的图片文件流。
      let fileSize = await packing(sourcePixelMap, filePath, packOpts);
      // 压缩目标图像字节长度
      const maxCompressedImageByte = maxCompressedImageSize;
      if (maxCompressedImageByte > fileSize) {
        fileSize = await packingImage(sourcePixelMap, filePath, packOpts, maxCompressedImageByte);
      }
      resolve(filePath);
    });
  });
}