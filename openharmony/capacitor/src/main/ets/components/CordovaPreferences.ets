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

import { HashMap } from "@kit.ArkTS";
import { NormalizeError } from "./NormalizeError";

export class CordovaPreferences {
  private preferences:HashMap<string, string> = new HashMap<string, string>();
  set(name:string, value:string):void {
    try {
      this.preferences.set(name,value);
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`CordovaPreferences set failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    }

  }
  getString(name:string):string|null {
    try {
      if (this.preferences.hasKey(name)) {
        return this.preferences.get(name);
      }
    } catch(error) {
      const cordovaError = NormalizeError(error);
      console.error(`CordovaPreferences getString failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    }
    return null;
  }
  getNumber(name:string):number|null {
    try {
      if(this.preferences.hasKey(name)) {
        return Number(this.preferences.get(name));
      }
    } catch(error) {
      const cordovaError = NormalizeError(error);
      console.error(`CordovaPreferences getNumber failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    }
    return null;
  }
  getBoolean(name:string):boolean|null {
    try {
      if(this.preferences.hasKey(name)) {
        if(this.preferences.get(name) == "true") {
          return true;
        }
        return false;
      }
    } catch(error) {
      const cordovaError = NormalizeError(error);
      console.error(`CordovaPreferences getBoolean failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    }
    return null;
  }

  getAll():HashMap<string, string> {
    return this.preferences;
  }
}