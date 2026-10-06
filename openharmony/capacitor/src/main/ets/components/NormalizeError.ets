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

export interface CordovaError {
  code: number | string
  message: string
}

export function NormalizeError(error: object): CordovaError {
  if (error == null) {
    return {
      code: 'NULL_ERROR',
      message: 'undefined'
    };
  }

  if (typeof error !== 'object'  && typeof error !== 'function') {
    return {
      code: 'UNKNOWN_CODE',
      message: String(error)
    };
  }

  if (error && typeof error === 'object' && error?.["code"] !== undefined && error?.["message"] !== undefined) {
    return {
      code: error?.["code"] as number,
      message: error?.["message"] as string
    }
  }

  if (error instanceof Error) {
    return {
      code: 'UNKNOWN_CODE',
      message: error.message,
    }
  }

  return {
    code: 'UNKNOWN_CODE',
    message: String(error)
  }
}
