package com.medreminder.app;

import android.content.Context;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;

import com.getcapacitor.JSObject;
import com.getcapacitor.Plugin;
import com.getcapacitor.PluginCall;
import com.getcapacitor.PluginMethod;
import com.getcapacitor.annotation.CapacitorPlugin;

import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * 语音播报药名（S-10）—— Android TextToSpeech 的极小包装。
 *
 * 为什么必须原生：Android WebView **不支持** Web Speech API 的 speechSynthesis
 * （那是 Chrome 浏览器的能力，WebView 引擎没带），纯 web 层做不到 ——
 * 这是 S-10 依赖原生的根本原因（鸿蒙侧另有 TTS Kit，届时同一 JS 出口换实现）。
 *
 * 设计约束：
 *   · 引擎初始化是**异步**的（onInit 回调），ready 之前 speak 直接优雅拒绝
 *     （返回 {ok:false}），调用方不等待 —— 播报是锦上添花，绝不能卡打卡流程；
 *   · 只说中文（Locale.SIMPLIFIED_CHINESE）—— 本应用面向中文用户；
 *   · QUEUE_ADD：连续多次 speak 排队播，不互相打断；
 *   · stop() 供「弹窗关闭时打断残留播报」用。
 */
@CapacitorPlugin(name = "MedTts")
public class TtsPlugin extends Plugin {

    private TextToSpeech tts;
    private final AtomicBoolean ready = new AtomicBoolean(false);
    private volatile int initStatus = -999;   // -999 = 回调还没回来
    private volatile int langResult = -999;

    /** 插件加载时初始化引擎。onInit 是异步回调 —— ready 置位前 speak 会优雅拒绝。 */
    @Override
    public void load() {
        initTts();
    }

    /* 引擎诊断（2026-10-20 真机反馈「引擎未就绪」后加）：

     * 只回一个 ready 布尔值不够 —— 真机上「没装中文引擎」和「引擎初始化失败」
     * 都表现为 ready=false，用户和开发者都无从判断。这里把关键码全带出去：
     *   initStatus  TextToSpeech 构造回调的码（0=SUCCESS，-1=ERROR）
     *   langResult  setLanguage 的返回值（-1/-2 = 该语言不支持，需要装语音数据）
     *   engine      系统默认引擎包名（没装时为空）
     * 并且 **失败后允许重建引擎** —— MIUI 上偶发首次 init 失败、重试即好。 */
    private void initTts() {
        try { if (tts != null) { tts.shutdown(); } } catch (Exception e) { /* ignore */ }
        ready.set(false);
        initStatus = -999;
        langResult = -999;
        try {
            tts = new TextToSpeech(getContext(), status -> {
                initStatus = status;
                if (status == TextToSpeech.SUCCESS) {
                    try { langResult = tts.setLanguage(Locale.SIMPLIFIED_CHINESE); }
                    catch (Exception e) { langResult = -998; }
                    // 缺中文语音时 setLanguage 返回 NOT_SUPPORTED_* —— 仍然置 ready，
                    // 引擎会退回默认语言念原文，好过完全哑掉（但诊断里看得见这个码）。
                    ready.set(true);
                } else {
                    ready.set(false);
                }
            });
            tts.setOnUtteranceProgressListener(new UtteranceProgressListener() {
                @Override public void onStart(String id) { }
                @Override public void onDone(String id) { }
                @Override public void onError(String id) { }
                @Deprecated @Override public void onError(String id, int code) { }
            });
        } catch (Exception e) {
            tts = null;
            ready.set(false);
        }
    }

    /* 重建节流：失败重试最多每 5 秒一次，避免每次 speak 都重建引擎 */
    private long lastReinit = 0;
    private boolean tryReinit() {
        long now = System.currentTimeMillis();
        if (now - lastReinit < 5000) return false;
        lastReinit = now;
        initTts();
        return true;
    }

    private void putDiag(JSObject r) {
        r.put("ready", ready.get());
        r.put("initStatus", initStatus);
        r.put("langResult", langResult);
        String eng = "";
        try { if (tts != null) eng = tts.getDefaultEngine() == null ? "" : tts.getDefaultEngine(); }
        catch (Exception e) { /* ignore */ }
        r.put("engine", eng);
    }

    /** 播报一段文本。返回 {ok, ready, ...诊断} —— ok=false 时调用方静默忽略（锦上添花原则）。 */
    @PluginMethod
    public void speak(PluginCall call) {
        String text = call.getString("text", "");
        if (text == null || text.trim().isEmpty()) {
            JSObject r = new JSObject(); r.put("ok", false); putDiag(r);
            call.resolve(r); return;
        }
        if (!ready.get()) {
            /* 没就绪 → 尝试重建一次（节流），仍不行就如实回诊断码 */
            tryReinit();
            JSObject r = new JSObject(); r.put("ok", false); putDiag(r);
            call.resolve(r); return;
        }
        try {
            tts.speak(text, TextToSpeech.QUEUE_ADD, null, "med-" + System.currentTimeMillis());
            JSObject r = new JSObject(); r.put("ok", true); putDiag(r);
            call.resolve(r);
        } catch (Exception e) {
            JSObject r = new JSObject(); r.put("ok", false); putDiag(r);
            call.resolve(r);
        }
    }

    /** 打断播报（提醒弹窗被处理/关闭时用，别让语音拖在后台念完）。 */
    @PluginMethod
    public void stop(PluginCall call) {
        try { if (tts != null) tts.stop(); } catch (Exception e) { /* ignore */ }
        call.resolve();
    }

    /** 引擎状态（调试/诊断用）。fail=true 时若未就绪会尝试重建一次。 */
    @PluginMethod
    public void status(PluginCall call) {
        boolean force = Boolean.TRUE.equals(call.getBoolean("reinit", false));
        if (force && !ready.get()) tryReinit();
        JSObject r = new JSObject();
        r.put("ttsAvailable", tts != null);
        putDiag(r);
        call.resolve(r);
    }
}
