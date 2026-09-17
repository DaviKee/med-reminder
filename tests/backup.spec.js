/* 自动本地备份（F-7）
 *
 * 需求：「每次保存的时候，本地也存一份」。
 *
 * 本模块最关键的决策是**写到哪个目录**：
 *   App 的 Manifest 里没有任何存储权限，Android 10+ 下写公共目录会失败，
 *   所以主路径用 Directory.External（app 专属外部目录，无需权限、
 *   清应用数据不会删），失败退到 Directory.Data。
 *   桩要如实模拟「某些目录写失败」这种情况，才测得出降级链。
 */
const fsmod = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const SRC = fsmod.readFileSync(path.join(ROOT, 'www/js/backup.js'), 'utf8');
const APP = fsmod.readFileSync(path.join(ROOT, 'www/js/app.js'), 'utf8')
  .replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const HTML = fsmod.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}
const sleep = ms => new Promise(r => setTimeout(r, ms));

/* ---------------- Filesystem 插件桩 ---------------- */
function makeFs(opts) {
  opts = opts || {};
  const files = new Map();          // 'DIR|path' -> text
  const log = [];
  const dead = {};                  // 标记为「写不进去」的目录
  (opts.dead || []).forEach(d => { dead[d] = true; });

  return {
    files, log, dead,
    mkdir(o) {
      log.push(['mkdir', o.directory, o.path]);
      if (dead[o.directory]) return Promise.reject(new Error('EACCES'));
      return Promise.resolve();
    },
    writeFile(o) {
      log.push(['write', o.directory, o.path]);
      if (dead[o.directory]) return Promise.reject(new Error('EACCES'));
      files.set(o.directory + '|' + o.path, o.data);
      return Promise.resolve({ uri: 'file:///x/' + o.path });
    },
    readdir(o) {
      log.push(['readdir', o.directory, o.path]);
      if (dead[o.directory]) return Promise.reject(new Error('EACCES'));
      const prefix = o.directory + '|' + o.path + '/';
      return Promise.resolve({
        files: Array.from(files.keys()).filter(k => k.indexOf(prefix) === 0).map(k => k.slice(prefix.length))
      });
    },
    deleteFile(o) {
      log.push(['delete', o.directory, o.path]);
      files.delete(o.directory + '|' + o.path);
      return Promise.resolve();
    }
  };
}

/* ---------------- 启动一个 backup.js 沙箱 ---------------- */
function boot(fs, store) {
  const ls = store || new Map();
  const sandbox = {
    console: { warn() {}, log() {}, error() {} },
    Promise, Date, Math, JSON, Object, Array, String, Number, Boolean,
    setTimeout, clearTimeout, setInterval, clearInterval,
    localStorage: {
      getItem: k => (ls.has(k) ? ls.get(k) : null),
      setItem: (k, v) => { ls.set(k, String(v)); },
      removeItem: k => { ls.delete(k); }
    }
  };
  sandbox.window = sandbox;
  sandbox.window.Capacitor = fs ? { Plugins: { Filesystem: fs } } : { Plugins: {} };
  vm.createContext(sandbox);
  vm.runInContext(SRC, sandbox);
  return { A: sandbox.window.MedAutoBackup, ls };
}

const has = (fs, dir, name) => fs.files.has(dir + '|MedReminder/' + name);
const namesIn = (fs, dir) => Array.from(fs.files.keys())
  .filter(k => k.indexOf(dir + '|MedReminder/') === 0)
  .map(k => k.split('/').pop());

(async function main() {
  console.log('=== A. 没有文件系统插件时（浏览器模式）===');
  {
    const { A } = boot(null);
    t('supported = false', A.status().supported === false, JSON.stringify(A.status()));
    let threw = '';
    try { A.schedule(() => '{}'); await A.now(); await A.flush(); A.status(); } catch (e) { threw = String(e && e.message); }
    t('★ 所有 API 安全降级，不抛错', threw === '', threw);
    t('status 形状完整', ['supported', 'ok', 'at', 'dir', 'count', 'err'].every(k => k in A.status()), JSON.stringify(A.status()));
  }

  console.log('');
  console.log('=== B. 正常写：latest + 当日各一份 ===');
  {
    const fsimpl = makeFs();
    const { A, ls } = boot(fsimpl);
    A.schedule(() => '{"hello":"world"}');
    const r = await A.flush();

    t('★ 写到了 EXTERNAL（app 专属外部目录，无需权限）', r && r.ok && r.dir === 'EXTERNAL', JSON.stringify(r));
    t('★ latest.json 已落盘', has(fsimpl, 'EXTERNAL', 'latest.json'), namesIn(fsimpl, 'EXTERNAL').join(','));
    t('★ 当日备份已落盘（backup-YYYYMMDD.json）',
      namesIn(fsimpl, 'EXTERNAL').some(n => /^backup-\d{8}\.json$/.test(n)), namesIn(fsimpl, 'EXTERNAL').join(','));
    t('内容正确', fsimpl.files.get('EXTERNAL|MedReminder/latest.json') === '{"hello":"world"}',
      fsimpl.files.get('EXTERNAL|MedReminder/latest.json'));
    t('先建了目录再写（否则 writeFile 会报父目录不存在）',
      fsimpl.log.findIndex(x => x[0] === 'mkdir') < fsimpl.log.findIndex(x => x[0] === 'write'),
      JSON.stringify(fsimpl.log.slice(0, 2)));
    t('status 记录成功与目录', A.status().ok === true && A.status().dir === 'EXTERNAL', JSON.stringify(A.status()));
    t('★ 状态落盘（重启后还能显示上次备份时间）', !!ls.get('medreminder.autoBackup.v1'), '没落盘');
  }

  console.log('');
  console.log('=== C. 防抖：连续改动只写一次 ===');
  {
    const fsimpl = makeFs();
    const { A } = boot(fsimpl);
    for (let i = 0; i < 5; i++) A.schedule(() => '{"n":' + i + '}');
    t('还没到点，一次都没写', fsimpl.log.filter(x => x[0] === 'write').length === 0,
      JSON.stringify(fsimpl.log.map(x => x[0])));
    await sleep(A.DEBOUNCE_MS + 400);
    const writes = fsimpl.log.filter(x => x[0] === 'write').length;
    t('★ 防抖后只写了一次（2 个文件：latest + 当日）', writes === 2, writes);
    t('落盘的是最后一次的内容', fsimpl.files.get('EXTERNAL|MedReminder/latest.json') === '{"n":4}',
      fsimpl.files.get('EXTERNAL|MedReminder/latest.json'));
  }

  console.log('');
  console.log('=== D. 目录降级链（真实场景：某些目录写不进去）===');
  {
    // EXTERNAL 写不进去 → 退到 DATA
    const fsimpl = makeFs({ dead: ['EXTERNAL'] });
    const { A } = boot(fsimpl);
    A.schedule(() => '{"x":1}');
    const r = await A.flush();
    t('★ EXTERNAL 失败时自动退到 DATA', r && r.ok && r.dir === 'DATA', JSON.stringify(r));
    t('DATA 里确实写进去了', has(fsimpl, 'DATA', 'latest.json'), namesIn(fsimpl, 'DATA').join(','));
    t('EXTERNAL 里没有半截文件', namesIn(fsimpl, 'EXTERNAL').length === 0, namesIn(fsimpl, 'EXTERNAL').join(','));

    // 两个都写不进去 → 记录失败，但**不抛**
    const dead2 = makeFs({ dead: ['EXTERNAL', 'DATA'] });
    const { A: A2 } = boot(dead2);
    let threw = '';
    let r2;
    try { A2.schedule(() => '{"x":1}'); r2 = await A2.flush(); } catch (e) { threw = String(e && e.message); }
    t('★ 全都写不进去也不抛错', threw === '', threw);
    t('★ 状态如实记为失败并带原因', r2 && r2.ok === false && r2.err, JSON.stringify(r2));
    t('status 能读到失败原因', A2.status().ok === false && !!A2.status().err, JSON.stringify(A2.status()));
  }

  console.log('');
  console.log('=== E. 保留策略：只留最近 14 份日备份 ===');
  {
    const fsimpl = makeFs();
    // 预置 20 份历史日备份
    for (let i = 1; i <= 20; i++) {
      const d = String(i).padStart(2, '0');
      fsimpl.files.set('EXTERNAL|MedReminder/backup-202609' + d + '.json', '{}');
    }
    const { A } = boot(fsimpl);
    A.schedule(() => '{"new":1}');
    const r = await A.flush();
    const left = namesIn(fsimpl, 'EXTERNAL').filter(n => /^backup-\d{8}\.json$/.test(n)).sort();
    t('★ 清理后不超过 14 份', left.length <= 14, left.length);
    t('★ 删的是最旧的（保留 20260907 之后）', left[0] >= 'backup-20260907.json', left[0]);
    t('latest.json 不受清理影响', has(fsimpl, 'EXTERNAL', 'latest.json'), '');
    t('count 反映了剩余份数', r && typeof r.count === 'number', JSON.stringify(r));
  }

  console.log('');
  console.log('=== F. 健壮性：取数据失败 / 并发 ===');
  {
    // 取数据的回调抛错 → 不能把调用方带崩
    const fsimpl = makeFs();
    const { A } = boot(fsimpl);
    let threw = '';
    let r;
    try {
      A.schedule(() => { throw new Error('序列化炸了'); });
      r = await A.flush();
    } catch (e) { threw = String(e && e.message); }
    t('★ 取数据回调抛错时不崩', threw === '', threw);
    t('记为失败而不是写半截', r && r.ok === false, JSON.stringify(r));

    // 并发 now()：不该交错写同一个文件
    const fs2 = makeFs();
    const { A: A2 } = boot(fs2);
    A2.schedule(() => '{"a":1}');
    const rs = await Promise.all([A2.now(), A2.now(), A2.now()]);
    t('★ 并发调用返回同一形状（都带 boolean 的 ok）',
      rs.every(x => x && typeof x.ok === 'boolean'), JSON.stringify(rs.map(x => x && x.ok)));
    t('★ 没有交错（文件仍是一份，内容完整）',
      fs2.files.get('EXTERNAL|MedReminder/latest.json') === '{"a":1}',
      fs2.files.get('EXTERNAL|MedReminder/latest.json'));
  }

  console.log('');
  console.log('=== G. 跨会话：状态保留 ===');
  {
    const store = new Map();
    const fs1 = makeFs();
    const { A } = boot(fs1, store);
    A.schedule(() => '{"first":1}');
    await A.flush();
    const at1 = A.status().at;
    t('第一次记下了时间', at1 > 0, at1);

    // 「重启 App」：新的沙箱，同一个 localStorage
    const fs2 = makeFs();
    const { A: A2 } = boot(fs2, store);
    t('★ 重启后仍能显示上次备份时间', A2.status().at === at1, A2.status().at + ' vs ' + at1);
    t('重启后 supported 仍为 true', A2.status().supported === true, JSON.stringify(A2.status()));
  }

  console.log('');
  console.log('=== H. 与 app 的接线（源码级）===');
  {
    t('★ index.html 引入了 backup.js', HTML.indexOf('js/backup.js') >= 0, '没引入');
    t('★ backup.js 在 app.js 之前加载',
      HTML.indexOf('js/backup.js') < HTML.indexOf('js/app.js'), '顺序反了');
    t('★ save() 里挂上了自动备份（所以「每次保存」都覆盖到）',
      /syncNotifications\(\);[\s\S]{0,320}MedAutoBackup\.schedule/.test(APP), '没挂');
    t('★ 传给备份的是与手动导出**同一个**格式（buildBackup）',
      /MedAutoBackup\.schedule\(function \(\) \{ return JSON\.stringify\(buildBackup\(\)\); \}\)/.test(APP), '格式不一致');
    t('记录页有自动备份卡', APP.indexOf('function autoBackupCardHtml(') >= 0 && APP.indexOf('AUTO · 自动备份') >= 0, '缺');
    t('有「立即备份一次」按钮并接了线',
      APP.indexOf("$('#btnAutoBak')") >= 0 && /ab\.onclick = function/.test(APP), '没接');
    t('卡片说明了位置', APP.indexOf('Android/data/com.medreminder.app/files/') >= 0, '缺位置说明');
    t('★ 卡片明确警告「卸载会连目录一起删」（不隐瞒局限）',
      APP.indexOf('卸载 App 会连这个目录一起删掉') >= 0, '缺警告');
    t('★ 切后台时把待写的落盘（防抖窗口内被杀不至于白改）',
      /appStateChange[\s\S]{0,300}MedAutoBackup\.flush\(\)/.test(APP), '没 flush');
    t('备份文件不写进 S（不污染业务数据）',
      APP.indexOf('autoBackup.v1') < 0, '不该出现在 app.js');
  }

  console.log('');
  console.log('==========================================');
  console.log('通过 %d / 共 %d', pass, pass + fail);
  if (fail) { console.log('失败清单：'); fails.forEach(f => console.log('  - ' + f)); }
  process.exit(fail ? 1 : 0);
})().catch(e => { console.error('用例本身抛错:', e); process.exit(2); });
