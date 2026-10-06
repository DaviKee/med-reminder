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

#include "Camera.h"
#include "FileCache.h"
#include "CordovaViewController.h"
#include "cJSON.h"
#include <filemanagement/file_uri/oh_file_uri.h>
#include <ohcamera/camera_manager.h>

REGISTER_CAP_PLUGIN(Camera, Camera)

REGISTER_PLUGIN_METHOD(Camera, getPhoto, PluginMethod::RETURN_PROMISE)
void Camera::getPhoto(PluginCall &call)
{
    m_call = call;
    cameraAction(call.getData());
}

REGISTER_PLUGIN_METHOD(Camera, pickImages, PluginMethod::RETURN_PROMISE)
void Camera::pickImages(PluginCall &call)
{
    m_call = call;
    selectPhotos(call.getData());
}

REGISTER_PLUGIN_METHOD(Camera, pickLimitedLibraryPhotos, PluginMethod::RETURN_PROMISE)
void Camera::pickLimitedLibraryPhotos(PluginCall &call)
{
    m_call = call;

    cJSON *requestArgs = cJSON_CreateObject();
    cJSON_AddNumberToObject(requestArgs, "quality", 100);
    cJSON_AddNumberToObject(requestArgs, "width", 1920);
    cJSON_AddNumberToObject(requestArgs, "height", 1080);
    cJSON_AddNumberToObject(requestArgs, "limit", 50);
    cJSON_AddBoolToObject(requestArgs, "correctOrientation", true);
    cJSON_AddStringToObject(requestArgs, "presentationStyle", "fullscreen");

    selectPhotos(requestArgs);
}

REGISTER_PLUGIN_METHOD(Camera, getLimitedLibraryPhotos, PluginMethod::RETURN_PROMISE)
void Camera::getLimitedLibraryPhotos(PluginCall &call)
{
    m_call = call;

    cJSON *requestArgs = cJSON_CreateObject();
    cJSON_AddNumberToObject(requestArgs, "quality", 100);
    cJSON_AddNumberToObject(requestArgs, "width", 1920);
    cJSON_AddNumberToObject(requestArgs, "height", 1080);
    cJSON_AddNumberToObject(requestArgs, "limit", 1);
    cJSON_AddBoolToObject(requestArgs, "correctOrientation", true);
    cJSON_AddStringToObject(requestArgs, "presentationStyle", "fullscreen");
    selectPhotos(requestArgs);
}

REGISTER_PLUGIN_METHOD(Camera, checkPermissions, PluginMethod::RETURN_PROMISE)
void Camera::checkPermissions(PluginCall &call)
{
    m_call = call;

    const char *permissions[] = {"ohos.permission.CAMERA", "ohos.permission.PHOTOS"};
    int count = sizeof(permissions) / sizeof(permissions[0]);
    cJSON *pArrayArgs = cJSON_CreateStringArray(permissions, count);
    if (!pArrayArgs) {
        return;
    }

    char *pPageArgs = cJSON_Print(pArrayArgs);
    executeArkTs("./MediaAction/CheckCameraPermission/HarmonyPermission", 0, pPageArgs, "Camera", call);

    cJSON_Delete(pArrayArgs);
}

REGISTER_PLUGIN_METHOD(Camera, requestPermissions, PluginMethod::RETURN_PROMISE)
void Camera::requestPermissions(PluginCall &call)
{
    m_call = call;

    cJSON *pPermissionArray = cJSON_GetObjectItem(call.getData(), "permissions");
    int nArray = 0;
    if (pPermissionArray) {
        nArray = cJSON_GetArraySize(pPermissionArray);
    }

    bool cameraReq = false;
    bool photosReq = false;

    for (int i = 0; i < nArray; ++i) {
        cJSON *it = cJSON_GetArrayItem(pPermissionArray, i);
        if (!it || !it->valuestring)
            continue;
        std::string permission = it->valuestring;
        if (permission == CameraPermissionType::strCamera) {
            cameraReq = true;
        } else if (permission == CameraPermissionType::strPhotos) {
            photosReq = true;
        }
    }

    if (cameraReq && photosReq) {
        m_nRequestPermissionType = RequestPermissionType::REQUEST_CAMERAANDPHOTOS_PERMISSION;
    } else if (cameraReq) {
        m_nRequestPermissionType = RequestPermissionType::REQUEST_CAMERA_PERMISSION;
    } else if (photosReq) {
        m_nRequestPermissionType = RequestPermissionType::REQUEST_PHOTOS_PERMISSION;
    } else {
        m_nRequestPermissionType = RequestPermissionType::REQUEST_NULL_PERMISSION;
    }

    // 根据请求类型构造要申请的权限列表
    std::vector<const char *> perms;
    if (m_nRequestPermissionType == RequestPermissionType::REQUEST_CAMERA_PERMISSION) {
        perms.push_back("ohos.permission.CAMERA");
    } else if (m_nRequestPermissionType == RequestPermissionType::REQUEST_PHOTOS_PERMISSION) {
        perms.push_back("ohos.permission.PHOTOS");
    } else if (m_nRequestPermissionType == RequestPermissionType::REQUEST_CAMERAANDPHOTOS_PERMISSION) {
        perms.push_back("ohos.permission.CAMERA");
        perms.push_back("ohos.permission.PHOTOS");
    }

    int count = static_cast<int>(perms.size());
    cJSON *pArrayArgs = cJSON_CreateStringArray(perms.data(), count);
    if (!pArrayArgs)
        return;

    char *pPageArgs = cJSON_Print(pArrayArgs);
    executeArkTs("./MediaAction/CheckCameraPermission/HarmonyPermission", 2, pPageArgs, "Camera", call);

    cJSON_Delete(pArrayArgs);
}

REGISTER_PLUGIN_METHOD(Camera, onArKTsResult, PluginMethod::RETURN_PROMISE)
void Camera::onArKTsResult(PluginCall &call)
{
    std::string content = "";
    cJSON *pContent = cJSON_GetObjectItem(call.getData(), "content");
    if (pContent) {
        content = pContent->valuestring;
    }
    cJSON *json = cJSON_GetObjectItem(call.getData(), "result");
    int code = 0;
    if (cJSON_GetObjectItem(call.getData(), "code")) {
        code = cJSON_GetObjectItem(call.getData(), "code")->valueint;
    }
    if (content == "getPhoto") {
        if (code == 1001) {
            m_call.reject(Error::errCameraFail);
        } else if (code == 1002) {
            m_call.reject(Error::errPhotosFail);
        } else if (code == 1003) {
            m_call.reject(Error::errUserCancel);
        } else if (code == 1004) {
            m_call.reject(Error::errFileFail);
        } else if (code == 1005) {
            m_call.reject(Error::errNoCameraPermission);
        } else {
            getPhotoHandle(json);
        }
    } else if (content == "pickImages") {
        if (code == 1002) {
            m_call.reject(Error::errPhotosFail);
        } else if (code == 1003) {
            m_call.reject(Error::errUserCancel);
        } else if (code == 1004) {
            m_call.reject(Error::errFileFail);
        } else {
            pickImagesHandle(json);
        }
    } else if (content == "checkPermission") {
        checkPermissionsHandle(json);
    } else if (content == "requestPermission") {
        requestPermissionsHandle(json);
    }
}

void Camera::cameraAction(cJSON *json)
{
    if (!json || json->type != cJSON_Object) {
        m_call.reject("Args Error!");
        return;
    }

    if (!checkCameraDevice()) {
        m_call.reject(Error::errNoCamera);
        return;
    }

    parsePhotoOptions(json);

    std::string strPageIndex = getPageIndexBySource(m_imageOptions.source);
    if (strPageIndex.empty()) {
        m_call.reject("invalid source type!");
        return;
    }

    bool isImageCompress = true;
    long lngCompressSize = 4 * 1024 * 1024;
    std::string strCompressShowToast;
    getCompressParams(isImageCompress, lngCompressSize, strCompressShowToast);

    cJSON *pCompressArgsJson =
        buildCompressArgsJson(isImageCompress, lngCompressSize, strCompressShowToast, m_imageOptions.quality);
    cJSON *pOperationArgsJson = buildOperationArgsJson();
    cJSON *pDialogArgsJson = nullptr;
    if (m_imageOptions.source == CameraSource::strPrompt) {
        pDialogArgsJson = buildDialogArgsJson();
    }

    cJSON *pJson = cJSON_CreateObject();
    cJSON_AddStringToObject(pJson, "method", "getPhoto");
    cJSON_AddItemToObject(pJson, "compressArgs", pCompressArgsJson);
    cJSON_AddItemToObject(pJson, "operationArgs", pOperationArgsJson);
    if (pDialogArgsJson) {
        cJSON_AddItemToObject(pJson, "promptConfig", pDialogArgsJson);
    }

    char *pPageArgs = cJSON_Print(pJson);
    executeArkTs(strPageIndex, (int)MediaType::IMAGE, pPageArgs, "Camera", m_call);

    cJSON_Delete(pJson);
}

void Camera::selectPhotos(cJSON *json)
{
    parseGalleryPhotoOptions(json);

    cJSON *pJsonArgs = cJSON_CreateObject();
    bool isImageCompress = true;
    int64_t lngCompressSize = 4 * 1024 * 1024;
    std::string strCompressShowToast = "";
    getCompressParams(isImageCompress, lngCompressSize, strCompressShowToast);

    cJSON *pCompressArgsJson =
        buildCompressArgsJson(isImageCompress, lngCompressSize, strCompressShowToast, m_galleryImageOptions.quality);
    cJSON *pOptionArgs = buildGalleryOperationArgsJson();
    cJSON_AddStringToObject(pJsonArgs, "method", "pickImages");
    cJSON_AddItemToObject(pJsonArgs, "compressArgs", pCompressArgsJson);
    cJSON_AddItemToObject(pJsonArgs, "operationArgs", pOptionArgs);

    char *pPageArgs = cJSON_Print(pJsonArgs);
    executeArkTs("./MediaAction/MediaAction/SelectMedia", 0, pPageArgs, "Camera", m_call);

    cJSON_Delete(pJsonArgs);
}

void Camera::parsePhotoOptions(cJSON *json)
{
    if (!json || json->type != cJSON_Object) {
        m_call.reject("Args Error!");
        return;
    }

    const int DEFAULT_QUALITY = 75;
    const int DEFAULT_MIN_QUALITY = 0;
    const int DEFAULT_MAX_QUALITY = 100;
    m_imageOptions.quality = cJSON_GetObjectItemIntCheck(json, "quality", DEFAULT_QUALITY);
    if (m_imageOptions.quality < DEFAULT_MIN_QUALITY)
        m_imageOptions.quality = DEFAULT_MIN_QUALITY;
    if (m_imageOptions.quality > DEFAULT_MAX_QUALITY)
        m_imageOptions.quality = DEFAULT_MAX_QUALITY;

    m_imageOptions.allowEditing = cJSON_GetObjectItemBool(json, "allowEditing", true);
    m_imageOptions.resultType = cJSON_GetObjectItemString(json, "resultType", "Uri");
    m_imageOptions.saveToGallery = cJSON_GetObjectItemBool(json, "saveToGallery", false);
    m_imageOptions.width = cJSON_GetObjectItemIntCheck(json, "width", 0);
    m_imageOptions.height = cJSON_GetObjectItemIntCheck(json, "height", 0);
    m_imageOptions.correctOrientation = cJSON_GetObjectItemBool(json, "correctOrientation", true);
    m_imageOptions.source = cJSON_GetObjectItemString(json, "source", "PROMPT");
    m_imageOptions.direction = cJSON_GetObjectItemString(json, "direction", "REAR");
    m_imageOptions.presentationStyle = cJSON_GetObjectItemString(json, "presentationStyle", "fullscreen");
    m_imageOptions.webUseInput = cJSON_GetObjectItemBool(json, "webUseInput", false);
    m_imageOptions.promptLabelHeader = cJSON_GetObjectItemString(json, "promptLabelHeader", "");
    m_imageOptions.promptLabelCancel = cJSON_GetObjectItemString(json, "promptLabelCancel", "");
    m_imageOptions.promptLabelPhoto = cJSON_GetObjectItemString(json, "promptLabelPhoto", "");
    m_imageOptions.promptLabelPicture = cJSON_GetObjectItemString(json, "promptLabelPicture", "");
}

void Camera::parseGalleryPhotoOptions(cJSON *json)
{
    if (!json || json->type != cJSON_Object) {
        m_call.reject("Args Error!");
        return;
    }

    const int DEFAULT_QUALITY = 75;
    const int DEFAULT_MIN_QUALITY = 0;
    const int DEFAULT_MAX_QUALITY = 100;
    m_galleryImageOptions.quality = cJSON_GetObjectItemIntCheck(json, "quality", DEFAULT_QUALITY);
    if (m_galleryImageOptions.quality < DEFAULT_MIN_QUALITY)
        m_imageOptions.quality = DEFAULT_MIN_QUALITY;
    if (m_galleryImageOptions.quality > DEFAULT_MAX_QUALITY)
        m_imageOptions.quality = DEFAULT_MAX_QUALITY;

    m_galleryImageOptions.width = cJSON_GetObjectItemIntCheck(json, "width", 0);
    m_galleryImageOptions.height = cJSON_GetObjectItemIntCheck(json, "height", 0);
    m_galleryImageOptions.correctOrientation = cJSON_GetObjectItemBool(json, "correctOrientation", true);
    m_galleryImageOptions.presentationStyle = cJSON_GetObjectItemString(json, "presentationStyle", "fullscreen");
    m_galleryImageOptions.limit = cJSON_GetObjectItemIntCheck(json, "limit", 0);
}

bool Camera::checkCameraDevice()
{
    Camera_Manager *cameraManager = nullptr;
    Camera_ErrorCode ret = OH_Camera_GetCameraManager(&cameraManager);
    if (!cameraManager || ret != CAMERA_OK) {
        return false;
    }
    uint32_t size = 0;
    Camera_Device *cameras = nullptr;
    ret = OH_CameraManager_GetSupportedCameras(cameraManager, &cameras, &size);
    bool result = (cameras && size > 0 && ret == CAMERA_OK);
    delete[] cameras;
    return result;
}

std::string Camera::getPageIndexBySource(const std::string &source)
{
    if (source == CameraSource::strCamera) {
        return "./ImageCompress/CameraAction/StartCamera";
    } else if (source == CameraSource::strPhotos) {
        return "./MediaAction/MediaAction/SelectMedia";
    } else if (source == CameraSource::strPrompt) {
        return "./MediaAction/CameraSelectDialog/NotificationCameraAndPhotos";
    }
    return "";
}

void Camera::getCompressParams(bool &isCompress, long &compressSize, std::string &showToast)
{
    CordovaViewController *cordovaViewController = (CordovaViewController *)(Application::g_cordovaViewController);
    if (cordovaViewController) {
        cordovaViewController->getCameraImageCompress(isCompress, compressSize, showToast);
    }
}

cJSON *Camera::buildCompressArgsJson(bool isCompress, long compressSize, const std::string &showToast, int quality)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "Quality", quality);
    if (isCompress)
        cJSON_AddTrueToObject(json, "CameraImageCompress");
    else
        cJSON_AddFalseToObject(json, "CameraImageCompress");
    cJSON_AddNumberToObject(json, "CompressImageSize", compressSize);
    cJSON_AddStringToObject(json, "CameraCompressShowToast", showToast.c_str());
    return json;
}

cJSON *Camera::buildOperationArgsJson()
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "AllowEditing", m_imageOptions.allowEditing);
    cJSON_AddBoolToObject(json, "SaveToGallery", m_imageOptions.saveToGallery);
    cJSON_AddNumberToObject(json, "Width", m_imageOptions.width);
    cJSON_AddNumberToObject(json, "Height", m_imageOptions.height);
    cJSON_AddBoolToObject(json, "CorrectOrientation", m_imageOptions.correctOrientation);
    cJSON_AddNumberToObject(json, "Limit", m_imageOptions.limit);
    cJSON_AddStringToObject(json, "Direction", m_imageOptions.direction.c_str());
    return json;
}

cJSON *Camera::buildGalleryOperationArgsJson()
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "CorrectOrientation", m_galleryImageOptions.correctOrientation);
    cJSON_AddNumberToObject(json, "Width", m_galleryImageOptions.width);
    cJSON_AddNumberToObject(json, "Height", m_galleryImageOptions.height);
    cJSON_AddNumberToObject(json, "Limit", m_galleryImageOptions.limit);
    return json;
}

cJSON *Camera::buildDialogArgsJson()
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "PromptLabelHeader", m_imageOptions.promptLabelHeader.c_str());
    cJSON_AddStringToObject(json, "PromptLabelPicture", m_imageOptions.promptLabelPicture.c_str());
    cJSON_AddStringToObject(json, "PromptLabelPhoto", m_imageOptions.promptLabelPhoto.c_str());
    cJSON_AddStringToObject(json, "PromptLabelCancel", m_imageOptions.promptLabelCancel.c_str());
    return json;
}

void Camera::getPhotoHandle(cJSON *json)
{
    if (!json || json->type != cJSON_Array) {
        m_call.reject("get photos error!");
        return;
    }

    int count = cJSON_GetArraySize(json);
    if (count <= 0) {
        m_call.reject("no image selected!");
        return;
    }

    cJSON *pDataJson = cJSON_GetArrayItem(json, 0);
    if (!pDataJson) {
        m_call.reject("get photos error!");
        return;
    }

    const char *result = cJSON_GetObjectItemString(pDataJson, "path", "");
    if (!result || strlen(result) == 0) {
        m_call.reject("no image selected!");
        return;
    }

    cJSON *pResultData = cJSON_CreateObject();
    if (!pResultData) {
        m_call.reject("internal error!");
        return;
    }

    if (!handlePhotoResultType(result, pResultData)) {
        cJSON_Delete(pResultData);
        return;
    }

    cJSON_AddStringToObject(pResultData, "format", "jpeg");
    cJSON_AddNumberToObject(pResultData, "saved", m_imageOptions.saveToGallery);
    cJSON_AddStringToObject(pResultData, "exif", cJSON_GetObjectItemString(pDataJson, "exif", ""));

    m_call.resolve(pResultData);
    cJSON_Delete(pResultData);
}

void Camera::pickImagesHandle(cJSON *json)
{
    if (!json || json->type != cJSON_Array) {
        m_call.reject("get photos error!");
        return;
    }

    int count = cJSON_GetArraySize(json);
    if (count <= 0) {
        m_call.reject("get photos error!");
        return;
    }

    cJSON *pArray = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *pDataJson = cJSON_GetArrayItem(json, i);
        if (!pDataJson || pDataJson->type != cJSON_Object)
            continue;

        std::string strPath = cJSON_GetObjectItemString(pDataJson, "path", "");
        std::string strUri = strPath;
        std::string strWebPath = getWebPathFromPath(strPath, strUri);

        cJSON *pGalleryPhoto = cJSON_CreateObject();
        cJSON_AddStringToObject(pGalleryPhoto, "path", strUri.c_str());
        cJSON_AddStringToObject(pGalleryPhoto, "webPath", strWebPath.c_str());
        cJSON_AddStringToObject(pGalleryPhoto, "format", "JPEG");
        cJSON_AddStringToObject(pGalleryPhoto, "exif", cJSON_GetObjectItemString(pDataJson, "exif", ""));
        cJSON_AddItemToArray(pArray, pGalleryPhoto);
    }

    cJSON *pResultData = cJSON_CreateObject();
    cJSON_AddItemToObject(pResultData, "photos", pArray);
    m_call.resolve(pResultData);
    cJSON_Delete(pResultData);
}

void Camera::checkPermissionsHandle(cJSON *json) { permissionsResultSend(json); }

void Camera::requestPermissionsHandle(cJSON *json) { permissionsResultSend(json); }

void Camera::permissionsResultSend(cJSON *json)
{
    if (!json || json->type != cJSON_Array) {
        m_call.reject("result error!");
        return;
    }

    std::string cameraGrantStatus = CameraPermissionState::strNoPermission;
    std::string photosGrantStatus = CameraPermissionState::strNoPermission;

    int n = cJSON_GetArraySize(json);
    for (int i = 0; i < n; ++i) {
        cJSON *item = cJSON_GetArrayItem(json, i);
        if (!item || item->type != cJSON_Object)
            continue;

        std::string permissionName = cJSON_GetObjectItemString(item, "permission", "");
        int result = 0;
        if (cJSON_GetObjectItem(item, "result")) {
            result = cJSON_GetObjectItem(item, "result")->valueint;
        }

        if (permissionName == "ohos.permission.CAMERA") {
            cameraGrantStatus = (result == 1) ? CameraPermissionState::strGranted : CameraPermissionState::strDenied;
        } else if (permissionName == "ohos.permission.PHOTOS") {
            photosGrantStatus = CameraPermissionState::strGranted; // 只要有该项就 granted
        }
    }

    cJSON *pRetJson = cJSON_CreateObject();
    cJSON_AddStringToObject(pRetJson, "camera", cameraGrantStatus.c_str());
    cJSON_AddStringToObject(pRetJson, "photos", photosGrantStatus.c_str());
    m_call.resolve(pRetJson);
    cJSON_Delete(pRetJson);
}

bool Camera::handlePhotoResultType(const char *result, cJSON *pResultData)
{
    if (m_imageOptions.resultType == CameraResultType::strBase64) {
        return handleBase64Result(result, pResultData);
    } else if (m_imageOptions.resultType == CameraResultType::strDataUrl) {
        return handleDataUrlResult(result, pResultData);
    } else if (m_imageOptions.resultType == CameraResultType::strUri) {
        return handleUriResult(result, pResultData);
    }
    return false;
}

bool Camera::handleBase64Result(const char *path, cJSON *resultJson)
{
    long fileSize = FileCache::getFileSize(path);
    if (fileSize < 0) {
        m_call.reject("invalid image file!");
        return false;
    }
    char *buf = (char *)malloc(fileSize);
    if (!buf) {
        m_call.reject("invalid image file size!");
        return false;
    }
    bool ok = false;
    if (FileCache::readFile(path, (unsigned char *)buf, fileSize)) {
        std::string strResult = CBase64::encode(buf, fileSize);
        cJSON_AddStringToObject(resultJson, "base64String", strResult.c_str());
        ok = true;
    } else {
        m_call.reject("read image file failed!");
    }
    free(buf);
    return ok;
}

bool Camera::handleDataUrlResult(const char *path, cJSON *resultJson)
{
    long fileSize = FileCache::getFileSize(path);
    if (fileSize < 0) {
        m_call.reject("invalid image file!");
        return false;
    }
    char *buf = (char *)malloc(fileSize);
    if (!buf) {
        m_call.reject("invalid image file size!");
        return false;
    }
    bool ok = false;
    if (FileCache::readFile(path, (unsigned char *)buf, fileSize)) {
        std::string strResult = CBase64::encode(buf, fileSize);
        std::string dataUrl = "data:image/jpeg;base64," + strResult;
        cJSON_AddStringToObject(resultJson, "dataUrl", dataUrl.c_str());
        ok = true;
    } else {
        m_call.reject("read image file failed!");
    }
    free(buf);
    return ok;
}

bool Camera::handleUriResult(const char *path, cJSON *resultJson)
{
    std::string strFilePath = path;
    std::string strUri = strFilePath;
    if (strFilePath.find("file://") == 0) {
        char *pathResult = NULL;
        FileManagement_ErrCode ret = OH_FileUri_GetPathFromUri(strFilePath.c_str(), strFilePath.length(), &pathResult);
        if (ret == 0 && pathResult != NULL) {
            strFilePath = pathResult;
            free(pathResult);
        }
    } else {
        char *pathResult = NULL;
        FileManagement_ErrCode ret = OH_FileUri_GetUriFromPath(strFilePath.c_str(), strFilePath.length(), &pathResult);
        if (ret == 0 && pathResult != NULL) {
            strUri = pathResult;
            free(pathResult);
        }
    }
    cJSON_AddStringToObject(resultJson, "path", strUri.c_str());
    strFilePath = "https://localhost" + strFilePath;
    cJSON_AddStringToObject(resultJson, "webPath", strFilePath.c_str());
    return true;
}

std::string Camera::getWebPathFromPath(const std::string &path, std::string &uri)
{
    std::string webPath;
    if (path.find("file://") == 0) {
        char *pathResult = nullptr;
        FileManagement_ErrCode ret = OH_FileUri_GetPathFromUri(path.c_str(), path.length(), &pathResult);
        if (ret == 0 && pathResult != nullptr) {
            webPath = "https://localhost" + std::string(pathResult);
            free(pathResult);
        } else {
            webPath = "https://localhost" + path;
        }
    } else {
        char *pathResult = nullptr;
        FileManagement_ErrCode ret = OH_FileUri_GetUriFromPath(path.c_str(), path.length(), &pathResult);
        if (ret == 0 && pathResult != nullptr) {
            uri = pathResult;
            free(pathResult);
        }
        webPath = "https://localhost" + path;
    }
    return webPath;
}

int cJSON_GetObjectItemIntCheck(cJSON *object, const char *string, int def)
{
    cJSON *it = cJSON_GetObjectItem(object, string);
    if (it && it->type == cJSON_Number) {
        if (it->valueint >= 0)
            return it->valueint;
        else
            return def;
    }
    return def;
}

bool cJSON_GetObjectItemBool(cJSON *object, const char *string, bool def)
{
    cJSON *it = cJSON_GetObjectItem(object, string);
    if (it && it->type == cJSON_True) {
        return true;
    }
    if (it && it->type == cJSON_False) {
        return false;
    }

    return def;
}

const char *cJSON_GetObjectItemString(cJSON *object, const char *string, const char *def)
{
    cJSON *it = cJSON_GetObjectItem(object, string);
    if (it && it->type == cJSON_String && it->valuestring)
        return it->valuestring;
    return def;
}
