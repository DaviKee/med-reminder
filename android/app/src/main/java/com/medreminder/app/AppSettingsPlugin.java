package com.medreminder.app;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;

import com.getcapacitor.Plugin;
import com.getcapacitor.PluginCall;
import com.getcapacitor.PluginMethod;
import com.getcapacitor.annotation.CapacitorPlugin;

/**
 * 极小原生插件：把用户送到本应用的通知设置页。
 *
 * 为什么必须自己写：Capacitor 官方没有「打开系统设置」的 API
 * （@capacitor/app 只有 exitApp/getInfo/getState/getLaunchUrl/minimizeApp + 监听器）。
 * LocalNotifications 里只有一个 changeExactNotificationSetting()，那是针对
 * SCHEDULE_EXACT_ALARM 精确闹钟授权的，不能用来开关通知总开关。
 *
 * 权限被拒后系统不再弹窗，只能由用户去设置里手动打开 —— 没有这个跳转，
 * 用户就得自己在系统设置里翻，这是真机上反馈的实际问题。
 */
@CapacitorPlugin(name = "AppSettings")
public class AppSettingsPlugin extends Plugin {

    /** 跳到本应用的通知设置页。Android 8.0+ 可直接定位到通知设置，更低版本退到应用详情页。 */
    @PluginMethod
    public void openNotificationSettings(PluginCall call) {
        String pkg = getContext().getPackageName();
        Intent intent;
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                intent = new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
                intent.putExtra(Settings.EXTRA_APP_PACKAGE, pkg);
            } else {
                intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                intent.setData(Uri.parse("package:" + pkg));
            }
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            getContext().startActivity(intent);
            call.resolve();
        } catch (Exception e) {
            // 某些精简 ROM 可能没有这个 Activity，退回应用详情页再试一次
            try {
                Intent fallback = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                fallback.setData(Uri.parse("package:" + pkg));
                fallback.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                getContext().startActivity(fallback);
                call.resolve();
            } catch (Exception e2) {
                call.reject("无法打开系统设置", e2);
            }
        }
    }
}
