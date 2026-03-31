/**
 * IndexedDB — one object per Pacific calendar day (America/Los_Angeles).
 */

const DB_NAME = "helpstat_web_app";
const DB_VER = 1;
const STORE = "days";

export function pstDateKey(d = new Date()) {
  return d.toLocaleDateString("en-CA", { timeZone: "America/Los_Angeles" });
}

export function pstTimeLabel(d = new Date()) {
  return d.toLocaleTimeString("en-US", {
    timeZone: "America/Los_Angeles",
    hour: "numeric",
    minute: "2-digit",
    timeZoneName: "short",
  });
}

function openDb() {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, DB_VER);
    req.onerror = () => reject(req.error);
    req.onsuccess = () => resolve(req.result);
    req.onupgradeneeded = (e) => {
      const db = e.target.result;
      if (!db.objectStoreNames.contains(STORE)) {
        db.createObjectStore(STORE, { keyPath: "pstDateKey" });
      }
    };
  });
}

async function getDayRecord(db, key) {
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE, "readonly");
    const r = tx.objectStore(STORE).get(key);
    r.onerror = () => reject(r.error);
    r.onsuccess = () =>
      resolve(
        r.result || {
          pstDateKey: key,
          sweeps: [],
          version: 1,
          pstZone: "America/Los_Angeles",
        },
      );
  });
}

async function putDayRecord(db, record) {
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE, "readwrite");
    const r = tx.objectStore(STORE).put(record);
    r.onerror = () => reject(r.error);
    r.onsuccess = () => resolve();
  });
}

export async function listDayKeys() {
  const db = await openDb();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE, "readonly");
    const r = tx.objectStore(STORE).getAllKeys();
    r.onerror = () => reject(r.error);
    r.onsuccess = () => resolve((r.result || []).sort().reverse());
  });
}

export async function getDay(key) {
  const db = await openDb();
  return getDayRecord(db, key);
}

export async function appendSweep(sweep) {
  const db = await openDb();
  const key = sweep.pstDateKey || pstDateKey(new Date(sweep.savedAtUtcMs));
  const rec = await getDayRecord(db, key);
  rec.sweeps = rec.sweeps || [];
  rec.sweeps.push(sweep);
  rec.version = 1;
  rec.pstZone = "America/Los_Angeles";
  await putDayRecord(db, rec);
}

export async function mergeImportedDay(record) {
  if (!record || !Array.isArray(record.sweeps)) return;
  const dayKey =
    record.pstDateKey || (record.sweeps[0] && record.sweeps[0].pstDateKey) || null;
  if (!dayKey) return;
  const db = await openDb();
  const existing = await getDayRecord(db, dayKey);
  const merged = new Map();
  for (const s of existing.sweeps || []) {
    merged.set(`${s.savedAtUtcMs}_${s.points?.length}`, s);
  }
  for (const s of record.sweeps) {
    merged.set(`${s.savedAtUtcMs}_${s.points?.length}`, s);
  }
  existing.pstDateKey = dayKey;
  existing.sweeps = [...merged.values()].sort((a, b) => a.savedAtUtcMs - b.savedAtUtcMs);
  existing.version = 1;
  existing.pstZone = "America/Los_Angeles";
  await putDayRecord(db, existing);
}

export async function exportDayJson(dayKey) {
  const rec = await getDay(dayKey);
  return JSON.stringify(rec, null, 2);
}
