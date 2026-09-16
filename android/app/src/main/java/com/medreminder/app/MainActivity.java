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
        super.onCreate(savedInstanceState);
    }
}
