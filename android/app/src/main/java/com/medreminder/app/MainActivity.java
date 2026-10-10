package com.medreminder.app;

import android.os.Bundle;
import android.view.View;
import android.webkit.WebView;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.getcapacitor.BridgeActivity;

public class MainActivity extends BridgeActivity {

    /* 上次注入的 insets —— 相同就不重复注入（每次布局变化都会回调） */
    private int lastTop = -1, lastBottom = -1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // 必须写在 super.onCreate() 之前：
        // super 内部最后会调 load()，而 load() 才用 bridgeBuilder 创建 Bridge。
        // registerPlugin() 只是往 bridgeBuilder 里加，写在 super 之后插件不会生效。
        registerPlugin(AppSettingsPlugin.class);
        registerPlugin(TtsPlugin.class);   // S-10 语音播报（Android TTS；鸿蒙侧同 JS 出口换实现）
        super.onCreate(savedInstanceState);

        // ---- 纯净状态栏（2026-10-09 小米 13，v1.5.25 的黑条 margin 方案被否） ----
        //
        // 根因链：targetSdk 35+ 在 Android 15+ 被系统**强制全面屏**，WebView 画到
        // 状态栏底下；而 Android WebView 不上报 CSS 的 env(safe-area-inset-*)（恒 0），
        // CSS 自己让不了位。
        //
        // 方案：WebView 保持全出血，把真实 insets **注入成 CSS 变量**，
        //   --sat = 顶部（状态栏+挖孔），--sab = 底部（导航条）
        // 由 www/css/app.css 的 `.phone` border-top / tabbar padding-bottom 消费。
        // 好处：状态栏底下就是 App 自己的背景（随明暗主题变色），浑然一体 ——
        // 这才是「纯净状态栏」；v1.5.25 的 margin 会让出一条死黑，被秦老师否了。
        //
        // 自适应：非全面屏设备（Android 14-）DecorView 先消费 insets 再派发，
        // 这里收到 0 → 变量 0 → 布局与从前完全一致。
        //
        // ⚠️ 注入完顺手调 window.__onSafeArea：boot 里的 applyTheme 跑得比这次注入早，
        //    它需要 --sat 有值才能决定「要不要跟随主题切状态栏图标颜色」。
        View web = bridge.getWebView();
        if (web != null) {
            ViewCompat.setOnApplyWindowInsetsListener(web, (v, windowInsets) -> {
                Insets bars = windowInsets.getInsets(
                        WindowInsetsCompat.Type.systemBars()
                                | WindowInsetsCompat.Type.displayCutout());
                if (bars.top != lastTop || bars.bottom != lastBottom) {
                    lastTop = bars.top;
                    lastBottom = bars.bottom;
                    final int top = bars.top, bottom = bars.bottom;
                    v.post(() -> {
                        try {
                            ((WebView) v).evaluateJavascript(
                                "document.documentElement.style.setProperty('--sat','"
                                    + top + "px');"
                                  + "document.documentElement.style.setProperty('--sab','"
                                    + bottom + "px');"
                                  + "if(window.__onSafeArea)try{window.__onSafeArea("
                                    + top + "," + bottom + ")}catch(e){}",
                                null);
                        } catch (Exception e) {
                            // 页面尚未就绪等异常：insets 下次变化会再试
                        }
                    });
                }
                return windowInsets;
            });
        }
    }
}
