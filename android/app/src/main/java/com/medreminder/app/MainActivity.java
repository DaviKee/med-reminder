package com.medreminder.app;

import android.os.Bundle;

import com.getcapacitor.BridgeActivity;

public class MainActivity extends BridgeActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // 必须写在 super.onCreate() 之前：
        // super 内部最后会调 load()，而 load() 才用 bridgeBuilder 创建 Bridge。
        // registerPlugin() 只是往 bridgeBuilder 里加，写在 super 之后插件不会生效。
        registerPlugin(AppSettingsPlugin.class);
        registerPlugin(TtsPlugin.class);   // S-10 语音播报（Android TTS；鸿蒙侧同 JS 出口换实现）
        super.onCreate(savedInstanceState);

        // ---- 安全区（2026-10-13 定稿）----
        // Capacitor 8 内置 SystemBars 插件（com.getcapacitor.plugin.SystemBars）已经
        // 全权处理 insets：新 WebView 走 CSS 变量透传（--safe-area-inset-*，已除
        // density、带键盘修正），旧 WebView 走物理 padding（变量为 0）。
        //
        // ⚠️ 历史教训（v1.5.24~31 三轮）：自己再挂一套 OnApplyWindowInsetsListener
        // 注入 --sat/--sab，一是与内置处理**双重让位**，二是注入的物理像素没除
        // density（小米 13 ≈3x）→ 预留空间变三倍 —— 真机反馈"上下削去太多"。
        // 结论：**不要再碰 insets**，CSS 直接消费 --safe-area-inset-*（app.css）。
    }
}
