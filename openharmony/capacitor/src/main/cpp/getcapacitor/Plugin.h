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

#ifndef MyApplication_PLUGIN_H
#define MyApplication_PLUGIN_H

#include <map>
#include <thread>
#include "PluginCall.h"
#include "Application.h"

typedef void *(*Constructor)();
class CCapacitorPluginFactory {
public:
    static void registerClass(const std::string &pluginName, const std::string &className, Constructor constructor)
    {
        constructors()[className] = constructor;
        PluginNames()[className] = pluginName;
    }

    static void *createObject(const std::string &className)
    {
        Constructor constructor = nullptr;
        if (constructors().find(className) != constructors().end())
            constructor = constructors().find(className)->second;
        if (constructor == nullptr) {
            return nullptr;
        }

        return (*constructor)();
    }

    static std::string getPluginName(const std::string &className)
    {
        if (PluginNames().find(className) != PluginNames().end()) {
            return PluginNames()[className];
        }
        return "";
    }

private:
    inline static std::map<std::string, Constructor> &constructors()
    {
        static std::map<std::string, Constructor> instance;
        return instance;
    }

    inline static std::map<std::string, std::string> &PluginNames()
    {
        static std::map<std::string, std::string> classNameToPluginName;
        return classNameToPluginName;
    }
};

#define REGISTER_CAP_PLUGIN(plugin_name, class_name)                                                                   \
    class class_name##Handler {                                                                                        \
    public:                                                                                                            \
        class_name##Handler()                                                                                          \
        {                                                                                                              \
            CCapacitorPluginFactory::registerClass(#plugin_name, #plugin_name, class_name##Handler::creatObjFunc);     \
        }                                                                                                              \
        static void *creatObjFunc() { return new class_name(); }                                                       \
    };                                                                                                                 \
    class_name##Handler class_name##Handler;

struct PluginMethod {
    static const std::string RETURN_PROMISE;
    static const std::string RETURN_CALLBACK;
    static const std::string RETURN_NONE;
};

class Plugin {
    std::mutex m_mutex;
    std::map<std::string, std::vector<PluginCall> > m_mapEventListeners;
    std::map<std::string, std::vector<cJSON *> > m_mapRetainedEventArguments;
    // 回调数据结构
    struct CallbackData {
        std::string action;
        int value;
        std::string args;
        std::string object;
        PluginCall call;
        napi_ref function_ref;
    };

public:
    Plugin() = default;
    ~Plugin() = default;
    virtual void load() {}
    void addEventListener(const std::string &strEventName, const PluginCall &call);
    void removeEventListener(const std::string &strEventName, const PluginCall &call);
    void notifyListeners(const std::string &strEventName, cJSON *pData, const bool retainUntilConsumed);
    void notifyListeners(const std::string &strEventName, cJSON *pData);
    bool hasListeners(const std::string &strEventName);
    void sendRetainedArgumentsForEvent(const std::string &strEventName);

    virtual void addListener(PluginCall &call);
    virtual void removeListener(PluginCall &call);
    virtual void removeAllListeners(PluginCall &call);
    void removeAllListeners();
    /**
     * @brief
     * OpenHarmony Capacitor的权限设计理念和原Capacitor权限设计理念不同，权限和业务是强相关性的，
     * 所以权限申请和授权下放到具体的插件方法，不支持类似于原来权限注解机制，有插件的方法自行决定是否需要弹窗申请相关权限（ArkTS侧有权限封装），而不是一揽子申请权限。
     * 但是为兼容原Capacitor前端代码，Plugin基类提供的checkPermissions和requestPermissions接口的resolve，如果插件需要具体的权限，由插件子类继承该方法实现权限的检查和申请；
     * 但是不希望提前申请，因此推荐的方法是：需要权限是再弹窗申请，无需提前弹窗获取用户授权
     */
    virtual void checkPermissions(PluginCall &call);
    virtual void requestPermissions(PluginCall &call);

    virtual void handleOnStart(const std::string &strWebTag) {}
    virtual void handleOnPageStart(const std::string &strWebTag) {}
    virtual void handleOnEnd(const std::string &strWebTag) {}
    virtual void handleOnResume(const std::string &strWebTag) {}
    virtual void handleOnPause(const std::string &strWebTag) {}
    virtual void handleOnDestroy(const std::string &strWebTag) {}

    /**
     * @brief Calls an ArkTS function from the C++ side. If the C++ side cannot complete all tasks and needs to invoke
     * an ArkTS function, this method enables cross-language communication from C++ to ArkTS. This function is a
     * synchronous call.
     * @param strAction Identifier for the ArkTS function call (maps to a specific function on the ArkTS side).
     * @param nValue Integer parameter passed from C++ to ArkTS.
     * @param pArgs String parameter passed from C++ to ArkTS (typically a JSON string).
     * @param pObject Plugin name on the C++ side.
     * @param call Callback object for execution results.
     * @return true if successful，false if failed
     */
    virtual bool executeArkTs(const std::string &strAction, const int nValue, const char *pArgs, const char *pObject,
                              PluginCall &call)
    {
        void *pFunctionRefresh = Application::getFunctionRefresh(call.getWebTag());
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
        napi_create_string_utf8((napi_env)Application::g_env, strAction.c_str(), strlen(strAction.c_str()), &pageIndex);
        desc[0].value = pageIndex;

        napi_value pageValue;
        int nPageValue = nValue;
        napi_create_int32((napi_env)Application::g_env, nPageValue, &pageValue);
        desc[1].value = pageValue;

        napi_value pageArgs = nullptr;
        const char *pPageArgs = pArgs;
        napi_create_string_utf8((napi_env)Application::g_env, pPageArgs, strlen(pPageArgs), &pageArgs);
        desc[2].value = pageArgs;

        napi_value pageObject = nullptr;
        const char *pPageObject = pObject;
        napi_create_string_utf8((napi_env)Application::g_env, pPageObject, strlen(pPageObject), &pageObject);
        desc[3].value = pageObject;

        napi_value pageWebTag = nullptr;
        std::string strWebTag = call.getWebTag();
        napi_create_string_utf8((napi_env)Application::g_env, strWebTag.c_str(), strlen(strWebTag.c_str()),
                                &pageWebTag);
        desc[4].value = pageWebTag;

        napi_value pageAttribute = nullptr;
        auto ret = napi_create_object_with_properties((napi_env)Application::g_env, &pageAttribute,
                                                      sizeof(desc) / sizeof(desc[0]), desc);
        if (ret != napi_ok) {
            call.reject("napi_create_object_with_properties error");
            return false;
        }

        napi_value argv[1] = {pageAttribute};

        napi_value function = nullptr;
        napi_get_reference_value((napi_env)Application::g_env, (napi_ref)pFunctionRefresh, &function);

        napi_call_function((napi_env)Application::g_env, nullptr, function, 1, argv, NULL);

        return true;
    }

    /**
     * @brief Calls an ArkTS function from the C++ side. If the C++ side cannot complete all tasks and needs to invoke
     * an ArkTS function, this method enables cross-language communication from C++ to ArkTS. This function is an
     * asynchronous call.
     * @param strAction Identifier for the ArkTS function call (maps to a specific function on the ArkTS side).
     * @param nValue Integer parameter passed from C++ to ArkTS.
     * @param pArgs String parameter passed from C++ to ArkTS (typically a JSON string).
     * @param pObject Plugin name on the C++ side.
     * @param cbc Callback object for execution results.
     * @return true if successful，false if failed
     */
    virtual bool executeArkTsAsync(const std::string &strAction, const int nValue, const char *pArgs,
                                   const char *pObject, PluginCall &call)
    {
        void *pFunctionRefresh = Application::getFunctionRefresh(call.getWebTag());
        if (pFunctionRefresh == nullptr) {
            return false;
        }

        CallbackData *data = new CallbackData{
            strAction, nValue, pArgs ? pArgs : "", pObject ? pObject : "", call, (napi_ref)pFunctionRefresh};

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
    static void callArkFunction(napi_env env, napi_value ArkTsCallback, void *context, void *data)
    {
        if (env == nullptr || data == nullptr) {
            return;
        }

        CallbackData *pCallbackData = static_cast<CallbackData *>(data);
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
        std::string strWebTag = pCallbackData->call.getWebTag();
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
    static void executeInThread(CallbackData *data)
    {
        if (Plugin::g_tsfn == nullptr) {
            delete data;
            return;
        }
        // 在线程安全函数中调用
        napi_status status = napi_call_threadsafe_function(Plugin::g_tsfn, data, napi_tsfn_nonblocking);
        if (status != napi_ok) {
            delete data;
        }
    }
    static napi_threadsafe_function g_tsfn;

public:
    /**
     * @brief Initializes thread-safe functions, called in CordovaViewController.
     */
    static void initThreadSafeFunction()
    {
        napi_value asyncName;
        napi_create_string_utf8((napi_env)Application::g_env, "CapacitorExecArkTs", NAPI_AUTO_LENGTH, &asyncName);
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
                                            &Plugin::g_tsfn);
        if (status != napi_ok) {
            // 创建失败后，程序会崩溃
            napi_throw_error((napi_env)Application::g_env, nullptr, "create napi_create_threadsafe_function failed");
        }
    }
};

using PluginMethodFun = void (Plugin::*)(const PluginCall &);
class CCapacitorPluginMethod {
public:
    template <typename PluginType>
    static void registerPluginMethod(const std::string &strClassName, const std::string &strMethodName,
                                     void (PluginType::*method)(PluginCall &),
                                     const std::string &strReturnType = PluginMethod::RETURN_PROMISE)
    {
        PluginMethodFun methodPointer =
            reinterpret_cast<PluginMethodFun>(static_cast<void (Plugin::*)(PluginCall &)>(method));
        std::pair<PluginMethodFun, std::string> methodPair(methodPointer, strReturnType);
        classFunctions()[strClassName][strMethodName] = methodPair;
    }

    static void invoke(Plugin *plugin, const std::string &strClassName, const std::string &strMethodName,
                       const PluginCall &call)
    {
        PluginMethodFun methodPointer = nullptr;
        if (classFunctions().find(strClassName) != classFunctions().end()) {
            std::map<std::string, std::pair<PluginMethodFun, std::string> > &mapMethodFun =
                classFunctions().find(strClassName)->second;
            if (mapMethodFun.find(strMethodName) != mapMethodFun.end()) {
                methodPointer = mapMethodFun[strMethodName].first;
                (plugin->*methodPointer)(call);
            }
        }
    }

    static bool getMethods(const std::string &strClassName,
                           std::map<std::string, std::pair<PluginMethodFun, std::string> > &mapMethods)
    {
        if (classFunctions().find(strClassName) != classFunctions().end()) {
            mapMethods = classFunctions().find(strClassName)->second;
            return true;
        }
        return false;
    }

private:
    inline static std::map<std::string, std::map<std::string, std::pair<PluginMethodFun, std::string> > > &
    classFunctions()
    {
        static std::map<std::string, std::map<std::string, std::pair<PluginMethodFun, std::string> > > instance;
        return instance;
    }
};


#define REGISTER_PLUGIN_METHOD(class_name, method_name, return_type, ...)                                              \
    namespace {                                                                                                        \
    struct class_name##_##method_name##_registrar {                                                                    \
        static bool registerMethod()                                                                                   \
        {                                                                                                              \
            std::string js_name = #method_name;                                                                        \
            constexpr bool has_alias = sizeof(#__VA_ARGS__) > 2;                                                       \
            if constexpr (has_alias) {                                                                                 \
                js_name = #__VA_ARGS__;                                                                                \
                js_name = js_name.substr(1, js_name.length() - 2);                                                     \
            }                                                                                                          \
            CCapacitorPluginMethod::registerPluginMethod<class_name>(#class_name, js_name, &class_name::method_name,   \
                                                                     return_type);                                     \
            return true;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        static inline bool registered = registerMethod();                                                              \
    };                                                                                                                 \
    }

#endif // MyApplication_PLUGIN_H
