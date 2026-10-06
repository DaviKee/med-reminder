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
import { Environment, fileUri, fileIo as fs } from '@kit.CoreFileKit';
import { NormalizeError } from '../NormalizeError';
import capacitor from 'libcapacitor.so';
import { util } from '@kit.ArkTS';


export function OHMkdir(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction Mkdir start,args is ${pageIndex.pageArgs}`);
  let mkdirParam: DirOptions = JSON.parse(pageIndex.pageArgs);
  let mkdirPath = getPath(mkdirParam.directory, mkdirParam.path);
  console.info(`FilesystemAction Mkdir mkdirPath: ${mkdirPath}`);

  try {
    let res = fs.accessSync(mkdirPath, fs.AccessModeType.EXIST);
    if (res) {
      // 目录已存在
      console.error(`FilesystemAction accessSync failed, res is ${res}`);
      capacitor.onArkTsResult(JSON.stringify({ content: 'mkdir', result: [], errorMsg: 'Directory exists' }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }
    // 创建目录
    fs.mkdirSync(mkdirPath, mkdirParam.recursive)
    let result: ArkTsAttribute = { content: 'mkdir', result: [] };
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction mkdirSync failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'mkdir', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

}

export function OHRmdir(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction Rmdir start,args is ${pageIndex.pageArgs}`);

  let rmdirParam: DirOptions = JSON.parse(pageIndex.pageArgs);
  let rmdirPath = getPath(rmdirParam.directory, rmdirParam.path);
  console.info(`FilesystemAction Rmdir rmdirPath: ${rmdirPath}`);

  try {
    let res = fs.accessSync(rmdirPath, fs.AccessModeType.EXIST);
    if (!res) {
      // 目录不存在
      console.error(`FilesystemAction accessSync failed, res is ${res}`);
      capacitor.onArkTsResult(JSON.stringify({ content: 'rmdir', result: [], errorMsg: 'No such directory' }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }
    let fileNames = fs.listFileSync(rmdirPath);
    if (fileNames.length > 0 && !rmdirParam.recursive) {
      // 目录不为空
      console.error(`FilesystemAction Rmdir failed, directory is not empty but recursive=false`);
      capacitor.onArkTsResult(JSON.stringify({
        content: 'rmdir',
        result: [],
        errorMsg: 'Cannot delete directory with children; received recursive=false but directory has contents.'
      }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }
    // 删除目录
    if (rmdirParam.recursive) {
      for (let i = 0; i < fileNames.length; i++) {
        let dirPath = rmdirPath + '/' + fileNames[i];
        // 判断是否为文件夹
        let isDirectory: boolean = false;
        try {
          isDirectory = fs.statSync(dirPath).isDirectory();
        } catch (error) {
          const cordovaError = NormalizeError(error);
          console.error(`FilesystemAction remove file failed with error, code is ${cordovaError.code}, message is ${cordovaError.message}`);
          capacitor.onArkTsResult(JSON.stringify({ content: 'rmdir', result: [], errorMsg: cordovaError.message }),
            pageIndex.pageObject, pageIndex.pageWebTag);
          return;
        }
        if (isDirectory) {
          fs.rmdirSync(dirPath);
        } else {
          fs.unlinkSync(dirPath);
        }
      }
    }
    // 删除目录自身
    fs.rmdirSync(rmdirPath);
    console.info(`FilesystemAction Rmdir success`);
    let result: ArkTsAttribute = { content: 'rmdir', result: [] };
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction rmdirSync failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'rmdir', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

}

export function OHRename(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction Rename start,args is ${pageIndex.pageArgs}`);
  let renameParam: CopyOptions = JSON.parse(pageIndex.pageArgs);
  let oldPath = getPath(renameParam.directory, renameParam.from);
  if (!renameParam.toDirectory) {
    renameParam.toDirectory = renameParam.directory
  }
  let newPath = getPath(renameParam.toDirectory, renameParam.to);
  console.info(`FilesystemAction Rename oldPath: ${oldPath}`);
  console.info(`FilesystemAction Rename newPath: ${newPath}`);

  try {
    let oldRes = fs.accessSync(oldPath, fs.AccessModeType.EXIST);
    // 判断源文件/目录是否存在
    if (!oldRes) {
      console.error(`FilesystemAction accessSync old failed, res is ${oldRes}`);
      capacitor.onArkTsResult(JSON.stringify({
        content: 'rename',
        result: [],
        errorMsg: `${renameParam.from} not exists`
      }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return
    }
    let newRes = fs.accessSync(newPath, fs.AccessModeType.EXIST);
    if (newRes) {
      console.error(`FilesystemAction accessSync old failed, res is ${oldRes}`);
      capacitor.onArkTsResult(JSON.stringify({ content: 'rename', result: [], errorMsg: `${renameParam.to} exists` }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return
    }
    fs.renameSync(oldPath, newPath);
    capacitor.onArkTsResult(JSON.stringify({ content: 'rename', result: [] }),
      pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction rename failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'rename', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

}

export function OHReadFile(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction ReadFile start,args is ${pageIndex.pageArgs}`);
  let readFileParam: FileOptions = JSON.parse(pageIndex.pageArgs);
  let filePath = getPath(readFileParam.directory, readFileParam.path);
  try {
    let file = fs.openSync(filePath, fs.OpenMode.READ_ONLY);
    let stat = fs.statSync(filePath);
    let buffer: ArrayBuffer = new ArrayBuffer(stat.size);
    fs.readSync(file.fd, buffer)
    const uint8Array = new Uint8Array(buffer);
    const base64Helper = new util.Base64Helper();
    let str = readFileParam.encoding ? uint8ArrayToString(uint8Array, readFileParam.encoding) :
      base64Helper.encodeToStringSync(uint8Array)
    fs.closeSync(file.fd)
    capacitor.onArkTsResult(JSON.stringify({ content: 'readFile', result: [str] }), pageIndex.pageObject,
      pageIndex.pageWebTag);
    return
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction readFile failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'readFile', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }
}

export function OHReadFileInChunks(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction ReadFileInChunks start,args is ${pageIndex.pageArgs}`);
  let readFileParam: ReadFileInChunksOptions = JSON.parse(pageIndex.pageArgs);
  let filePath = getPath(readFileParam.directory, readFileParam.path);
  try {
    if (!fs.accessSync(filePath, fs.AccessModeType.EXIST)) {
      console.error(`FilesystemAction ReadFileInChunks accessSync failed`);
      capacitor.onArkTsResult(JSON.stringify({ content: 'readFileInChunks', result: [], errorMsg: 'No such file' }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }
    const textDecoderOptions: util.TextDecoderOptions = {
      fatal: false,
      ignoreBOM: true
    }
    const decodeToStringOptions: util.DecodeToStringOptions = {
      stream: true
    }
    const textDecoder = util.TextDecoder.create(readFileParam.encoding, textDecoderOptions);
    const base64Helper = new util.Base64Helper();
    // 创建文件输入流,定义缓冲区
    let chunkArray: string[] = [];
    let inputStream = fs.createStreamSync(filePath, 'r')
    let bufSize = readFileParam.chunkSize;
    let buf = new ArrayBuffer(bufSize);
    let readLen = 0;
    // 循环读取文件
    do {
      readLen = inputStream.readSync(buf);
      if (readLen > 0) {
        let uint8Array = new Uint8Array(buf, 0, readLen);
        let chunkStr = readFileParam.encoding ? textDecoder.decodeToString(uint8Array, decodeToStringOptions) :
          base64Helper.encodeToStringSync(uint8Array)
        chunkArray.push(chunkStr);
        capacitor.onArkTsResult(JSON.stringify({ content: 'readFileInChunks', result: [chunkStr] }),
          pageIndex.pageObject,
          pageIndex.pageWebTag);
      } else {
        // 读取结束
        console.info(`FilesystemAction ReadFileInChunks read end`)
        capacitor.onArkTsResult(JSON.stringify({ content: 'readFileInChunks', result: [''] }),
          pageIndex.pageObject, pageIndex.pageWebTag);
        return;
      }
    } while (readLen > 0)
    inputStream.closeSync();

  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction readFileInChunks failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'readFileInChunks', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

}

export function OHReaddir(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction ReadFile start,args is ${pageIndex.pageArgs}`);
  let readdirParam: BaseFileOptions = JSON.parse(pageIndex.pageArgs);
  let filePath = getPath(readdirParam.directory, readdirParam.path);
  try {
    let stat = fs.statSync(filePath);
    if (!stat.isDirectory()) {
      // 非目录
      console.error(`FilesystemAction readdir failed, not a directory`);
      capacitor.onArkTsResult(JSON.stringify({ content: 'readdir', result: [], errorMsg: 'Not a directory' }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return
    }
    let fileNames = fs.listFileSync(filePath);
    let files: FileInfo[] = [];
    for (let i = 0; i < fileNames.length; i++) {
      let dirPath = filePath + '/' + fileNames[i];
      let dirStat = fs.statSync(dirPath);
      let file: FileInfo = {
        name: fileNames[i],
        type: dirStat.isDirectory() ? 'directory' : 'file',
        size: dirStat.size,
        ctime: dirStat.ctime,
        mtime: dirStat.mtime,
        uri: fileUri.getUriFromPath(dirPath)
      }
      files.push(file);
    }
    let readdirResult: ReaddirResult = {
      files: files
    }
    capacitor.onArkTsResult(JSON.stringify({ content: 'readdir', readFiles: readdirResult }), pageIndex.pageObject,
      pageIndex.pageWebTag);
    return;
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction readdir failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'readdir', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }
}

export function OHDeleteFile(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction DeleteFile start,args is ${pageIndex.pageArgs}`);
  let statParam: BaseFileOptions = JSON.parse(pageIndex.pageArgs);
  let filePath = getPath(statParam.directory, statParam.path);
  console.info(`FilesystemAction delete filePath: ${filePath}`);
  try {
    fs.unlinkSync(filePath)
    capacitor.onArkTsResult(JSON.stringify({ content: 'deleteFile', result: [] }),
      pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction delete failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'deleteFile', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

}

export function OHCopyFile(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction CopyFile start,args is ${pageIndex.pageArgs}`);
  let copyParam: CopyOptions = JSON.parse(pageIndex.pageArgs);
  let oldPath = getPath(copyParam.directory, copyParam.from);
  if (!copyParam.toDirectory) {
    copyParam.toDirectory = copyParam.directory
  }
  let newPathDir = getPathDir(copyParam.toDirectory);
  let newPath = getPath(copyParam.toDirectory, copyParam.to);
  console.info(`FilesystemAction CopyFile oldPath: ${oldPath}`);
  console.info(`FilesystemAction CopyFile newPath: ${newPath}`);
  try {
    // 判断新路径是否已存在
    let newPathFlag = fs.accessSync(newPath, fs.AccessModeType.EXIST)
    if (newPathFlag) {
      console.error(`FilesystemAction copyFile failed, newPath already exist`);
      capacitor.onArkTsResult(JSON.stringify({
        content: 'copy',
        result: [],
        errorMsg: `${copyParam.to} already exist`
      }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return
    }
    // 判断源文件/目录
    let oldStat = fs.statSync(oldPath);
    if (oldStat.isFile()) {
      // 文件
      fs.copyFileSync(oldPath, newPath)
    } else {
      // 目录
      if (!newPathFlag) {
        fs.mkdirSync(newPath);
      }
      let filenames = fs.listFileSync(oldPath);
      for (let i = 0; i < filenames.length; i++) {
        let srcPath = oldPath + `/${filenames[i]}`;
        // 判断文件是否是目录文件
        if (fs.statSync(srcPath).isDirectory()) {
          fs.copyDirSync(srcPath, newPath);
        } else {
          let dstPath = newPath + `/${filenames[i]}`;
          fs.copyFileSync(srcPath, dstPath);
        }
      }
    }
    let uri = fileUri.getUriFromPath(newPath)
    console.info(`FilesystemAction CopyFile success,uri is ${uri}`);
    let result: ArkTsAttribute = { content: 'copy', result: [uri] };
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction copyFile failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'copy', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }
}

export function OHWriteFile(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction WriteFile start,args is ${pageIndex.pageArgs}`);
  // 判断是写入还是追加 0 写入,1 追加
  let writeType = pageIndex.pageValue;
  let writeFileParam: WriteParamOptions = JSON.parse(pageIndex.pageArgs);
  let pathDir: string = getPathDir(writeFileParam.directory);
  let filePath = getPath(writeFileParam.directory, writeFileParam.path);
  console.info(`FilesystemAction WriteFile filePath: ${filePath}`);

  if (writeFileParam.encoding && !checkEncoding(writeFileParam.encoding)) {
    console.error(`FilesystemAction WriteFile failed, encoding is invalid`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'writeFile', errorMsg: 'openHarmony only support Encoding.UTF8' }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

  if (!writeFileParam.encoding && typeof writeFileParam.data === 'string') {
    // 处理base64字符串
    const base64Helper = new util.Base64Helper();
    let array = base64Helper.decodeSync(writeFileParam.data)
    writeFileParam.data = array.buffer as ArrayBuffer;
  }

  try {
    if (writeType === 0) {
      // 写入
      if (writeFileParam.recursive) {
        // 递归写入
        let mkDirPath = filePath.substring(filePath.lastIndexOf(pathDir) + 1, filePath.lastIndexOf('/'))
        console.info(`FilesystemAction WriteFile mkDirPath: ${mkDirPath}`);
        if (!fs.accessSync(mkDirPath, fs.AccessModeType.EXIST)) {
          fs.mkdirSync(mkDirPath, writeFileParam.recursive)
        }
      }
      let file = fs.openSync(filePath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
      fs.writeSync(file.fd, writeFileParam.data);
      let uri = fileUri.getUriFromPath(file.path)
      fs.closeSync(file.fd);
      console.info(`FilesystemAction WriteFile success,uri is ${uri}`);
      let result: ArkTsAttribute = { content: 'writeFile', result: [uri] };
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
    } else {
      // 追加
      let file = fs.openSync(filePath, fs.OpenMode.READ_WRITE | fs.OpenMode.APPEND);
      fs.writeSync(file.fd, writeFileParam.data);
      fs.closeSync(file.fd);
      let result: ArkTsAttribute = { content: 'appendFile', result: [] };
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
    }

  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction writeFile failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({
      content: writeType === 0 ? 'writeFile' : 'appendFile',
      result: [],
      errorMsg: cordovaError.message
    }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

}

export function OHStat(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction Stat start,args is ${pageIndex.pageArgs}`);
  let statParam: BaseFileOptions = JSON.parse(pageIndex.pageArgs);
  let filePath = getPath(statParam.directory, statParam.path);

  try {
    let res = fs.accessSync(filePath, fs.AccessModeType.EXIST);
    if (!res) {
      // 文件不存在
      console.error(`FilesystemAction accessSync failed, res is ${res}`);
      capacitor.onArkTsResult(JSON.stringify({ content: 'stat', result: [], errorMsg: 'No such file or directory' }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }
    // 获取文件信息
    let stat = fs.statSync(filePath);
    let fileInfo: FileInfo = {
      name: filePath.substring(filePath.lastIndexOf('/') + 1) || '',
      type: stat.isDirectory() ? 'directory' : 'file',
      size: stat.size,
      ctime: stat.ctime,
      mtime: stat.mtime,
      uri: fileUri.getUriFromPath(filePath)
    };
    capacitor.onArkTsResult(JSON.stringify({ content: 'stat', result: [fileInfo] }), pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction rmdirSync failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'stat', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }
}

function getPath(directory: string, path: string) {
  let pathDir: string = getPathDir(directory);
  let filePath = pathDir + path;
  return filePath
}

function checkEncoding(encoding: string) {
  // 当前文件读写仅支持utf-8
  return encoding === 'utf8';
}

export function OHGetUri(pageIndex: NativeAttribute) {
  console.info(`FilesystemAction GetUri start,args is ${pageIndex.pageArgs}`);

  let uriParam: BaseFileOptions = JSON.parse(pageIndex.pageArgs);
  let path: string = getPath(uriParam.directory, uriParam.path);
  console.info(`FilesystemAction GetUri path: ${path}`);

  try {
    let res = fs.accessSync(path, fs.AccessModeType.EXIST);
    if (!res) {
      console.error(`FilesystemAction accessSync failed, res is ${res}`);
      capacitor.onArkTsResult(JSON.stringify({ content: 'getUri', result: [], errorMsg: 'No such file or directory' }),
        pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }
    let uri = fileUri.getUriFromPath(path)
    let result: ArkTsAttribute = { content: 'getUri', result: [uri] };
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`FilesystemAction getFullDirectoryUri failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    capacitor.onArkTsResult(JSON.stringify({ content: 'getUri', result: [], errorMsg: cordovaError.message }),
      pageIndex.pageObject, pageIndex.pageWebTag);
    return;
  }

}

export function uint8ArrayToString(array: Uint8Array, encoding: string) {
  let textDecoderOptions: util.TextDecoderOptions = {
    fatal: false,
    ignoreBOM: true
  }
  let decodeToStringOptions: util.DecodeToStringOptions = {
    stream: false
  }
  let textDecoder = util.TextDecoder.create(encoding, textDecoderOptions);
  let retStr = textDecoder.decodeToString(array, decodeToStringOptions);
  console.info('FilesystemAction Byte flow into understandable strings：' + retStr);
  return retStr;
}

export function getPathDir(directory: string) {
  let context = getContext() as common.UIAbilityContext;
  let applicationContext = context.getApplicationContext();
  let pathDir: string;
  switch (directory) {
    case 'DOCUMENTS':
      if (canIUse("SystemCapability.FileManagement.File.Environment.FolderObtain")) {
        pathDir = Environment.getUserDocumentDir();
      } else {
        pathDir = applicationContext.filesDir;
      }
      break;
    case 'DATA':
      pathDir = applicationContext.filesDir;
      break;
    case 'CACHE':
      pathDir = applicationContext.cacheDir;
      break;
    case 'TEMPORARY':
      pathDir = applicationContext.tempDir;
      break;
    case 'LIBRARY':
      pathDir = applicationContext.filesDir;
      break;
    default:
      pathDir = applicationContext.filesDir;
      break;
  }
  pathDir += '/';
  return pathDir;
}

interface DirOptions {
  path: string;
  recursive: boolean;
  directory: string;
}

interface FileInfo {
  name: string;
  type: string;
  size: number;
  ctime: number;
  mtime: number;
  uri: string;
}

interface BaseFileOptions {
  path: string;
  directory: string;
}

interface CopyOptions {
  from: string;
  to: string;
  directory: string;
  toDirectory: string;
}

interface FileOptions extends BaseFileOptions {
  encoding: string;
}

interface WriteParamOptions extends FileOptions {
  data: ArrayBuffer | string | Uint8Array;
  recursive: boolean;
}

interface DownloadFileOptions extends BaseFileOptions {
  progress: boolean;
  recursive: boolean;
  url: string;
}

interface ReaddirResult {
  files: FileInfo[];
}

interface ReadFileInChunksOptions extends FileOptions {
  chunkSize: number;
}