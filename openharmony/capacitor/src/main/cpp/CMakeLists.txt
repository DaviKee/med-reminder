# the minimum version of CMake.
cmake_minimum_required(VERSION 3.8.0)
project(MyApplication3)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(OPENSSL_LIB_PATH ${NATIVERENDER_ROOT_PATH}/../../../libs/${OHOS_ARCH})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${NATIVERENDER_ROOT_PATH}/openssl/${OHOS_ARCH}/include
                    )

#START_ADD_SUBDIRECTORY
add_subdirectory(getcapacitor)
add_subdirectory(App)
add_subdirectory(Camera)
add_subdirectory(Filesystem)
add_subdirectory(LocalNotifications)
#END_ADD_SUBDIRECTORY

add_compile_options(-Wno-deprecated-declarations)

add_library(capacitor SHARED
    Application.cpp
    rawfile_request.cpp
    HttpUrl.cpp
    napi_init.cpp
    Base64.cpp
    CallbackContext.cpp
    cJSON.cpp
    ConfigXmlParser.cpp
    CordovaBridge.cpp
    CordovaExposedJsApi.cpp
    CordovaPreferences.cpp
    CordovaViewController.cpp
    CordovaWebViewEngine.cpp
    NativeToJsMessageQueue.cpp
    SystemCookieManager.cpp
    PluginManager.cpp
    PluginResult.cpp
    XMLParser.cpp
    XMLNode.cpp
    Thread.cpp
    CoreHarmony/CoreHarmony.cpp
    Socket.cpp
    SSLSocket.cpp
    ConnPool.cpp
    FileCache.cpp
    MemPool.cpp
    Lock.cpp
    TsCordovaPlugin/TsCordovaPlugin.cpp
    FileSystem.cpp
)

target_link_libraries(capacitor PUBLIC
    librawfile.z.so
    libace_napi.z.so
    libohweb.so
    libhilog_ndk.z.so
    libbundle_ndk.z.so
    libnative_rdb_ndk.z.so
    libnet_connection.so
    libohfileuri.so
    libohenvironment.so
    libohcamera.so
    libimage_ndk.z.so
    libimage_receiver_ndk.z.so
    libnative_image.so
    libpixelmap_ndk.z.so
    libimage_source_ndk.z.so
    libimage_packer_ndk.z.so
    libz.so
    libavplayer.so
    libnative_media_core.so
    libohvibrator.z.so
    libicu.so
    libohcommonevent.so
    libohbattery_info.so
    libohpreferences.so
    libhiappevent_ndk.z.so
    libudmf.so
    libpasteboard.so
    libnative_buffer.so
    libohimage.so
    libimage_source.so
    libmedia_asset_manager.so
    libimage_packer.so
    libohsensor.so
    ${OPENSSL_LIB_PATH}/libssl.so.3
    ${OPENSSL_LIB_PATH}/libcrypto.so.3
    "-Wl,--whole-archive"
    getcapacitor
    App
    Camera
    Filesystem
    LocalNotifications
    "-Wl,--no-whole-archive"
)