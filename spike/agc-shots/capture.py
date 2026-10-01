# -*- coding: utf-8 -*-
"""从主项目 www/ 采集 AGC「代理提醒」申请截图（含演示数据）。

为什么不用鸿蒙模拟器：本会话模拟器启动失败（日志停在 Hypervisor 就绪后静默退出，试了 4 次，
进程消失）。改用系统 Edge + 同一份 www/ —— **界面与 App 完全一致**（都是同一套 HTML/CSS/JS）。

前置：本地已起 `python -m http.server 8899 --directory <repo>/www`
用法：python spike/agc-shots/capture.py
"""
import os
from playwright.sync_api import sync_playwright

URL = 'http://127.0.0.1:8899/'
OUT = r'C:\WorkBuddy\med-reminder\med-reminder\spike\harmony-test\screenshots-for-agc'

# 演示数据：一条"固定时刻"（对应闹钟类提醒）+ 一条"按间隔"（对应倒计时类提醒）
# ⚠️ med.times 是**分钟数数组**（不是 '08:00' 字符串）—— normTimes() 用 Number(v) 解析，
#    传字符串会静默变成"未设时刻"。480 = 08:00，1200 = 20:00。
SEED = """
try {
  if (!localStorage.getItem('medreminder.v1')) {
    localStorage.setItem('medreminder.v1', JSON.stringify({
      meds: [
        { id: 'demo-1', name: '阿司匹林肠溶片', interval: 12, mode: 'fixed', times: [480, 1200] },
        { id: 'demo-2', name: '二甲双胍', interval: 8 }
      ],
      doses: {}, notified: {}
    }));
  }
} catch (e) {}
"""

# 按文本点击"叶子元素"（click 会冒泡到卡片/按钮）
CLICK = """(txt) => {
  const all = [...document.querySelectorAll('*')];
  const cands = all.filter(el => el.children.length === 0 &&
      el.textContent.trim() === txt && el.getBoundingClientRect().width > 0);
  if (!cands.length) return 'NOTFOUND:' + txt;
  const el = cands[cands.length - 1];
  el.click();
  return 'clicked:' + txt;
}"""


def main():
    os.makedirs(OUT, exist_ok=True)
    with sync_playwright() as p:
        b = p.chromium.launch(channel='msedge', headless=True)
        # 427 × 944 CSS 像素 @3x → 输出 1281 × 2832，与模拟器截图同规格
        ctx = b.new_context(viewport={'width': 427, 'height': 944}, device_scale_factor=3)
        ctx.add_init_script(SEED)
        pg = ctx.new_page()

        def shot(name):
            pg.screenshot(path=os.path.join(OUT, name + '.png'))
            print('  [OK] ' + name + '.png')

        def click(txt):
            r = pg.evaluate(CLICK, txt)
            print('  ' + str(r))
            pg.wait_for_timeout(1600)

        pg.goto(URL, wait_until='networkidle')
        pg.wait_for_timeout(2500)
        print('title =', pg.title())

        print('[1] 今日页（含演示药品的待服项）')
        # 移除「通知未开启 · 收不到提醒」卡片（#btnPerm）——
        # 它只在**浏览器降级**时出现（没有 Capacitor 插件），真机上通知可用、不会显示。
        # 留着会让审核误判"这个 App 根本收不到提醒"。
        pg.evaluate("() => { const e = document.getElementById('btnPerm'); if (e) e.remove(); }")
        pg.wait_for_timeout(400)
        shot('今日提醒排程')

        print('[2] 药品列表')
        click('药品')
        shot('我的药品-提醒规则')

        print('[3] 编辑药品① 阿司匹林（按固定时刻 08:00 / 20:00）')
        click('阿司匹林肠溶片')
        shot('设置提醒-每日固定时刻')

        print('[4] 编辑药品② 二甲双胍（按间隔 8 小时）')
        pg.goto(URL, wait_until='networkidle')      # 重载回到今日页（避开关浮层）
        pg.wait_for_timeout(2000)
        click('药品')
        click('二甲双胍')
        shot('设置提醒-按间隔倒计时')

        b.close()
    print('完成。输出目录:', OUT)


if __name__ == '__main__':
    main()
