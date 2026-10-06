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

import { HashMap, util } from "@kit.ArkTS"
import { NormalizeError } from "./NormalizeError"

export enum MessageStatus {
  NO_RESULT = 0,
  OK = 1,
  CLASS_NOT_FOUND_EXCEPTION = 2,
  ILLEGAL_ACCESS_EXCEPTION = 3,
  INSTANTIATION_EXCEPTION = 4,
  MALFORMED_URL_EXCEPTION = 5,
  IO_EXCEPTION = 6,
  INVALID_ACTION = 7,
  JSON_EXCEPTION = 8,
  ERROR = 9
}

export enum MessageType {
  /**
   * 字符串
   */
  STRING = 1,
  /**
   * JSON对象，包括JSON对象和JSON数组
   */
  JSON = 2,
  /**
   * 数值类型，包括整形和浮点
   */
  NUMBER = 3,
  /**
   * boolean
   */
  BOOLEAN = 4,
  /**
   * 空值
   */
  NULL = 5,
  /**
   * 字节数组
   */
  ARRAYBUFFER = 6,
  /**
   * 二进制字符串
   */
  BINARYSTRING = 7,
  /**
   * 多媒体类型
   */
  MULTIPART = 8
}

export enum MessageStatusDesc {
  NO_RESULT = 'No result',
  OK = 'OK',
  CLASS_NOT_FOUND_EXCEPTION = 'Class not found',
  ILLEGAL_ACCESS_EXCEPTION = 'Illegal access',
  INSTANTIATION_EXCEPTION = 'Instantiation error',
  MALFORMED_URL_EXCEPTION = 'Malformed url',
  IO_EXCEPTION = 'IO error',
  INVALID_ACTION = 'Invalid action',
  JSON_EXCEPTION = 'JSON error',
  ERROR = 'Error'
}

const STATUS_DESC_MAPPER: HashMap<MessageStatus, string> = new HashMap()
STATUS_DESC_MAPPER.set(MessageStatus.NO_RESULT, MessageStatusDesc.NO_RESULT)
STATUS_DESC_MAPPER.set(MessageStatus.OK, MessageStatusDesc.OK)
STATUS_DESC_MAPPER.set(MessageStatus.CLASS_NOT_FOUND_EXCEPTION, MessageStatusDesc.CLASS_NOT_FOUND_EXCEPTION)
STATUS_DESC_MAPPER.set(MessageStatus.ILLEGAL_ACCESS_EXCEPTION, MessageStatusDesc.ILLEGAL_ACCESS_EXCEPTION)
STATUS_DESC_MAPPER.set(MessageStatus.INSTANTIATION_EXCEPTION, MessageStatusDesc.INSTANTIATION_EXCEPTION)
STATUS_DESC_MAPPER.set(MessageStatus.MALFORMED_URL_EXCEPTION, MessageStatusDesc.MALFORMED_URL_EXCEPTION)
STATUS_DESC_MAPPER.set(MessageStatus.IO_EXCEPTION, MessageStatusDesc.IO_EXCEPTION)
STATUS_DESC_MAPPER.set(MessageStatus.INVALID_ACTION, MessageStatusDesc.INVALID_ACTION)
STATUS_DESC_MAPPER.set(MessageStatus.JSON_EXCEPTION, MessageStatusDesc.JSON_EXCEPTION)
STATUS_DESC_MAPPER.set(MessageStatus.ERROR, MessageStatusDesc.ERROR)

/**
 * 将Cordova消息状态枚举值转换为文字描述
 *
 * @param status 状态枚举
 * @returns 状态描述
 */
export function convertMessageStatusDesc(status: MessageStatus): string {
  try {
    return STATUS_DESC_MAPPER.hasKey(status) ? STATUS_DESC_MAPPER.get(status) : MessageStatusDesc.ERROR;
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`ConvertMessageStatusDesc failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    return MessageStatusDesc.ERROR;
  }
}

export class PluginResult {
  private readonly status: MessageStatus
  private readonly messageType: MessageType
  private keepCallback: boolean = false
  private readonly message: string = ''
  private readonly multipartMessages: PluginResult[] = []

  constructor(status: MessageStatus, type: MessageType, message: string) {
    this.status = status
    this.messageType = type
    this.message = message
  }

  public static create(status: MessageStatus): PluginResult {
    return PluginResult.createByString(status, convertMessageStatusDesc(status))
  }

  public static createByString(status: MessageStatus, message:string): PluginResult {
    return new PluginResult(status, MessageType.STRING, message)
  }

  public static createByJson(status: MessageStatus, message: object): PluginResult {
    return new PluginResult(status, MessageType.JSON, JSON.stringify(message))
  }

  public static createByJsonString(status: MessageStatus, message: string): PluginResult {
    return new PluginResult(status, MessageType.JSON, message)
  }

  public static createByNumber(status: MessageStatus, message: number): PluginResult {
    return new PluginResult(status, MessageType.NUMBER, '' + message)
  }

  public static createByBoolean(status: MessageStatus, message: boolean): PluginResult {
    return new PluginResult(status, MessageType.BOOLEAN, message?"true":"false")
  }

  public static createByNull(status: MessageStatus): PluginResult {
    return new PluginResult(status, MessageType.NULL, "")
  }

  public static createByArrayBuffer(status: MessageStatus, message: Uint8Array): PluginResult {
    return new PluginResult(status, MessageType.ARRAYBUFFER, new util.Base64Helper().encodeToStringSync(message, util.Type.BASIC))
  }

  public static createByBinaryString(status: MessageStatus, message: Uint8Array): PluginResult {
    return new PluginResult(status, MessageType.BINARYSTRING, new util.Base64Helper().encodeToStringSync(message, util.Type.BASIC))
  }

  public static createByMessageList(status: MessageStatus, message: PluginResult[]): PluginResult {
    const result = new PluginResult(status, MessageType.MULTIPART, '')
    result.multipartMessages.push(...message)
    return result
  }

  getKeepCallback(): boolean {
    return this.keepCallback
  }

  setKeepCallback(keepCallback: boolean): void {
    this.keepCallback = keepCallback
  }

  getStatus(): MessageStatus {
    return this.status
  }

  getMessageType(): MessageType {
    return this.messageType
  }

  getMessage(): string {
    return this.message
  }

  getMultipartMessagesSize(): number {
    return this.multipartMessages.length
  }

  getMultipartMessage(index: number): PluginResult {
    return this.multipartMessages[index]
  }
}