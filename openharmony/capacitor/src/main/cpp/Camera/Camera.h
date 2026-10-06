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

#ifndef CAPACITOR_PLUGIN_CAMERA_H
#define CAPACITOR_PLUGIN_CAMERA_H

#include "getcapacitor/Plugin.h"
#include "cJSON.h"

class Camera : public Plugin {
    struct CameraSource {
        static constexpr const char *strPrompt = "PROMPT";
        static constexpr const char *strCamera = "CAMERA";
        static constexpr const char *strPhotos = "PHOTOS";
    };

    struct CameraResultType {
        static constexpr const char *strUri = "uri";
        static constexpr const char *strBase64 = "base64";
        static constexpr const char *strDataUrl = "dataUrl";
    };

    struct CameraPermissionType {
        static constexpr const char *strCamera = "camera";
        static constexpr const char *strPhotos = "photos";
    };

    struct CameraPermissionState {
        static constexpr const char *strPrompt = "prompt";
        static constexpr const char *strPromptWithRationale = "prompt-with-rationale";
        static constexpr const char *strGranted = "granted";
        static constexpr const char *strDenied = "denied";
        static constexpr const char *strLimited = "limited";
        static constexpr const char *strNoPermission = "no-permission-args";
    };

    struct Error {
        static constexpr const char *errNoCamera = "Device doesn't have a camera available";
        static constexpr const char *errCameraFail = "Unable to launch camera application";
        static constexpr const char *errPhotosFail = "Unable to launch photo application";
        static constexpr const char *errUserCancel = "User cancelled photos app";
        static constexpr const char *errFileFail = "Unable to create photo on disk";
        static constexpr const char *errNoCameraPermission = "User denied access to camera";
    };

    struct ImageOptionsBase {
        int32_t quality;
        int32_t width;                     // 宽度
        int32_t height;                    // 高度
        bool correctOrientation;       // 是否要自动将图像“向上”旋转以校正纵向模式
        std::string presentationStyle; // 相机的呈现风格
    };

    struct ImageOptions : public ImageOptionsBase {
        bool allowEditing;
        std::string resultType;         // 数据应如何返回
        bool saveToGallery;             // 是否将照片保存到相册
        std::string source;             // 照片来源
        std::string direction;          // 摄像机方向
        bool webUseInput;               // 使用PWA元素体验还是文件输入
        std::string promptLabelHeader;  // 显示提示时使用的文本值
        std::string promptLabelCancel;  // 显示提示时使用的文本值
        std::string promptLabelPhoto;   // 显示提示时使用的文本值
        std::string promptLabelPicture; // 显示提示时使用的文本值
        int limit = 1;
    };

    struct GalleryImageOptions : public ImageOptionsBase {
        int limit; // 用户可选择的最大图片数量
    };

    struct PermissionStatus {
        std::string cameraStatus; //'prompt' | 'prompt-with-rationale' | 'granted' | 'denied' || 'limited'
        std::string photosStatus; //'prompt' | 'prompt-with-rationale' | 'granted' | 'denied' || 'limited'
    };

    struct permissions {
        std::string CameraPluginPermissions; //'prompt' | 'prompt-with-rationale' | 'granted' | 'denied' || 'limited'
    };

    enum class MediaType { IMAGE = 0, VIDEO = 1, IMAGE_VIDEO = 3 };

    enum class DataType { DATA_URL = 0, FILE_URL = 1, BASE64 = 3 };

    enum class RequestPermissionType {
        REQUEST_NULL_PERMISSION = 0,
        REQUEST_CAMERAANDPHOTOS_PERMISSION = 1,
        REQUEST_CAMERA_PERMISSION = 2,
        REQUEST_PHOTOS_PERMISSION = 3
    };

    enum class SourceType { PHOTO_LIBRARY = 0, CAMERA = 1, SAVED_PHOTO_ALBUM = 2 };

    int m_nQuality;
    int m_nMediaType;
    int m_nDestType;
    int m_nSrcType;
    RequestPermissionType m_nRequestPermissionType;
    ImageOptions m_imageOptions;
    GalleryImageOptions m_galleryImageOptions;
    PluginCall m_call;

public:
    Camera() {}
    ~Camera(){};

    void getPhoto(PluginCall &call);
    void pickImages(PluginCall &call);
    void checkPermissions(PluginCall &call);
    void requestPermissions(PluginCall &call);
    void pickLimitedLibraryPhotos(PluginCall &call);
    void getLimitedLibraryPhotos(PluginCall &call);
    void onArKTsResult(PluginCall &call);

private:
    void getPhotoHandle(cJSON *json);
    void pickImagesHandle(cJSON *json);
    void checkPermissionsHandle(cJSON *json);
    void requestPermissionsHandle(cJSON *json);
    void permissionsResultSend(cJSON *json);
    void parsePhotoOptions(cJSON *json);
    void parseGalleryPhotoOptions(cJSON *json);
    void cameraAction(cJSON *json);
    void selectPhotos(cJSON *json);
    bool checkCameraDevice();
    std::string getPageIndexBySource(const std::string &source);
    void getCompressParams(bool &isCompress, long &compressSize, std::string &showToast);
    cJSON *buildCompressArgsJson(bool isCompress, long compressSize, const std::string &showToast, int quality);
    cJSON *buildOperationArgsJson();
    cJSON *buildGalleryOperationArgsJson();
    cJSON *buildDialogArgsJson();
    bool handlePhotoResultType(const char *result, cJSON *pResultData);
    bool handleBase64Result(const char *path, cJSON *resultJson);
    bool handleDataUrlResult(const char *path, cJSON *resultJson);
    bool handleUriResult(const char *path, cJSON *resultJson);
    std::string getWebPathFromPath(const std::string &path, std::string &uri);
};

int cJSON_GetObjectItemIntCheck(cJSON *object, const char *string, int def);
bool cJSON_GetObjectItemBool(cJSON *object, const char *string, bool def);
const char *cJSON_GetObjectItemString(cJSON *object, const char *string, const char *def);

#endif // CAPACITOR_PLUGIN_CAMERA_H
