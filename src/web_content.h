#pragma once

// Single-Page HTML/CSS – Optik angelehnt an WLAN_to_RS485_Modul + DE/EN i18n
static const char WEB_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Inklinometer</title>
<meta http-equiv="Cache-Control" content="no-store">
<style>
:root{
  --bg:#0b1220;--panel:#121a2b;--card:#18233a;--line:#2a3a5a;
  --text:#e8eefc;--muted:#9db0d0;--blue:#3b82f6;--green:#22c55e;
  --red:#ef4444;--amber:#f59e0b;--ok:#16a34a;--warn:#d97706;--bad:#dc2626;
}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;background:var(--bg);color:var(--text)}
.wrap{max-width:980px;margin:0 auto;padding:16px}
.header{display:flex;gap:12px;align-items:center;justify-content:space-between;margin-bottom:14px;flex-wrap:wrap}
.brand{display:flex;gap:10px;align-items:center}
.logo{width:36px;height:36px;border-radius:10px;background:linear-gradient(135deg,#2563eb,#60a5fa);display:grid;place-items:center;font-weight:800}
h1{font-size:18px;margin:0}
.sub{color:var(--muted);font-size:12px}
.header-right{display:flex;gap:8px;align-items:center}
.lang{display:flex;gap:4px;border:1px solid var(--line);border-radius:10px;padding:2px;background:var(--panel)}
.lang button{border:0;background:transparent;color:var(--muted);padding:6px 10px;border-radius:8px;cursor:pointer;font-size:12px;font-weight:600}
.lang button.active{background:rgba(59,130,246,.2);color:var(--text)}
.badge{padding:4px 8px;border-radius:999px;font-size:12px;border:1px solid var(--line);background:var(--card)}
.badge.ok{border-color:rgba(34,197,94,.4);color:#86efac}
.badge.warn{border-color:rgba(245,158,11,.4);color:#fcd34d}
.badge.bad{border-color:rgba(239,68,68,.4);color:#fca5a5}
.tabs{display:flex;gap:8px;flex-wrap:wrap;margin:10px 0 14px}
.tab{border:1px solid var(--line);background:var(--panel);color:var(--text);padding:8px 12px;border-radius:10px;cursor:pointer}
.tab.active{background:rgba(59,130,246,.15);border-color:rgba(59,130,246,.5)}
.grid{display:grid;grid-template-columns:1fr;gap:12px}
@media(min-width:860px){.grid.two{grid-template-columns:1fr 1fr}}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:14px}
.card h2{margin:0 0 10px;font-size:15px}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
label{display:block;font-size:12px;color:var(--muted);margin:0 0 4px}
input,select,button,textarea{font:inherit}
input[type=text],input[type=number],input[type=password],select,textarea{
  width:100%;padding:10px 12px;border-radius:10px;border:1px solid var(--line);
  background:var(--card);color:var(--text);outline:none
}
input:focus,select:focus,textarea:focus{border-color:rgba(59,130,246,.7);box-shadow:0 0 0 3px rgba(59,130,246,.15)}
.btn{border:1px solid var(--line);background:var(--card);color:var(--text);padding:10px 12px;border-radius:10px;cursor:pointer}
.btn.primary{background:var(--blue);border-color:transparent;color:white}
.btn.danger{background:rgba(239,68,68,.15);border-color:rgba(239,68,68,.4);color:#fecaca}
.btn:disabled{opacity:.5;cursor:not-allowed}
.kv{display:grid;grid-template-columns:140px 1fr;gap:6px 10px;font-size:13px}
.kv .k{color:var(--muted)}
.mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
.muted{color:var(--muted);font-size:12px}
.hr{height:1px;background:var(--line);margin:12px 0}
.toast{position:fixed;right:16px;bottom:16px;background:var(--card);border:1px solid var(--line);padding:10px 12px;border-radius:12px;display:none;max-width:360px;z-index:50}
.toast.show{display:block}
.toast.ok{border-color:rgba(34,197,94,.5)}
.toast.bad{border-color:rgba(239,68,68,.5)}
.angle{font-size:84px;font-weight:700;letter-spacing:-.02em;line-height:1.05}
.site-footer{margin-top:28px;padding:16px 0 8px;border-top:1px solid var(--line);text-align:center;color:var(--muted);font-size:12px}
.formgrid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
@media(max-width:640px){.formgrid{grid-template-columns:1fr}}
.field{margin-bottom:2px}
.check{display:flex;align-items:center;gap:8px;margin-top:18px}
.check input{width:auto}
.progress{height:10px;background:#0f172a;border:1px solid var(--line);border-radius:999px;overflow:hidden}
.bar{height:100%;width:0%;background:linear-gradient(90deg,#2563eb,#60a5fa)}
</style>
</head>
<body>
<div class="wrap">
  <div class="header">
    <div class="brand">
      <div class="logo">I</div>
      <div>
        <h1>Inklinometer IIS2ICLX</h1>
        <div class="sub" data-i18n="subtitle">SoftAP · Configuration · OTA</div>
      </div>
    </div>
    <div class="header-right">
      <div class="lang" id="langSwitch">
        <button type="button" data-lang="de" class="active">DE</button>
        <button type="button" data-lang="en">EN</button>
      </div>
      <div id="connBadge" class="badge warn" data-i18n="badge_loading">…</div>
    </div>
  </div>

  <div class="tabs">
    <button class="tab active" data-tab="status" data-i18n="tab_status">Status</button>
    <button class="tab" data-tab="config" data-i18n="tab_config">Configuration</button>
    <button class="tab" data-tab="calib" data-i18n="tab_calib">Calibration</button>
    <button class="tab" data-tab="update" data-i18n="tab_update">Update</button>
    <button class="tab" data-tab="reset" data-i18n="tab_reset">Reset</button>
  </div>

  <div id="tab-status" class="grid two">
    <div class="card">
      <h2 data-i18n="h_elevation">Elevation</h2>
      <div class="angle mono" id="elevVal">—</div>
      <div class="muted" id="elevHint" data-i18n="elev_hint">Live from sensor</div>
      <div class="row" style="margin-top:12px">
        <div id="settledBadge" class="badge warn" data-i18n="badge_settled_unk">…</div>
      </div>
    </div>
    <div class="card">
      <h2 data-i18n="h_system">System</h2>
      <div class="kv">
        <div class="k" data-i18n="k_uptime">Uptime</div><div class="mono" id="uptime">—</div>
        <div class="k" data-i18n="k_error">Error</div><div class="mono" id="errFlag">—</div>
        <div class="k" data-i18n="k_settled">Settled</div><div class="mono" id="settledFlag">—</div>
        <div class="k">SoftAP</div><div class="mono" id="apInfo">—</div>
        <div class="k">UID</div><div class="mono" id="uid">—</div>
        <div class="k" data-i18n="k_fw">Firmware</div><div class="mono" id="fwVer">—</div>
        <div class="k" data-i18n="k_heap">Free Heap</div><div class="mono" id="heap">—</div>
      </div>
    </div>
  </div>

  <div id="tab-config" class="grid" style="display:none">
    <div class="card">
      <h2 data-i18n="h_config">Configuration (NVS)</h2>
      <div class="formgrid">
        <div class="field">
          <label for="mountRotation" data-i18n="lbl_mount">Mount rotation (0° side)</label>
          <select id="mountRotation">
            <option value="0">0°</option>
            <option value="90">90°</option>
            <option value="180">180°</option>
            <option value="270">270°</option>
          </select>
        </div>
        <div class="field check"><input type="checkbox" id="invertRotationDir"><label for="invertRotationDir" data-i18n="lbl_invrot">Invert rotation direction</label></div>
        <div class="field check"><input type="checkbox" id="limit180"><label for="limit180" data-i18n="lbl_limit180">Limit angle to 0–180°</label></div>
        <div class="field check"><input type="checkbox" id="fullScale1g"><label for="fullScale1g" data-i18n="lbl_fs1g">Full-scale ±1 g (else ±2 g)</label></div>
        <div class="field check"><input type="checkbox" id="debugSerial"><label for="debugSerial" data-i18n="lbl_debug_usb">Debug via USB</label></div>
        <div class="field">
          <label for="calib" data-i18n="lbl_calib">Calibration offset (°)</label>
          <input type="number" id="calib" step="0.01" min="-180" max="180">
        </div>
        <div class="field">
          <label for="filter" data-i18n="lbl_filter">Filter alpha (0.01…1)</label>
          <input type="number" id="filter" step="0.01" min="0.01" max="1">
        </div>
        <div class="field">
          <label for="decimals" data-i18n="lbl_decimals">Decimal places</label>
          <select id="decimals"><option value="0">0</option><option value="1">1</option><option value="2">2</option></select>
        </div>
      </div>
      <div class="hr"></div>
      <div class="row">
        <button class="btn primary" id="btnSaveCfg" data-i18n="btn_save">Save</button>
        <button class="btn" id="btnReloadCfg" data-i18n="btn_reload">Reload</button>
      </div>
      <div class="muted" style="margin-top:8px" data-i18n="hint_config_save">Changes are stored in NVS. Full-scale is applied to the sensor immediately.</div>
    </div>
  </div>

  <div id="tab-calib" class="grid" style="display:none">
    <div class="card">
      <h2 data-i18n="h_calib">2-point level calibration</h2>
      <p class="muted" data-i18n-html="p_calib">Place sensor on a level surface → <b>Measurement 1</b>. Then rotate ~180° in place → <b>Measurement 2</b>.<br>
      Both values are typically close (~90°). Then: offset = 90° − mean.</p>
      <div class="kv" style="margin:10px 0">
        <div class="k" data-i18n="k_m1">Measurement 1</div><div class="mono" id="calA">—</div>
        <div class="k" data-i18n="k_m2">Measurement 2</div><div class="mono" id="calB">—</div>
        <div class="k" data-i18n="k_mid">Midpoint</div><div class="mono" id="calMid">—</div>
        <div class="k" data-i18n="k_sep">Separation</div><div class="mono" id="calSep">—</div>
        <div class="k" data-i18n="k_newoff">New offset</div><div class="mono" id="calOff">—</div>
        <div class="k" data-i18n="k_cur">Current</div><div class="mono" id="calCur">—</div>
      </div>
      <div class="row">
        <button class="btn primary" id="btnCal1" data-i18n="btn_cal1">1. Measure (level)</button>
        <button class="btn primary" id="btnCal2" data-i18n="btn_cal2">2. Measure (180°)</button>
      </div>
      <div class="row" style="margin-top:8px">
        <button class="btn" id="btnCalApply" disabled data-i18n="btn_cal_apply">Apply offset</button>
        <button class="btn" id="btnCalClear" data-i18n="btn_cal_clear">Clear steps</button>
      </div>
      <div class="muted" style="margin-top:8px" id="calHint" data-i18n="hint_cal_idle">Hold still; each measurement averages briefly (~0.5 s).</div>
    </div>
  </div>

  <div id="tab-update" class="grid" style="display:none">
    <div class="card">
      <h2 data-i18n="h_update">Firmware update (OTA)</h2>
      <div class="kv">
        <div class="k" data-i18n="k_cur">Current</div><div class="mono" id="updCur">—</div>
        <div class="k" data-i18n="k_part">Partition</div><div class="mono" id="updPart">—</div>
      </div>
      <div class="hr"></div>
      <div class="field">
        <label for="fwFile">firmware.bin</label>
        <input type="file" id="fwFile" accept=".bin">
      </div>
      <div class="progress" style="margin:12px 0"><div class="bar" id="updBar"></div></div>
      <div class="row">
        <button class="btn primary" id="btnUpload" data-i18n="btn_upload">Upload &amp; flash</button>
      </div>
      <div class="muted" style="margin-top:8px" id="updMsg" data-i18n="hint_ota">App firmware only (dual OTA). Reboot after success.</div>
    </div>
  </div>

  <div id="tab-reset" class="grid" style="display:none">
    <div class="card">
      <h2 data-i18n="h_reset">Reboot / factory reset</h2>
      <p class="muted" data-i18n="p_reset">Reboot: SoftAP stays off until button again. Factory: clear NVS and restore compile defaults.</p>
      <div class="row">
        <button class="btn" id="btnReboot" data-i18n="btn_reboot">Reboot</button>
        <button class="btn danger" id="btnFactory" data-i18n="btn_factory">Factory reset</button>
      </div>
    </div>
  </div>

  <footer class="site-footer">&copy; DK8DE J&ouml;rg K&ouml;rner 2026</footer>
</div>
<div class="toast" id="toast"></div>
<script>
const $ = (id)=>document.getElementById(id);
let lang = 'de';
const I18N = {
de:{
  subtitle:'SoftAP · Konfiguration · OTA',
  badge_loading:'laden…', badge_live:'live', badge_ok:'OK', badge_http:'HTTP',
  badge_offline:'offline', badge_reconnect:'reconnect…',
  badge_settled_unk:'…', badge_settled:'ruhig', badge_moving:'bewegt',
  tab_status:'Status', tab_config:'Konfiguration', tab_calib:'Kalibrierung',
  tab_update:'Update', tab_reset:'Reset',
  h_elevation:'Elevation', elev_hint:'Live aus Sensor (FIFO-Mittelung)',
  k_uptime:'Uptime', k_error:'Fehler', k_settled:'Settled',
  h_system:'System', k_fw:'Firmware', k_heap:'Free Heap',
  lbl_debug_usb:'Debug über USB',
  dbg_on:'Ein: Elevation ca. 10×/s auf USB-CDC (115200 Baud).',
  dbg_off:'Aus: keine Winkel auf USB-CDC.',
  hint_rs485:'RS485-Protokoll läuft parallel weiter. Alternativ: <span class="mono">#SETDEBUG,1$</span>',
  h_config:'Konfiguration (NVS)',
  lbl_mount:'Montage-Drehung (0°-Seite)', lbl_invrot:'Drehrichtung umkehren',
  lbl_limit180:'Winkel auf 0–180° begrenzen', lbl_fs1g:'Full-Scale ±1 g (sonst ±2 g)',
  lbl_calib:'Kalibrier-Offset (°)', lbl_filter:'Filter alpha (0.01…1)', lbl_decimals:'Nachkommastellen',
  btn_save:'Speichern', btn_reload:'Neu laden',
  hint_config_save:'Änderungen werden im NVS gespeichert. Full-Scale wird sofort am Sensor gesetzt.',
  h_calib:'2-Punkt-Ebenkalibrierung',
  p_calib:'Sensor auf ebene Fläche legen → <b>Messung 1</b>. Dann auf der Stelle horizontal ca. 180° drehen → <b>Messung 2</b>.<br>Beide Werte liegen typischerweise nahe beieinander (~90°). Daraus: Offset = 90° − Mittelwert.',
  k_m1:'Messung 1', k_m2:'Messung 2', k_mid:'Mittelpunkt', k_sep:'Abstand',
  k_newoff:'Neuer Offset', k_cur:'Aktuell',
  btn_cal1:'1. Messung (eben)', btn_cal2:'2. Messung (180°)',
  btn_cal_apply:'Offset übernehmen', btn_cal_clear:'Schritte löschen',
  hint_cal_idle:'Bitte ruhig halten; jede Messung mittelt kurz (~0,5 s).',
  cal_measuring1:'Messung 1… ruhig halten',
  cal_ok1:'OK. Sensor horizontal ca. 180° drehen, dann Messung 2.',
  cal_fail1:'Messung 1 fehlgeschlagen',
  cal_measuring2:'Messung 2… ruhig halten',
  cal_ok_flat:'OK: Beide Messungen ~gleich (Drehung auf der Stelle). Offset = 90° − Mittelwert → übernehmen.',
  cal_ok_flip:'OK: ~180° Abstand (Flip in der Messebene). Offset übernehmen.',
  cal_ok_odd:'Abstand ungewöhnlich – Sensorlage prüfen oder trotzdem übernehmen.',
  cal_fail2:'Messung 2 fehlgeschlagen (zuerst Messung 1?)',
  cal_sep_flat:' (ok, eben ~gleich)', cal_sep_flip:' (ok ~180°-Flip)', cal_sep_odd:' (unüblich)',
  cal_applied:'Offset gespeichert (NVS).',
  cal_cleared:'Kalibrier-Schritte gelöscht (Offset unverändert).',
  cal_no_calc:'Keine Berechnung',
  h_update:'Firmware-Update (OTA)', k_part:'Partition',
  btn_upload:'Hochladen & flashen', hint_ota:'Nur App-Firmware (Dual-OTA). Nach Erfolg Neustart.',
  upd_choose:'Datei wählen', upd_uploading:'Upload…', upd_ok:'OK – Neustart…', upd_fail:'Update fehlgeschlagen',
  h_reset:'Neustart / Werkseinstellung',
  p_reset:'Neustart: SoftAP bleibt aus bis erneut Taster. Werkseinstellung: NVS löschen und Compile-Defaults.',
  btn_reboot:'Neustart', btn_factory:'Werkseinstellung',
  confirm_factory:'Wirklich Werkseinstellung und Neustart?',
  toast_dbg_on:'USB-Debug ein', toast_dbg_off:'USB-Debug aus', toast_dbg_fail:'Debug-Schalter fehlgeschlagen',
  toast_saved:'Gespeichert', toast_save_fail:'Speichern fehlgeschlagen', toast_cfg_loaded:'Config geladen',
  toast_cfg_fail:'Fehler', toast_cal1:'Messung 1 OK', toast_cal2:'Messung 2 OK',
  toast_cal_apply:'Kalibrierung übernommen', toast_cal_apply_fail:'Übernehmen fehlgeschlagen',
  toast_cal_clear:'Schritte zurückgesetzt', toast_update_ok:'Update OK', toast_update_fail:'Update fehlgeschlagen',
  toast_reboot:'Neustart…', toast_factory:'Factory…', toast_lang_fail:'Sprache speichern fehlgeschlagen',
  auth_required:'Anmeldung erforderlich'
},
en:{
  subtitle:'SoftAP · Configuration · OTA',
  badge_loading:'loading…', badge_live:'live', badge_ok:'OK', badge_http:'HTTP',
  badge_offline:'offline', badge_reconnect:'reconnect…',
  badge_settled_unk:'…', badge_settled:'settled', badge_moving:'moving',
  tab_status:'Status', tab_config:'Configuration', tab_calib:'Calibration',
  tab_update:'Update', tab_reset:'Reset',
  h_elevation:'Elevation', elev_hint:'Live from sensor (FIFO average)',
  k_uptime:'Uptime', k_error:'Error', k_settled:'Settled',
  h_system:'System', k_fw:'Firmware', k_heap:'Free Heap',
  lbl_debug_usb:'Debug via USB',
  dbg_on:'On: elevation ~10×/s on USB-CDC (115200 baud).',
  dbg_off:'Off: no angle output on USB-CDC.',
  hint_rs485:'RS485 protocol continues in parallel. Alternative: <span class="mono">#SETDEBUG,1$</span>',
  h_config:'Configuration (NVS)',
  lbl_mount:'Mount rotation (0° side)', lbl_invrot:'Invert rotation direction',
  lbl_limit180:'Limit angle to 0–180°', lbl_fs1g:'Full-scale ±1 g (else ±2 g)',
  lbl_calib:'Calibration offset (°)', lbl_filter:'Filter alpha (0.01…1)', lbl_decimals:'Decimal places',
  btn_save:'Save', btn_reload:'Reload',
  hint_config_save:'Changes are stored in NVS. Full-scale is applied to the sensor immediately.',
  h_calib:'2-point level calibration',
  p_calib:'Place sensor on a level surface → <b>Measurement 1</b>. Then rotate ~180° in place → <b>Measurement 2</b>.<br>Both values are typically close (~90°). Then: offset = 90° − mean.',
  k_m1:'Measurement 1', k_m2:'Measurement 2', k_mid:'Midpoint', k_sep:'Separation',
  k_newoff:'New offset', k_cur:'Current',
  btn_cal1:'1. Measure (level)', btn_cal2:'2. Measure (180°)',
  btn_cal_apply:'Apply offset', btn_cal_clear:'Clear steps',
  hint_cal_idle:'Hold still; each measurement averages briefly (~0.5 s).',
  cal_measuring1:'Measurement 1… hold still',
  cal_ok1:'OK. Rotate sensor ~180° horizontally, then measurement 2.',
  cal_fail1:'Measurement 1 failed',
  cal_measuring2:'Measurement 2… hold still',
  cal_ok_flat:'OK: both readings ~equal (rotate in place). Offset = 90° − mean → apply.',
  cal_ok_flip:'OK: ~180° separation (flip in sense plane). Apply offset.',
  cal_ok_odd:'Unusual separation – check placement or apply anyway.',
  cal_fail2:'Measurement 2 failed (do measurement 1 first?)',
  cal_sep_flat:' (ok, level ~equal)', cal_sep_flip:' (ok ~180° flip)', cal_sep_odd:' (unusual)',
  cal_applied:'Offset saved (NVS).',
  cal_cleared:'Calibration steps cleared (offset unchanged).',
  cal_no_calc:'Nothing to apply',
  h_update:'Firmware update (OTA)', k_part:'Partition',
  btn_upload:'Upload & flash', hint_ota:'App firmware only (dual OTA). Reboot after success.',
  upd_choose:'Choose a file', upd_uploading:'Upload…', upd_ok:'OK – rebooting…', upd_fail:'Update failed',
  h_reset:'Reboot / factory reset',
  p_reset:'Reboot: SoftAP stays off until button again. Factory: clear NVS and restore compile defaults.',
  btn_reboot:'Reboot', btn_factory:'Factory reset',
  confirm_factory:'Really factory-reset and reboot?',
  toast_dbg_on:'USB debug on', toast_dbg_off:'USB debug off', toast_dbg_fail:'Debug toggle failed',
  toast_saved:'Saved', toast_save_fail:'Save failed', toast_cfg_loaded:'Config loaded',
  toast_cfg_fail:'Error', toast_cal1:'Measurement 1 OK', toast_cal2:'Measurement 2 OK',
  toast_cal_apply:'Calibration applied', toast_cal_apply_fail:'Apply failed',
  toast_cal_clear:'Steps cleared', toast_update_ok:'Update OK', toast_update_fail:'Update failed',
  toast_reboot:'Rebooting…', toast_factory:'Factory…', toast_lang_fail:'Failed to save language',
  auth_required:'Login required'
}
};
function t(key){ const d=I18N[lang]||I18N.de; return (d[key]!=null)?d[key]:((I18N.de[key]!=null)?I18N.de[key]:key); }
function applyLang(next){
  lang = (next==='en')?'en':'de';
  document.documentElement.lang = lang;
  document.querySelectorAll('#langSwitch button').forEach(b=>b.classList.toggle('active', b.dataset.lang===lang));
  document.querySelectorAll('[data-i18n]').forEach(el=>{
    const k=el.getAttribute('data-i18n');
    if(k) el.textContent = t(k);
  });
  document.querySelectorAll('[data-i18n-html]').forEach(el=>{
    const k=el.getAttribute('data-i18n-html');
    if(k) el.innerHTML = t(k);
  });
  setDebugUi($('debugSerial')?$('debugSerial').checked:false);
  if($('settledFlag')){
    const v=$('settledFlag').textContent;
    if(v==='1') applySettled(true);
    else if(v==='0') applySettled(false);
  }
  if(ws && ws.readyState===1) $('connBadge').textContent=t('badge_live');
}
async function setLang(next){
  const prev=lang;
  applyLang(next);
  try{
    await api('/api/lang',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({lang})});
  }catch(e){
    applyLang(prev);
    toast(t('toast_lang_fail'), false);
  }
}
document.querySelectorAll('#langSwitch button').forEach(b=>{
  b.onclick=()=>setLang(b.dataset.lang);
});

function toast(msg, ok=true){
  const el=$('toast'); el.textContent=msg; el.className='toast show '+(ok?'ok':'bad');
  setTimeout(()=>el.classList.remove('show'), 2500);
}
async function api(path, opts){
  const sep = path.indexOf('?')>=0 ? '&' : '?';
  const url = path + sep + '_=' + Date.now();
  const base = {credentials:'same-origin', cache:'no-store', headers:{'Accept':'application/json'}};
  const o = opts || {};
  const headers = Object.assign({}, base.headers, o.headers||{});
  const ctrl = new AbortController();
  const timer = setTimeout(()=>ctrl.abort(), 2500);
  try{
    const r = await fetch(url, Object.assign({}, base, o, {headers, signal:ctrl.signal}));
    if(r.status===401) throw new Error(t('auth_required'));
    if(!r.ok) throw new Error('HTTP '+r.status);
    const ct = r.headers.get('content-type')||'';
    if(ct.includes('application/json')) return r.json();
    return r.text();
  }finally{
    clearTimeout(timer);
  }
}
function showTab(name){
  document.querySelectorAll('.tab').forEach(b=>b.classList.toggle('active', b.dataset.tab===name));
  ['status','config','calib','update','reset'].forEach(n=>{
    const el=$('tab-'+n); if(el) el.style.display = (n===name)?'':'none';
  });
}
document.querySelectorAll('.tab').forEach(b=>b.addEventListener('click',()=>showTab(b.dataset.tab)));

function fmtUptime(s){
  s=Math.floor(s||0); const h=Math.floor(s/3600), m=Math.floor((s%3600)/60), sec=s%60;
  return h+'h '+m+'m '+sec+'s';
}
function setElevation(a){
  if(a==null || a.elevation==null || isNaN(Number(a.elevation))) return;
  const d = (a.decimals!=null)? Number(a.decimals) : 2;
  const el = $('elevVal');
  if(el) el.textContent = Number(a.elevation).toFixed(d)+'\u00B0';
  if(a.seq!=null){
    const h = $('elevHint');
    if(h) h.textContent = 'seq '+a.seq;
  }
}
function setDebugUi(on){
  const b = $('debugSerial');
  if(b) b.checked = !!on;
}
async function setDebug(on){
  try{
    await api('/api/debug',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({debug:!!on})});
    setDebugUi(on);
    toast(on ? t('toast_dbg_on') : t('toast_dbg_off'));
  }catch(e){
    toast(t('toast_dbg_fail'), false);
    setDebugUi(!on);
  }
}
if($('debugSerial')){
  $('debugSerial').onchange = ()=>setDebug($('debugSerial').checked);
}
let statusBusy = false;
let liveBusy = false;
let ws = null;
let wsRetry = null;
let lastSeq = -1;
function applySettled(v){
  const settled = !!v;
  if($('settledFlag')) $('settledFlag').textContent = settled ? '1' : '0';
  const b = $('settledBadge');
  if(b){
    b.textContent = settled ? t('badge_settled') : t('badge_moving');
    b.className = 'badge ' + (settled ? 'ok' : 'warn');
  }
}
function applyLive(a){
  setElevation(a);
  if(a.seq!=null) lastSeq = Number(a.seq);
  if(a.error!=null){
    $('errFlag').textContent = a.error ? '1' : '0';
  }
  if(a.settled!=null) applySettled(a.settled);
  if(a.heap!=null) $('heap').textContent = a.heap+' B';
  $('connBadge').textContent = (ws && ws.readyState===1) ? t('badge_live') : t('badge_ok');
  $('connBadge').className = 'badge ok';
}
function connectWs(){
  if(wsRetry){ clearTimeout(wsRetry); wsRetry=null; }
  try{ if(ws){ ws.onclose=null; ws.close(); } }catch(e){}
  const proto = location.protocol==='https:' ? 'wss://' : 'ws://';
  ws = new WebSocket(proto + location.hostname + ':81/');
  ws.onopen = ()=>{
    $('connBadge').textContent=t('badge_live');
    $('connBadge').className='badge ok';
  };
  ws.onmessage = (ev)=>{
    try{ applyLive(JSON.parse(ev.data)); }catch(e){}
  };
  ws.onerror = ()=>{};
  ws.onclose = ()=>{
    $('connBadge').textContent=t('badge_http');
    $('connBadge').className='badge warn';
    wsRetry = setTimeout(connectWs, 2000);
  };
}
async function refreshLive(){
  if(liveBusy) return;
  liveBusy = true;
  try{
    const a = await api('/api/live');
    applyLive(a);
  }catch(e){
    if(!ws || ws.readyState!==1){
      $('connBadge').textContent=t('badge_offline');
      $('connBadge').className='badge bad';
    }
  }finally{
    liveBusy = false;
  }
}
async function refreshStatus(){
  if(statusBusy) return;
  statusBusy = true;
  try{
    const s = await api('/api/status');
    $('uptime').textContent = fmtUptime(s.uptime);
    $('apInfo').textContent = (s.ssid||'')+' / '+ (s.ip||'192.168.4.1');
    $('uid').textContent = s.uid||'—';
    $('fwVer').textContent = s.fw||'—';
    if(s.heap!=null) $('heap').textContent = s.heap+' B';
    if(s.debug!=null) setDebugUi(!!s.debug);
    if(s.settled!=null) applySettled(s.settled);
  }catch(e){
  }finally{
    statusBusy = false;
  }
}
function fillCfg(c){
  if($('mountRotation')) $('mountRotation').value = String(c.mountRotation!=null?c.mountRotation:0);
  $('invertRotationDir').checked=!!c.invertRotationDir;
  $('limit180').checked=!!c.limit180;
  $('fullScale1g').checked=!!c.fullScale1g;
  $('debugSerial').checked=!!c.debug;
  setDebugUi(!!c.debug);
  $('calib').value = Number(c.calib).toFixed(2);
  $('filter').value = Number(c.filter).toFixed(2);
  $('decimals').value = String(c.decimals);
  if($('calCur')) $('calCur').textContent = Number(c.calib).toFixed(2)+'\u00B0';
  if(c.lang) applyLang(c.lang);
}
let pendingCalOffset = null;
function fmtDeg(v){ return (Number(v).toFixed(2))+'\u00B0'; }
function bindCalibUi(){
  const b1=$('btnCal1'), b2=$('btnCal2'), ba=$('btnCalApply'), bc=$('btnCalClear');
  if(!b1||!b2||!ba||!bc) return;
  b1.onclick = async ()=>{
    $('calHint').textContent = t('cal_measuring1');
    ba.disabled = true;
    pendingCalOffset = null;
    try{
      const r = await api('/api/calib/1',{method:'POST'});
      $('calA').textContent = fmtDeg(r.a);
      $('calB').textContent = '—';
      $('calMid').textContent = '—';
      $('calSep').textContent = '—';
      $('calOff').textContent = '—';
      $('calHint').textContent = t('cal_ok1');
      toast(t('toast_cal1'));
    }catch(e){
      $('calHint').textContent = t('cal_fail1');
      toast(t('cal_fail1'), false);
    }
  };
  b2.onclick = async ()=>{
    $('calHint').textContent = t('cal_measuring2');
    try{
      const r = await api('/api/calib/2',{method:'POST'});
      $('calB').textContent = fmtDeg(r.b);
      $('calMid').textContent = fmtDeg(r.mid);
      const sepExtra = r.mode==='flat'?t('cal_sep_flat'):(r.mode==='flip'?t('cal_sep_flip'):t('cal_sep_odd'));
      $('calSep').textContent = fmtDeg(r.sep)+sepExtra;
      $('calOff').textContent = fmtDeg(r.offset);
      pendingCalOffset = Number(r.offset);
      ba.disabled = false;
      if(r.mode==='flat') $('calHint').textContent = t('cal_ok_flat');
      else if(r.mode==='flip') $('calHint').textContent = t('cal_ok_flip');
      else $('calHint').textContent = t('cal_ok_odd');
      toast(t('toast_cal2'));
    }catch(e){
      $('calHint').textContent = t('cal_fail2');
      toast(t('cal_fail2'), false);
    }
  };
  ba.onclick = async ()=>{
    if(pendingCalOffset==null || isNaN(pendingCalOffset)){ toast(t('cal_no_calc'), false); return; }
    try{
      const r = await api('/api/calib/apply',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({offset:pendingCalOffset})});
      $('calCur').textContent = fmtDeg(r.calib);
      if($('calib')) $('calib').value = Number(r.calib).toFixed(2);
      ba.disabled = true;
      $('calHint').textContent = t('cal_applied');
      toast(t('toast_cal_apply'));
    }catch(e){ toast(t('toast_cal_apply_fail'), false); }
  };
  bc.onclick = async ()=>{
    try{ await api('/api/calib/clear',{method:'POST'}); }catch(e){}
    pendingCalOffset = null;
    ba.disabled = true;
    $('calA').textContent='—'; $('calB').textContent='—';
    $('calMid').textContent='—'; $('calSep').textContent='—'; $('calOff').textContent='—';
    $('calHint').textContent = t('cal_cleared');
    toast(t('toast_cal_clear'));
  };
}
bindCalibUi();
async function loadCfg(){
  const c = await api('/api/config');
  fillCfg(c);
}
$('btnReloadCfg').onclick = ()=>loadCfg().then(()=>toast(t('toast_cfg_loaded'))).catch(()=>toast(t('toast_cfg_fail'),false));
$('btnSaveCfg').onclick = async ()=>{
  const body = {
    mountRotation: parseInt($('mountRotation').value,10),
    invertRotationDir: $('invertRotationDir').checked,
    limit180: $('limit180').checked,
    fullScale1g: $('fullScale1g').checked,
    debug: $('debugSerial').checked,
    calib: parseFloat($('calib').value),
    filter: parseFloat($('filter').value),
    decimals: parseInt($('decimals').value,10),
    lang: lang
  };
  try{
    await api('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    toast(t('toast_saved'));
    await loadCfg();
  }catch(e){ toast(t('toast_save_fail'), false); }
};
async function loadUpdateInfo(){
  try{
    const u = await api('/api/update/info');
    $('updCur').textContent = u.fw||'—';
    $('updPart').textContent = (u.partition||'—')+' ('+(u.size||0)+' B)';
  }catch(e){}
}
$('btnUpload').onclick = async ()=>{
  const f = $('fwFile').files[0];
  if(!f){ toast(t('upd_choose'), false); return; }
  $('btnUpload').disabled=true; $('updBar').style.width='0%'; $('updMsg').textContent=t('upd_uploading');
  try{
    const fd = new FormData(); fd.append('update', f, f.name);
    const xhr = new XMLHttpRequest();
    await new Promise((resolve,reject)=>{
      xhr.upload.onprogress = (ev)=>{
        if(ev.lengthComputable){
          const p = Math.round(ev.loaded*100/ev.total);
          $('updBar').style.width=p+'%';
        }
      };
      xhr.onload = ()=>{
        if(xhr.status>=200 && xhr.status<300) resolve(); else reject(new Error('HTTP '+xhr.status));
      };
      xhr.onerror = ()=>reject(new Error('network'));
      xhr.open('POST','/api/update');
      xhr.send(fd);
    });
    $('updMsg').textContent=t('upd_ok');
    toast(t('toast_update_ok'));
    setTimeout(()=>api('/api/reboot',{method:'POST'}).catch(()=>{}), 800);
  }catch(e){
    $('updMsg').textContent=t('upd_fail');
    toast(t('toast_update_fail'), false);
  }finally{
    $('btnUpload').disabled=false;
  }
};
$('btnReboot').onclick = ()=>{
  api('/api/reboot',{method:'POST'}).then(()=>toast(t('toast_reboot'))).catch(()=>toast(t('toast_reboot')));
};
$('btnFactory').onclick = ()=>{
  if(!confirm(t('confirm_factory'))) return;
  api('/api/factory',{method:'POST'}).then(()=>toast(t('toast_factory'))).catch(()=>toast(t('toast_factory')));
};

applyLang('de');
loadCfg().catch(()=>{});
loadUpdateInfo().catch(()=>{});
refreshStatus();
connectWs();
refreshLive();
setInterval(refreshLive, 100);
setInterval(refreshStatus, 10000);
</script>
</body>
</html>
)HTML";
