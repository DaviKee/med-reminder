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
// 定义创建函数的返回值，用于ArkTS侧和Native侧的交互。
export interface ArkTsAttributeCameraEx {
  result: Array<string>;
  content: string;
  code?: number;
}

export interface PhotoFileInfo {
  path: string;
  exif: string | object; // 根据实际 exif 数据类型调整
}

export interface ArkTsAttributeCameraGallery{
  content: string;
  result: PhotoFileInfo[]; // 改为 FileInfo 对象数组
  code?: number;
}
