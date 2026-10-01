# -*- coding: utf-8 -*-
"""检查本机是否满足 HarmonyOS 模拟器运行条件，并诊断上次启动失败的原因。

⚠️ 本机命令通道限制：reg.exe 禁用、wmic.exe 在程序黑名单 → 一律用 Python 标准库。
⚠️ 内存等数值**不要用整除**（曾把 15.8 GiB 截成 15 而误报"内存不足"）。
"""
import os
import sys
import glob
import ctypes
import platform
import shutil
import winreg
from datetime import datetime

SEP = "=" * 64


def head(t):
    print("\n" + SEP)
    print(t)
    print(SEP)


def reg_get(path, name):
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, path) as k:
            return winreg.QueryValueEx(k, name)[0]
    except FileNotFoundError:
        return "(不存在)"
    except Exception as e:
        return "(读取失败: %s)" % e


def reg_subkeys(path):
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, path) as k:
            n = winreg.QueryInfoKey(k)[0]
            return [winreg.EnumKey(k, i) for i in range(n)]
    except Exception:
        return []


# ---------------- 1. 系统 ----------------
head("1. 操作系统")
print("  OS          :", platform.system(), platform.release())
print("  版本号      :", platform.version())
print("  平台架构    :", platform.machine())
try:
    v = platform.win32_ver()
    print("  Win32       :", v)
except Exception as e:
    print("  Win32       : err", e)
print("  逻辑处理器  :", os.cpu_count(), "核")
print("  CPU 型号    :", platform.processor())
try:
    print("  CPU 标识    :",
          reg_get(r"HARDWARE\DESCRIPTION\System\CentralProcessor\0", "ProcessorNameString"))
except Exception as e:
    print("  CPU 标识    : err", e)

# ---------------- 2. 内存（不整除） ----------------
head("2. 内存")


class MEMORYSTATUSEX(ctypes.Structure):
    _fields_ = [("dwLength", ctypes.c_ulong),
                ("dwMemoryLoad", ctypes.c_ulong),
                ("ullTotalPhys", ctypes.c_ulonglong),
                ("ullAvailPhys", ctypes.c_ulonglong),
                ("ullTotalPageFile", ctypes.c_ulonglong),
                ("ullAvailPageFile", ctypes.c_ulonglong),
                ("ullTotalVirtual", ctypes.c_ulonglong),
                ("ullAvailVirtual", ctypes.c_ulonglong),
                ("ullAvailExtendedVirtual", ctypes.c_ulonglong)]


m = MEMORYSTATUSEX()
m.dwLength = ctypes.sizeof(MEMORYSTATUSEX)
ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m))
GB = 1024 ** 3
print("  物理内存总量: %.2f GB" % (m.ullTotalPhys / GB))
print("  当前可用    : %.2f GB" % (m.ullAvailPhys / GB))
print("  占用率      : %d%%" % m.dwMemoryLoad)

# ---------------- 3. 磁盘 ----------------
head("3. 磁盘空间")
for d in ("C:\\", "D:\\"):
    if os.path.exists(d):
        try:
            u = shutil.disk_usage(d)
            print("  %s 总 %.2f GB / 可用 %.2f GB" % (d, u.total / GB, u.free / GB))
        except Exception as e:
            print("  %s err %s" % (d, e))

# ---------------- 4. 虚拟化 / Hyper-V ----------------
head("4. 虚拟化与 Hyper-V（模拟器的硬依赖）")
print("  hypervisorlaunchtype        :",
      reg_get(r"SYSTEM\CurrentControlSet\Control\Session Manager", "(无此键则看 bcdedit)"))
print("  DeviceGuard.EnableVirtualizationBasedSecurity:",
      reg_get(r"SYSTEM\CurrentControlSet\Control\DeviceGuard", "EnableVirtualizationBasedSecurity"))
print("  DeviceGuard.SecurityServicesRunning:",
      reg_get(r"SYSTEM\CurrentControlSet\Control\DeviceGuard", "SecurityServicesRunning"))

print("\n  关键服务（Start: 2=自动 3=手动 4=禁用；不存在=未安装）:")
for svc, desc in [("vmcompute", "Hyper-V 主机计算服务"),
                  ("HvHost", "HV 主机服务"),
                  ("vmms", "Hyper-V 虚拟机管理")]:
    st = reg_get(r"SYSTEM\CurrentControlSet\Services\%s" % svc, "Start")
    img = reg_get(r"SYSTEM\CurrentControlSet\Services\%s" % svc, "ImagePath")
    print("    %-10s %-22s Start=%s  %s" % (svc, desc, st, str(img)[:60]))

# ---------------- 5. DevEco / 模拟器 ----------------
head("5. DevEco Studio 与模拟器")
emu = r"D:\Program Files\Huawei\DevEco Studio\tools\emulator\Emulator.exe"
print("  Emulator.exe:", "存在" if os.path.exists(emu) else "缺失")
if os.path.exists(emu):
    print("    大小 %.1f MB  修改时间 %s" % (
        os.path.getsize(emu) / 1024 ** 2,
        datetime.fromtimestamp(os.path.getmtime(emu)).strftime("%Y-%m-%d %H:%M")))

local = os.environ.get("LOCALAPPDATA", "")
print("  LOCALAPPDATA:", local)

# 镜像
img_root = os.path.join(local, "Huawei", "Sdk", "system-image")
if os.path.isdir(img_root):
    print("\n  系统镜像:")
    for p in glob.glob(os.path.join(img_root, "*", "*", "*")):
        if os.path.isdir(p):
            try:
                sz = sum(os.path.getsize(os.path.join(dp, f))
                         for dp, _, fs in os.walk(p) for f in fs)
            except Exception:
                sz = -1
            print("    %s  %.2f GB" % (p.replace(local, "%LOCALAPPDATA%"), sz / GB))
else:
    print("  ⚠ 镜像目录不存在:", img_root)

# 已部署实例
dep = os.path.join(local, "Huawei", "Emulator", "deployed")
if os.path.isdir(dep):
    print("\n  已部署实例:")
    for p in glob.glob(os.path.join(dep, "*")):
        print("    ", os.path.basename(p))
else:
    print("  ⚠ 部署目录不存在:", dep)

# ---------------- 6. 模拟器日志 / 崩溃 ----------------
head("6. 模拟器日志与崩溃痕迹（找真因）")
cands = []
for pat in ["Huawei/Emulator/**/*.log", "Huawei/Emulator/**/*.dmp",
            "Huawei/Emulator/**/*.txt", "Huawei/Sdk/**/*.dmp"]:
    cands += glob.glob(os.path.join(local, *pat.split("/")), recursive=True)
cands = [c for c in cands if os.path.isfile(c)]
cands.sort(key=lambda p: os.path.getmtime(p), reverse=True)
if not cands:
    print("  （未找到 .log/.dmp/.txt）")
for p in cands[:12]:
    print("  %s  %8.1f KB  %s" % (
        datetime.fromtimestamp(os.path.getmtime(p)).strftime("%m-%d %H:%M"),
        os.path.getsize(p) / 1024, p.replace(local, "%LOCALAPPDATA%")))

# ---------------- 7. 残留锁 ----------------
head("7. 残留锁 / 进程")
locks = glob.glob(os.path.join(local, "Huawei", "Emulator", "**", "*.lock"), recursive=True)
print("  .lock 文件:", len(locks))
for p in locks[:8]:
    print("    ", p.replace(local, "%LOCALAPPDATA%"))

print("\n检查完成。")
