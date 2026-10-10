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

    /** 插件加载时初始化引擎。onInit 是异步回调 —— ready 置位前 speak 会优雅拒绝。 */
    @Override
    public void load() {
        Context ctx = getContext();
        tts = new TextToSpeech(ctx, status -> {
            if (status == TextToSpeech.SUCCESS) {
                int r = tts.setLanguage(Locale.SIMPLIFIED_CHINESE);
                // 缺中文语音时 setLanguage 返回 NOT_SUPPORTED_2XL etc. —— 仍然置 ready，
                // 引擎会退回默认语言念拼音/原文，好过完全哑掉。
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
    }

    /** 播报一段文本。返回 {ok, ready} —— ok=false 时调用方静默忽略（锦上添花原则）。 */
    @PluginMethod
    public void speak(PluginCall call) {
        String text = call.getString("text", "");
        if (text == null || text.trim().isEmpty()) {
            JSObject r = new JSObject(); r.put("ok", false); r.put("ready", ready.get());
            call.resolve(r); return;
        }
        if (!ready.get()) {
            JSObject r = new JSObject(); r.put("ok", false); r.put("ready", false);
            call.resolve(r); return;
        }
        try {
            tts.speak(text, TextToSpeech.QUEUE_ADD, null, "med-" + System.currentTimeMillis());
            JSObject r = new JSObject(); r.put("ok", true); r.put("ready", true);
            call.resolve(r);
        } catch (Exception e) {
            JSObject r = new JSObject(); r.put("ok", false); r.put("ready", ready.get());
            call.resolve(r);
        }
    }

    /** 打断播报（提醒弹窗被处理/关闭时用，别让语音拖在后台念完）。 */
    @PluginMethod
    public void stop(PluginCall call) {
        try { if (tts != null) tts.stop(); } catch (Exception e) { /* ignore */ }
        call.resolve();
    }

    /** 引擎是否就绪（调试/诊断用）。 */
    @PluginMethod
    public void status(PluginCall call) {
        JSObject r = new JSObject();
        r.put("ready", ready.get());
        r.put("ttsAvailable", tts != null);
        call.resolve(r);
    }
}
