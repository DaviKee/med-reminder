package com.medreminder.app;

import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.getcapacitor.BridgeActivity;

public class MainActivity extends BridgeActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // 必须写在 super.onCreate() 之前：
        // super 内部最后会调 load()，而 load() 才用 bridgeBuilder 创建 Bridge。
        // registerPlugin() 只是往 bridgeBuilder 里加，写在 super 之后插件不会生效。
        registerPlugin(AppSettingsPlugin.class);
        super.onCreate(savedInstanceState);

        // ---- 安全区适配（2026-10-09，小米 13 真机反馈：内容顶进状态栏） ----
        //
        // 根因：targetSdk 35+ 在 Android 15+ 上被系统**强制全面屏**（edge-to-edge），
        // WebView 会画到状态栏底下；而 Android WebView 不上报 CSS 的
        // env(safe-area-inset-top)（恒为 0），CSS 那道防线形同虚设。
        // 旧设备（Android 14-）没有强制全面屏，WebView 本来就在状态栏下方，所以没暴露。
        //
        // 修法：把系统栏 insets 变成 WebView 的 margin —— 让出状态栏/导航栏，
        // 让出的区域露出窗口背景（splash_bg 近黑，与旧设备 statusBarColor 的表现一致）。
        //
        // 自适应的关键：DecorView 在「非全面屏」路径上会先消费掉 insets 再往下游派发，
        // 所以旧设备这里收到的是 0（行为不变）；只有被强制全面屏的设备才会拿到
        // 非零 insets —— 这正是「只在该让的地方让」。
        View web = bridge.getWebView();
        if (web != null) {
            ViewCompat.setOnApplyWindowInsetsListener(web, (v, windowInsets) -> {
                Insets bars = windowInsets.getInsets(
                        WindowInsetsCompat.Type.systemBars()
                                | WindowInsetsCompat.Type.displayCutout());
                ViewGroup.MarginLayoutParams lp =
                        (ViewGroup.MarginLayoutParams) v.getLayoutParams();
                if (lp != null
                        && (lp.topMargin != bars.top || lp.bottomMargin != bars.bottom)) {
                    lp.topMargin = bars.top;
                    lp.bottomMargin = bars.bottom;
                    v.setLayoutParams(lp);
                }
                return WindowInsetsCompat.CONSUMED;
            });
        }
    }
}
