# -*- coding: utf-8 -*-
"""让 build-apk.sh 的版本回写不再破坏文件行尾（替换掉会转 CRLF→LF 的 sed -i）"""
import io

P = r'D:\WorkBuddy\MedReminder\med-reminder\build-apk.sh'
t = io.open(P, encoding='utf-8', newline='').read()
assert t.count('\r\n') == 0, 'build-apk.sh 应为 LF'

FAILED = []


def sub(t, old, new, label):
    if old in t:
        print('  ok :', label)
        return t.replace(old, new, 1)
    print('  !! :', label)
    FAILED.append(label)
    return t


HELPER = '''# ---------- 保留行尾的原地替换 ----------
# ⚠️ 千万不要用 `sed -i` 改这些文件：Git Bash（MSYS）的 sed 在文本模式下
# 会把**整个文件**的 CRLF 转成 LF。2026-09-21 实测确认，后果是每次构建都
# 静默重写 app.js / build.gradle 的行尾 —— 而本项目已经多次被「行尾漂移」
# 坑到（补丁锚点莫名失配，排查很久）。所以这里用纯 bash 实现，逐行重建，
# 先探测原文件行尾再原样写回。
#   用法：set_line <文件> <行首前缀> <替换后的整行>
set_line() {
  _f="$1"; _pat="$2"; _new="$3"
  IFS= read -r _first < "$_f" || true
  case "$_first" in
    *$'\\r') _nl=$'\\r\\n' ;;
    *)       _nl=$'\\n'   ;;
  esac
  : > "$_f.tmp"
  while IFS= read -r _line || [ -n "$_line" ]; do
    _line="${_line%$'\\r'}"
    if [ "$_line" != "${_line#"$_pat"}" ]; then _line="$_new"; fi
    printf '%s%s' "$_line" "$_nl" >> "$_f.tmp"
  done < "$_f"
  mv "$_f.tmp" "$_f"
}

'''

t = sub(t,
      '# 构建日期**一律取系统当天**，并回写进 app.js。',
      HELPER + '# 构建日期**一律取系统当天**，并回写进 app.js。',
      '插入 set_line 帮助函数')

t = sub(t,
      """sed -i.bak "s/^\\(  var APP_BUILD = \\).*/\\1'$BUILDDATE';/" www/js/app.js
rm -f www/js/app.js.bak""",
      """set_line www/js/app.js "  var APP_BUILD = " "  var APP_BUILD = '$BUILDDATE';" """.rstrip(),
      'app.js 改用 set_line')

t = sub(t,
      """  sed -i.bak "s/^\\( *versionCode \\).*/\\1$VCODE/" "$GRADLE"
  sed -i.bak "s/^\\( *versionName \\).*/\\1\\"$VER\\"/" "$GRADLE"
  rm -f "$GRADLE.bak\"""",
      """  set_line "$GRADLE" "        versionCode " "        versionCode $VCODE"
  set_line "$GRADLE" "        versionName " "        versionName \\"$VER\\"" """.rstrip(),
      'gradle 改用 set_line')

# ⚠️ 必须**剥离注释**再查：警告注释里就写着 "sed -i"，直接查会命中自己的说明文字
# （这正是 MEMORY.md §10① 记的那个坑，本次又踩了一次）
import re
_code_only = re.sub(r'^[ \t]*#.*$', '', t, flags=re.M)
assert 'sed -i' not in _code_only, '还有残留的 sed -i：' + str([l for l in _code_only.split('\n') if 'sed -i' in l])

io.open(P, 'w', encoding='utf-8', newline='').write(t)
b = open(P, 'rb').read()
print()
print('行数 %d · CRLF %d · LF-only %d' % (len(t.splitlines()), b.count(b'\r\n'), b.count(b'\n') - b.count(b'\r\n')))
print('未匹配:', FAILED if FAILED else '无')
