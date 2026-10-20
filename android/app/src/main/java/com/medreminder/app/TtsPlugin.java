package com.medreminder.app;

import android.content.Context;
import android.speech.tts.TextToSpeech;

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
 * （那是 Chrome 浏览器的能力，WebView 引擎没带），纯 web 层做不到。
 *
 * 2026-10-20 真机（小米）反馈「引擎未就绪」后重写初始化逻辑：
 *   · **可重试**：onInit 失败不等于永久失败（系统引擎刚被唤醒、TTS 服务重启都会失败一次）。
 *     speak 时若未就绪，会重新 init 一次；JS 侧 800ms 后自动补念。
 *   · **状态可诊断**：status() 返回 initStatus / langStatus / 引擎个数 / 默认引擎名 ——
 *     「未就绪」时能立刻看出是"系统没装引擎"还是"引擎装了但初始化失败"。
 *   · 一切失败都 resolve（不 reject）：播报是锦上添花，绝不能卡住打卡流程。
 */
@CapacitorPlugin(name = "MedTts")
public class TtsPlugin extends Plugin {

    private TextToSpeech tts;
    private final AtomicBoolean ready = new AtomicBoolean(false);
    private volatile int initStatus = -1;     // TextToSpeech.SUCCESS(0) / ERROR(-1)
    private volatile int langStatus = 99;     // setLanguage 返回值（99 = 没跑到）
    private volatile boolean initing = false;

    @Override
    public void load() {
        initTts();
    }

    /** 初始化（或重建）引擎。失败不抛，只记录 initStatus —— speak 时会再试。 */
    private synchronized void initTts() {
        if (initing) return;
        initing = true;
        ready.set(false);
        try {
            if (tts != null) {
                try { tts.shutdown(); } catch (Exception ignored) { }
                tts = null;
            }
            // 用 ApplicationContext：Activity 销毁重建（转屏/返回）不该影响引擎生命周期
            Context ctx = getContext().getApplicationContext();
            tts = new TextToSpeech(ctx, status -> {
                initStatus = status;
                if (status == TextToSpeech.SUCCESS) {
                    try {
                        langStatus = tts.setLanguage(Locale.SIMPLIFIED_CHINESE);
                    } catch (Exception e) {
                        langStatus = 98;      // 调用本身抛了
                    }
                    // 即使缺中文语音也置 ready：引擎会退回默认语言念，好过完全哑掉
                    ready.set(true);
                } else {
                    ready.set(false);
                }
                initing = false;
            });
        } catch (Exception e) {
            initStatus = -2;                  // 构造就抛了
            initing = false;
        }
    }

    /** 播报。返回 {ok, ready, reason, initStatus} —— ok=false 时调用方会补念一次（见 tts.js）。 */
    @PluginMethod
    public void speak(PluginCall call) {
        String text = call.getString("text", "");
        if (text == null || text.trim().isEmpty()) {
            resolve(call, false, "empty");
            return;
        }
        if (!ready.get()) {
            initTts();                        // 未就绪 → 重新初始化（异步），本次先拒
            resolve(call, false, "not-ready");
            return;
        }
        try {
            tts.speak(text, TextToSpeech.QUEUE_ADD, null, "med-" + System.currentTimeMillis());
            resolve(call, true, "");
        } catch (Exception e) {
            ready.set(false);                 // 引擎坏了 → 下次 speak 重新 init
            resolve(call, false, "speak-failed");
        }
    }

    private void resolve(PluginCall call, boolean ok, String reason) {
        JSObject r = new JSObject();
        r.put("ok", ok);
        r.put("ready", ready.get());
        r.put("reason", reason);
        r.put("initStatus", initStatus);
        call.resolve(r);
    }

    @PluginMethod
    public void stop(PluginCall call) {
        try { if (tts != null) tts.stop(); } catch (Exception e) { /* ignore */ }
        call.resolve();
    }

    /** 诊断：引擎装没装、初始化成没成、默认引擎是谁。 */
    @PluginMethod
    public void status(PluginCall call) {
        JSObject r = new JSObject();
        r.put("ready", ready.get());
        r.put("ttsAvailable", tts != null);
        r.put("initStatus", initStatus);
        r.put("langStatus", langStatus);
        try {
            r.put("engines", tts != null && tts.getEngines() != null ? tts.getEngines().size() : 0);
            r.put("defaultEngine", tts != null && tts.getDefaultEngine() != null ? tts.getDefaultEngine() : "");
        } catch (Exception e) {
            r.put("engines", 0);
            r.put("defaultEngine", "");
        }
        call.resolve(r);
    }
}
