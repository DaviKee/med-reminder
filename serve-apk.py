#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
手机扫码安装用的临时下载服务。
只监听局域网网卡，下载完 Ctrl+C / 关掉即可。

  python serve-apk.py [port]
"""
import http.server
import os
import socketserver
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
APK = "med-reminder-debug.apk"
APK_PATH = os.path.join(ROOT, APK)
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080


def host_ip():
    """取本机局域网 IP（不发包到外网，仅建 UDP 套接字读路由）。"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"
    finally:
        s.close()


def human(n):
    return "%.1f MB" % (n / 1024.0 / 1024.0)


PAGE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0A0A0A">
<title>定时服药提醒 · 下载安装</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
  body{
    background:#0A0A0A;color:#fff;min-height:100vh;
    font-family:-apple-system,BlinkMacSystemFont,"Noto Sans SC","PingFang SC","Microsoft YaHei",sans-serif;
    display:flex;flex-direction:column;align-items:center;justify-content:center;
    padding:32px 24px calc(32px + env(safe-area-inset-bottom));
  }
  .cap{
    width:76px;height:76px;border-radius:9999px;background:#FF7A17;
    position:relative;margin-bottom:26px;
  }
  .cap::after{
    content:"";position:absolute;left:50%;top:50%;width:34px;height:3px;
    background:#0A0A0A;border-radius:2px;transform:translate(-50%,-50%) rotate(-45deg);
  }
  h1{font-size:24px;line-height:32px;font-weight:500;letter-spacing:-.4px;text-align:center}
  .meta{
    margin-top:10px;font-size:13px;line-height:20px;color:#7D8187;text-align:center;
    font-family:ui-monospace,Consolas,monospace;letter-spacing:.4px;
  }
  .btn{
    display:block;width:100%;max-width:340px;margin-top:34px;
    background:#FF7A17;color:#0A0A0A;text-decoration:none;text-align:center;
    font-size:16px;font-weight:500;line-height:22px;padding:17px 20px;border-radius:9999px;
  }
  .btn:active{opacity:.85}
  .note{
    margin-top:24px;max-width:340px;background:#191919;border:1px solid #212327;
    border-radius:12px;padding:16px 18px;
  }
  .note p{font-size:13px;line-height:21px;color:#7D8187}
  .note p+p{margin-top:9px}
  .note b{color:#DADBDF;font-weight:500}
  .accent{color:#FF7A17}
  .foot{margin-top:22px;font-size:12px;line-height:18px;color:#5F6368;text-align:center}
  .weixin{display:none;margin-top:14px;padding:12px 14px;border-radius:10px;
    background:rgba(255,122,23,.1);border:1px solid rgba(255,122,23,.35);
    font-size:13px;line-height:20px;color:#FF7A17;max-width:340px}
</style>
</head>
<body>
  <div class="cap"></div>
  <h1>定时服药提醒</h1>
  <p class="meta">v1.0 · __SIZE__ · Android 8.0+</p>

  <a class="btn" href="/apk">下载安装包</a>

  <div class="weixin" id="wx">
    检测到你在微信里打开，微信会拦截 APK。<br>请点右上角「···」→「在浏览器打开」。
  </div>

  <div class="note">
    <p><b>安装时</b>系统会提示「未知来源应用」，选<b>允许</b>即可 —— 这是自己打包的调试签名包，正常提示。</p>
    <p><b>装好后务必</b><span class="accent">允许通知权限</span>，并把电池设为「不优化 / 无限制」，否则锁屏时收不到提醒。</p>
  </div>

  <p class="foot">下载完就可以关掉电脑上的服务了</p>

<script>
  if (/MicroMessenger/i.test(navigator.userAgent)) {
    document.getElementById('wx').style.display = 'block';
  }
</script>
</body>
</html>
"""


class Handler(http.server.BaseHTTPRequestHandler):
    server_version = "MedReminder/1.0"

    def _send(self, code, body, ctype, extra=None):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        path = self.path.split("?")[0]

        if path in ("/", "/index.html"):
            if not os.path.exists(APK_PATH):
                self._send(404, b"APK not found", "text/plain; charset=utf-8")
                return
            page = PAGE.replace("__SIZE__", human(os.path.getsize(APK_PATH)))
            self._send(200, page.encode("utf-8"), "text/html; charset=utf-8")
            return

        if path == "/apk" or path.endswith(".apk"):
            if not os.path.exists(APK_PATH):
                self._send(404, b"APK not found", "text/plain; charset=utf-8")
                return
            with open(APK_PATH, "rb") as f:
                data = f.read()
            self._send(
                200,
                data,
                "application/vnd.android.package-archive",
                {"Content-Disposition": 'attachment; filename="%s"' % APK},
            )
            return

        self._send(404, b"Not Found", "text/plain; charset=utf-8")

    def log_message(self, fmt, *args):
        sys.stderr.write("  %s  %s\n" % (self.address_string(), fmt % args))


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    import socket

    if not os.path.exists(APK_PATH):
        sys.exit("找不到 %s" % APK_PATH)

    ip = host_ip()
    with Server((ip, PORT), Handler) as httpd:
        print("=" * 52)
        print("  定时服药提醒 · 手机扫码安装")
        print("=" * 52)
        print("  电脑上看说明：  http://%s:%d/" % (ip, PORT))
        print("  手机扫码后打开：http://%s:%d/" % (ip, PORT))
        print("  APK 直链：     http://%s:%d/apk" % (ip, PORT))
        print("  文件大小：     %s" % human(os.path.getsize(APK_PATH)))
        print("-" * 52)
        print("  手机需与本机同一 WiFi。装好后 Ctrl+C 关掉即可。")
        print("=" * 52)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n已关闭下载服务。")
