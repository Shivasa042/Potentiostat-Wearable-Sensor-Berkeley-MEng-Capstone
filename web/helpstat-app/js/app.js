import { HelpStatBle, floatFromBytes } from "./ble.js";
import {
  appendSweep,
  exportDayJson,
  getDay,
  listDayKeys,
  mergeImportedDay,
  pstDateKey,
  pstTimeLabel,
} from "./storage.js";
import { renderSweep } from "./charts.js";

const $ = (id) => document.getElementById(id);
const chartRoot = $("chart-root");

const ble = new HelpStatBle();

const live = {
  listReal: [],
  listImag: [],
  listFreq: [],
  listPhase: [],
  listMagnitude: [],
  rct: null,
  rs: null,
};

function resetLive() {
  live.listReal = [];
  live.listImag = [];
  live.listFreq = [];
  live.listPhase = [];
  live.listMagnitude = [];
  live.rct = null;
  live.rs = null;
}

let sweepTimer = null;
let sweepStartMs = 0;
let expectedPoints = 0;

function estimatePoints() {
  const f0 = parseFloat($("in-f0").value) || 200000;
  const f1 = parseFloat($("in-f1").value) || 1;
  const ppd = parseFloat($("in-ppd").value) || 5;
  const high = Math.max(f0, f1);
  const low = Math.min(f0, f1);
  if (low <= 0 || high <= 0) return 27;
  return Math.max(1, Math.floor(1.5 + (Math.log10(high) - Math.log10(low)) * ppd) - 1);
}

function formatFreq(f) {
  if (f >= 1e6) return `${(f / 1e6).toFixed(1)} MHz`;
  if (f >= 1000) return `${(f / 1000).toFixed(1)} kHz`;
  if (f >= 1) return `${f.toFixed(1)} Hz`;
  return `${(f * 1000).toFixed(1)} mHz`;
}

function formatElapsed(ms) {
  const s = Math.floor(ms / 1000);
  const m = Math.floor(s / 60);
  return `${m}:${String(s % 60).padStart(2, "0")}`;
}

function updateProgress() {
  const received = live.listFreq.length;
  const elapsed = Date.now() - sweepStartMs;
  const pct = expectedPoints > 0 ? Math.min(100, Math.round((received / expectedPoints) * 100)) : 0;

  $("progress-fill").style.width = `${pct}%`;

  let status;
  if (received === 0) {
    status = `Waiting for first point\u2026 ${formatElapsed(elapsed)} elapsed`;
  } else {
    const lastFreq = live.listFreq[received - 1];
    status = `Point ${received}/${expectedPoints} \u00b7 ${formatFreq(lastFreq)} \u00b7 ${formatElapsed(elapsed)}`;
  }
  $("progress-text").textContent = status;
}

function startProgress() {
  stopProgress();
  expectedPoints = estimatePoints();
  sweepStartMs = Date.now();
  $("sweep-progress").hidden = false;
  $("progress-fill").style.width = "0%";
  updateProgress();
  sweepTimer = setInterval(updateProgress, 1000);
}

function stopProgress(final) {
  if (sweepTimer) {
    clearInterval(sweepTimer);
    sweepTimer = null;
  }
  if (final) {
    const elapsed = Date.now() - sweepStartMs;
    const n = live.listFreq.length;
    $("progress-fill").style.width = "100%";
    $("progress-text").textContent = `Complete \u2014 ${n} points in ${formatElapsed(elapsed)}`;
  }
}

function onBleNotify(key, value) {
  if (key === "real") live.listReal.push(floatFromBytes(value));
  else if (key === "imag") live.listImag.push(floatFromBytes(value));
  else if (key === "currFreq") {
    live.listFreq.push(floatFromBytes(value));
    updateProgress();
  }
  else if (key === "phase") {
    let ph = floatFromBytes(value);
    if (ph > 180) ph -= 360;
    else if (ph < -180) ph += 360;
    live.listPhase.push(ph);
  } else if (key === "magnitude") live.listMagnitude.push(floatFromBytes(value));
  else if (key === "rct") live.rct = new TextDecoder().decode(value).trim();
  else if (key === "rs") {
    live.rs = new TextDecoder().decode(value).trim();
    void finalizeSweep();
  }
}

async function finalizeSweep() {
  const n = live.listFreq.length;
  if (n === 0 || live.listReal.length !== n || live.listImag.length !== n) return;
  if (live.listPhase.length !== n || live.listMagnitude.length !== n) return;

  const now = Date.now();
  const points = [];
  for (let i = 0; i < n; i++) {
    points.push({
      f: live.listFreq[i],
      re: live.listReal[i],
      im: live.listImag[i],
      ph: live.listPhase[i],
      mag: live.listMagnitude[i],
    });
  }
  const sweep = {
    savedAtUtcMs: now,
    pstDateKey: pstDateKey(new Date(now)),
    pstTimeLabel: pstTimeLabel(new Date(now)),
    rct: live.rct || "",
    rs: live.rs || "",
    points,
  };
  await appendSweep(sweep);
  stopProgress(true);
  $("live-status").textContent = `Saved sweep (${sweep.pstTimeLabel})`;
  await refreshHistorySelectors();
  $("sel-day").value = sweep.pstDateKey;
  await onDayChange();
  $("sel-sweep").selectedIndex = $("sel-sweep").options.length - 1;
  onSweepChange();
  showTab("history");
}

function setBleUi(connected, msg) {
  $("btn-connect").disabled = connected;
  $("btn-disconnect").disabled = !connected;
  $("btn-sweep").disabled = !connected;
  $("ble-line").textContent = msg;
}

function iosOrSafariNoBle() {
  const ua = navigator.userAgent || "";
  const iOS = /iPad|iPhone|iPod/.test(ua) || (navigator.platform === "MacIntel" && navigator.maxTouchPoints > 1);
  const noBle = !ble.isWebBluetoothAvailable;
  return { iOS, noBle };
}

function showPlatformBanner() {
  const { iOS, noBle } = iosOrSafariNoBle();
  const el = $("platform-banner");
  if (noBle) {
    el.hidden = false;
    el.classList.add("banner-warn");
    if (iOS) {
      el.innerHTML =
        "<strong>iPhone / iPad:</strong> Apple does not expose Web Bluetooth in Safari (or Chrome on iOS). " +
        "You can still use <strong>History</strong> to import JSON exported from Android or desktop Chrome, and view plots. " +
        "For live BLE, use this page in <strong>Chrome on Android</strong> or <strong>Chrome/Edge on a laptop</strong>.";
    } else {
      el.innerHTML =
        "<strong>Web Bluetooth unavailable.</strong> Use Google Chrome or Microsoft Edge on desktop or Android, " +
        "and serve this app over <code>http://localhost</code> or HTTPS.";
    }
  } else {
    el.hidden = true;
  }
}

function showTab(name) {
  for (const t of document.querySelectorAll("[data-tab]")) {
    t.classList.toggle("active", t.dataset.tab === name);
  }
  for (const p of document.querySelectorAll("[data-panel]")) {
    p.hidden = p.dataset.panel !== name;
  }
  if (name === "history") {
    requestAnimationFrame(() => onSweepChange());
  }
}

async function refreshHistorySelectors() {
  const keys = await listDayKeys();
  const sel = $("sel-day");
  const cur = sel.value;
  sel.innerHTML = "";
  if (keys.length === 0) {
    const o = document.createElement("option");
    o.value = "";
    o.textContent = "No saved days";
    sel.appendChild(o);
    return;
  }
  for (const k of keys) {
    const o = document.createElement("option");
    o.value = k;
    o.textContent = k;
    sel.appendChild(o);
  }
  if (cur && keys.includes(cur)) sel.value = cur;
}

async function onDayChange() {
  const day = $("sel-day").value;
  const sweepSel = $("sel-sweep");
  sweepSel.innerHTML = "";
  if (!day) {
    renderSweep(chartRoot, null);
    $("hist-summary").textContent = "";
    return;
  }
  const rec = await getDay(day);
  const sweeps = rec.sweeps || [];
  if (sweeps.length === 0) {
    const o = document.createElement("option");
    o.textContent = "No sweeps";
    sweepSel.appendChild(o);
    renderSweep(chartRoot, null);
    return;
  }
  sweeps.forEach((s, i) => {
    const o = document.createElement("option");
    o.value = String(i);
    o.textContent = `#${i + 1} — ${s.pstTimeLabel || s.savedAtUtcMs}`;
    sweepSel.appendChild(o);
  });
  sweepSel.selectedIndex = sweeps.length - 1;
  onSweepChange();
}

function onSweepChange() {
  const day = $("sel-day").value;
  const idx = parseInt($("sel-sweep").value, 10);
  if (!day || Number.isNaN(idx)) return;
  void getDay(day).then((rec) => {
    const s = rec.sweeps?.[idx];
    if (s) {
      $("hist-summary").textContent = `Rct: ${s.rct || "—"} Ω   Rs: ${s.rs || "—"} Ω`;
      renderSweep(chartRoot, s);
    }
  });
}

function readSweepOptionsFromForm() {
  const v = (id) => $(id).value.trim();
  const n = (id) => {
    const x = v(id);
    return x === "" ? null : x;
  };
  return {
    rctEst: n("in-rct") ?? "127000",
    rsEst: n("in-rs") ?? "150",
    startFreq: n("in-f0"),
    endFreq: n("in-f1"),
    numPoints: n("in-ppd"),
    numCycles: n("in-cycles"),
    rcalVal: n("in-rcal"),
    biasVolt: n("in-bias"),
    zeroVolt: n("in-zero"),
    delaySecs: n("in-delay"),
    extGain: n("in-extg"),
    dacGain: n("in-dacg"),
  };
}

async function main() {
  showPlatformBanner();

  document.querySelectorAll("[data-tab]").forEach((btn) => {
    btn.addEventListener("click", () => showTab(btn.dataset.tab));
  });

  $("btn-connect").addEventListener("click", async () => {
    try {
      ble.onNotify = onBleNotify;
      ble.onDisconnect = () => setBleUi(false, "Disconnected");
      await ble.connect();
      setBleUi(true, `Connected: ${ble.device.name || "PHEW"}`);
    } catch (e) {
      console.error(e);
      setBleUi(false, String(e.message || e));
    }
  });

  $("btn-disconnect").addEventListener("click", () => {
    ble.disconnect();
    setBleUi(false, "Disconnected");
  });

  $("btn-sweep").addEventListener("click", async () => {
    resetLive();
    $("live-status").textContent = "Sweep running\u2026";
    startProgress();
    try {
      await ble.runSweep(readSweepOptionsFromForm());
    } catch (e) {
      console.error(e);
      stopProgress(false);
      $("sweep-progress").hidden = true;
      $("live-status").textContent = String(e.message || e);
    }
  });

  $("btn-refresh-days").addEventListener("click", async () => {
    await refreshHistorySelectors();
    await onDayChange();
  });

  $("sel-day").addEventListener("change", () => onDayChange());
  $("sel-sweep").addEventListener("change", () => onSweepChange());

  $("btn-export").addEventListener("click", async () => {
    const day = $("sel-day").value;
    if (!day) return;
    const json = await exportDayJson(day);
    const blob = new Blob([json], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = `phew_${day}.json`;
    a.click();
    URL.revokeObjectURL(a.href);
  });

  $("file-import").addEventListener("change", async (ev) => {
    const file = ev.target.files?.[0];
    if (!file) return;
    try {
      const data = JSON.parse(await file.text());
      if (Array.isArray(data.sweeps) && (data.pstDateKey || data.sweeps[0]?.pstDateKey)) {
        await mergeImportedDay(data);
      } else {
        alert("Expected JSON with sweeps[] and pstDateKey (or pstDateKey on each sweep).");
      }
      await refreshHistorySelectors();
      await onDayChange();
    } catch (e) {
      alert("Import failed: " + e);
    }
    ev.target.value = "";
  });

  setBleUi(false, ble.isWebBluetoothAvailable ? "Not connected" : "BLE not supported here");
  await refreshHistorySelectors();
  await onDayChange();
}

main();
