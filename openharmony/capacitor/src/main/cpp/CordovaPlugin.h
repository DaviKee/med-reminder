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

#ifndef CORDOVA_PLUGIN_H
#define CORDOVA_PLUGIN_H


#include "CordovaPreferences.h"
#include "CallbackContext.h"
#include "CordovaArgs.h"

/**
 * @brief Dynamically instantiates C++ side plugin objects.
 *        Each plugin must use the REGISTER_PLUGIN_CLASS macro in its CPP file to enable dynamic instantiation.
 */
typedef void* (*Constructor)();

class CObjectFactory {
public:
    static void registerClass(std::string className, Constructor constructor)
    {
        constructors()[className] = constructor;
    }

    static void* createObject(const std::string& className)
    {
        Constructor constructor = nullptr;
        if (constructors().find(className) != constructors().end())
            constructor = constructors().find(className)->second;
        if (constructor == nullptr)
            return nullptr;

        return (*constructor)();
    }

private:
    inline static std::map<std::string, Constructor>& constructors()
    {
        static std::map<std::string, Constructor> instance;
        return instance;
    }
};

/**
 * @brief Registers C++ plugins. Each C++ plugin must be registered to allow dynamic instantiation.
 */
#define REGISTER_PLUGIN_CLASS(class_name)                                                                              \
    class class_name##Helper {                                                                                         \
    public:                                                                                                            \
        class_name##Helper()                                                                                           \
        {                                                                                                              \
            CObjectFactory::registerClass(#class_name, class_name##Helper::creatObjFunc);                              \
        }                                                                                                              \
        static void* creatObjFunc()                                                                                    \
        {                                                                                                              \
            return new class_name();                                                                                   \
        }                                                                                                              \
    };                                                                                                                 \
    class_name##Helper class_name##helper;

class CordovaPlugin {
    std::string m_serviceName;
    void* m_pluginManage;

    // 回调数据结构
    struct CallbackData {
        std::string action;
        int value;
        std::string args;
        std::string object;
        CallbackContext cbc;
        napi_ref function_ref;
    };

protected:
    std::vector<std::string> m_vecHostName; // only WhitelistPlugin
public:
    /**cProperty configuration object from config.xml.*/
    CordovaPreferences* m_preferences;

    CordovaPlugin()
    {
        m_serviceName = "";
        m_pluginManage = nullptr;
    }

    ~CordovaPlugin() {}

    void* getPluginManage()
    {
        return m_pluginManage;
    }

    std::string getServiceName()
    {
        return m_serviceName;
    }

    std::vector<std::string>* getHostName()
    {
        return &m_vecHostName;
    }

    /**
     * @brief Call this after constructing to initialize the plugin.
     * Final because we want to be able to change args without breaking plugins.
     */
    void privateInitialize(const std::string& strServiceName, void* pluginManage, CordovaPreferences* preferences)
    {
        m_serviceName = strServiceName;
        m_pluginManage = pluginManage;
        m_preferences = preferences;
        initialize();
        pluginInitialize();
    }

    /**
     * Called after plugin construction and fields have been initialized.
     * Prefer to use pluginInitialize instead since there is no value in
     * having parameters on the initialize() function.
     *
     * @deprecated Use {@link #pluginInitialize()} instead. This method is no longer recommended
     *             and will be removed in future versions.
     */
    virtual void initialize()
    {
        return;
    }

    /**
     * @brief Called after plugin construction and fields have been initialized.
     */
    virtual void pluginInitialize()
    {
        return;
    }

    /**
     * @brief Called when a message is sent to plugin.
     * @param id            The message id
     * @param data          The message data
     */
    virtual void onMessage(const std::string& id, const std::string& data)
    {
        return;
    }

    /**
     * @brief Called when the system is about to start resuming a previous activity.
     *
     * @param multitasking		Flag indicating if multitasking is turned on for app
     */
    virtual void onPause(bool multitasking) {}

    /**
     * @brief Called when the ability will start interacting with the user.
     * @param multitasking		Flag indicating if multitasking is turned on for app
     */
    virtual void onResume(bool multitasking) {}

    /**
     * @brief  Called when the app is becoming visible to the user.
     */
    virtual void onStart(const std::string& strWebTag) {}

    /**
     *@brief  Called when the web is load start
     */
    virtual void onPageStart(const std::string& strWebTag) {}

    /**
     * @brief  Called when the web is load end
     */
    virtual void onPageEnd(const std::string& strWebTag) {}

    /**
     * @brief The final call you receive before your activity is destroyed.
     */
    virtual void onDestroy(const std::string& strWebTag) {}

    /**
     * @brief 依赖于白名单插件的配置的返回，其他插件几乎不使用
     * @return true 允许请求，false，拒绝请求
     */
    virtual bool shouldAllowRequest(const std::string& url)
    {
        return true;
    }

    /**
     * @brief Plugin execution function.
     * @param action Action std::string.
     * @param rawArgs Argument data.
     * @param callbackContext Callback object for execution results.
     * @return true if successful, false if the action is not found.
     */
    bool execute(const std::string& action, const std::string& rawArgs, CallbackContext callbackContext)
    {
        cJSON* json = cJSON_Parse(rawArgs.c_str());
        bool ret = execute(action, json, callbackContext);
        cJSON_Delete(json); // 插件内如果有多线程操作需复制json，否则会内存错误
        return ret;
    }

    /**
     * @brief Plugin execution function, implemented by specific plugins.
     * @param action Action std::string.
     * @param args JSON-formatted data.
     * @param callbackContext Callback object for execution results.
     * @return true if successful, false if the action is not found.
     */
    virtual bool execute(const std::string& action, cJSON* args, CallbackContext callbackContext)
    {
        return false;
    }

    /**
     * @brief Plugin execution function, implemented by specific plugins.
     * @param serviceName
     * @param action Action
     * @param rawArgs Argument data.
     * @param callbackContext Callback object for execution results.
     * @return true
     */
    bool execute(const std::string& serviceName,
                 const std::string& action,
                 const std::string& rawArgs,
                 CallbackContext callbackContext)
    {
        cJSON* json = cJSON_Parse(rawArgs.c_str());
        bool ret = execute(serviceName, action, json, callbackContext);
        cJSON_Delete(json); // 插件内如果有多线程操作需复制json，否则会内存错误
        return ret;
    }

    virtual bool
    execute(const std::string& serviceName, const std::string& action, cJSON* args, CallbackContext callbackContext)
    {
        return false;
    }

    /**
     * @brief Calls an ArkTS function from the C++ side. If the C++ side cannot complete all tasks and needs to invoke
     * an ArkTS function, this method enables cross-language communication from C++ to ArkTS. This function is a
     * synchronous call.
     * @param strAction Identifier for the ArkTS function call (maps to a specific function on the ArkTS side).
     * @param nValue Integer parameter passed from C++ to ArkTS.
     * @param pArgs String parameter passed from C++ to ArkTS (typically a JSON std::string).
     * @param pObject Plugin name on the C++ side.
     * @param cbc Callback object for execution results.
     * @return true if successful，false if failed
     */
    virtual bool executeArkTs(
        const std::string& strAction, const int nValue, const char* pArgs, const char* pObject, CallbackContext cbc)
    {
        void* pFunctionRefresh = Application::getFunctionRefresh(cbc.getWebTag());
        if (pFunctionRefresh == nullptr) {
            return false;
        }
        napi_property_descriptor desc[] = {
            {"pageIndex", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"pageValue", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"pageArgs", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"pageObject", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"pageWebTag", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr}};

        napi_value pageIndex = nullptr;
        const char* pPageIndex = strAction.c_str();
        napi_create_string_utf8((napi_env)Application::g_env, pPageIndex, strlen(pPageIndex), &pageIndex);
        desc[0].value = pageIndex;

        napi_value pageValue;
        int nPageValue = nValue;
        napi_create_int32((napi_env)Application::g_env, nPageValue, &pageValue);
        desc[1].value = pageValue;

        napi_value pageArgs = nullptr;
        const char* pPageArgs = pArgs;
        napi_create_string_utf8((napi_env)Application::g_env, pPageArgs, strlen(pPageArgs), &pageArgs);
        desc[2].value = pageArgs;

        napi_value pageObject = nullptr;
        const char* pPageObject = pObject;
        napi_create_string_utf8((napi_env)Application::g_env, pPageObject, strlen(pPageObject), &pageObject);
        desc[3].value = pageObject;

        napi_value pageWebTag = nullptr;
        std::string strWebTag = cbc.getWebTag();
        const char* pPageWebTag = strWebTag.c_str();
        napi_create_string_utf8((napi_env)Application::g_env, pPageWebTag, strlen(pPageWebTag), &pageWebTag);
        desc[4].value = pageWebTag;

        napi_value pageAttribute = nullptr;
        auto ret = napi_create_object_with_properties(
            (napi_env)Application::g_env, &pageAttribute, sizeof(desc) / sizeof(desc[0]), desc);
        if (ret != napi_ok) {
            cbc.error("napi_create_object_with_properties error");
            return false;
        }

        napi_value argv[1] = {pageAttribute};

        napi_value function = nullptr;
        napi_get_reference_value((napi_env)Application::g_env, (napi_ref)pFunctionRefresh, &function);

        napi_call_function((napi_env)Application::g_env, nullptr, function, 1, argv, nullptr);

        return true;
    }

    /**
     * @brief Calls an ArkTS function from the C++ side. If the C++ side cannot complete all tasks and needs to invoke
     * an ArkTS function, this method enables cross-language communication from C++ to ArkTS. This function is an
     * asynchronous call.
     * @param strAction Identifier for the ArkTS function call (maps to a specific function on the ArkTS side).
     * @param nValue Integer parameter passed from C++ to ArkTS.
     * @param pArgs String parameter passed from C++ to ArkTS (typically a JSON std::string).
     * @param pObject Plugin name on the C++ side.
     * @param cbc Callback object for execution results.
     * @return true if successful，false if failed
     */
    virtual bool executeArkTsAsync(
        const std::string& strAction, const int nValue, const char* pArgs, const char* pObject, CallbackContext cbc)
    {
        void* pFunctionRefresh = Application::getFunctionRefresh(cbc.getWebTag());
        if (pFunctionRefresh == nullptr) {
            return false;
        }

        CallbackData* data = new CallbackData{
            strAction, nValue, pArgs ? pArgs : "", pObject ? pObject : "", cbc, (napi_ref)pFunctionRefresh};

        std::thread worker(executeInThread, data);
        worker.detach();
        return true;
    }

private:
    /**
     * @brief Function executed in the worker thread. Cannot be called elsewhere.
     * @param env Execution environment.
     * @param ArkTsCallback ArkTS function (not passed in; set inside the function).
     * @param context Context environment (currently unused).
     * @param data Input data pointer.
     */
    static void callArkFunction(napi_env env, napi_value ArkTsCallback, void* context, void* data)
    {
        if (env == nullptr || data == nullptr) {
            return;
        }

        CallbackData* pCallbackData = static_cast<CallbackData*>(data);
        // 创建属性描述符
        napi_property_descriptor desc[] = {
            {"pageIndex", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"pageValue", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"pageArgs", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"pageObject", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"pageWebTag", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr}};

        napi_value pageIndex;
        napi_create_string_utf8(env, pCallbackData->action.c_str(), pCallbackData->action.length(), &pageIndex);
        desc[0].value = pageIndex;

        napi_value pageValue;
        napi_create_int32(env, pCallbackData->value, &pageValue);
        desc[1].value = pageValue;

        napi_value pageArgs;
        napi_create_string_utf8(env, pCallbackData->args.c_str(), pCallbackData->args.length(), &pageArgs);
        desc[2].value = pageArgs;

        napi_value pageObject;
        napi_create_string_utf8(env, pCallbackData->object.c_str(), pCallbackData->object.length(), &pageObject);
        desc[3].value = pageObject;

        napi_value pageWebTag;
        std::string strWebTag = pCallbackData->cbc.getWebTag();
        napi_create_string_utf8(env, strWebTag.c_str(), strWebTag.length(), &pageWebTag);
        desc[4].value = pageWebTag;

        napi_value pageAttribute;
        napi_status status =
            napi_create_object_with_properties(env, &pageAttribute, sizeof(desc) / sizeof(desc[0]), desc);
        if (status != napi_ok) {
            delete pCallbackData;
            return;
        }

        napi_value argv[1] = {pageAttribute};

        napi_value function = nullptr;
        napi_get_reference_value((napi_env)Application::g_env, (napi_ref)pCallbackData->function_ref, &function);

        napi_call_function(env, nullptr, function, 1, argv, nullptr);
        delete pCallbackData;
    }

    /**
     * @brief Worker thread function, called in asynchronous functions.
     * @param data Input data pointer.
     */
    static void executeInThread(CallbackData* data)
    {
        if (Application::g_tsfn == nullptr) {
            delete data;
            return;
        }
        // 在线程安全函数中调用
        napi_status status = napi_call_threadsafe_function(Application::g_tsfn, data, napi_tsfn_nonblocking);
        if (status != napi_ok) {
            delete data;
        }
    }

public:
    /**
     * @brief Initializes thread-safe functions, called in CordovaViewController.
     */
    static void initThreadSafeFunction()
    {
        napi_value asyncName;
        napi_create_string_utf8((napi_env)Application::g_env, "ArkTsExecutor", NAPI_AUTO_LENGTH, &asyncName);
        napi_status status =
            napi_create_threadsafe_function((napi_env)Application::g_env,
                                            nullptr, // ArkTS function (to be set later)
                                            nullptr, // Asynchronous resources（异步资源）
                                            asyncName, // Resource name（资源名称，不能传入null，否则创建安全线程失败）
                                            0,               // Infinite queue（无限队列）
                                            1,               // Initial number of threads（初始线程数）
                                            nullptr,         // thread_finalize_data
                                            nullptr,         // napi_finalize
                                            nullptr,         // context
                                            callArkFunction, // ArkTs Function
                                            &Application::g_tsfn);
        if (status != napi_ok) {
            // 创建失败后，程序会崩溃
            napi_throw_error((napi_env)Application::g_env, nullptr, "create napi_create_threadsafe_function failed");
        }
    }
};
#endif