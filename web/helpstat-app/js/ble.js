import { CHAR, DEVICE_NAME_FILTER, NOTIFY_KEYS, SERVICE_UUID } from "./constants.js";

function dec(v) {
  return new TextDecoder().decode(v).trim();
}

export function floatFromBytes(v) {
  return parseFloat(dec(v));
}

export class HelpStatBle {
  constructor() {
    this.device = null;
    this.server = null;
    /** @type {Record<string, BluetoothRemoteGATTCharacteristic>} */
    this.chars = {};
    this._notified = new Set();
    this.onDisconnect = null;
    this.onNotify = null;
  }

  get isWebBluetoothAvailable() {
    return typeof navigator !== "undefined" && !!navigator.bluetooth;
  }

  async connect() {
    if (!this.isWebBluetoothAvailable) {
      throw new Error("Web Bluetooth not available in this browser.");
    }
    this._notified.clear();
    this.device = await navigator.bluetooth.requestDevice({
      filters: [{ name: DEVICE_NAME_FILTER }],
      optionalServices: [SERVICE_UUID],
    });
    this.device.addEventListener("gattserverdisconnected", () => {
      this._notified.clear();
      this.chars = {};
      this.server = null;
      if (this.onDisconnect) this.onDisconnect();
    });
    this.server = await this.device.gatt.connect();
    const service = await this.server.getPrimaryService(SERVICE_UUID);
    this.chars = {};
    for (const [key, uuid] of Object.entries(CHAR)) {
      try {
        this.chars[key] = await service.getCharacteristic(uuid);
      } catch {
        /* optional on some builds */
      }
    }
  }

  disconnect() {
    if (this.device?.gatt?.connected) {
      this.device.gatt.disconnect();
    }
  }

  async _ensureNotify(key) {
    const c = this.chars[key];
    if (!c) return;
    const id = c.uuid.toLowerCase();
    if (this._notified.has(id)) return;
    this._notified.add(id);
    await c.startNotifications();
    c.addEventListener("characteristicvaluechanged", (ev) => {
      if (this.onNotify) this.onNotify(key, ev.target.value);
    });
  }

  async enableAllNotifications() {
    for (const k of NOTIFY_KEYS) {
      await this._ensureNotify(k);
    }
  }

  async writeUtf8(key, text) {
    const c = this.chars[key];
    if (!c) throw new Error(`Missing characteristic ${key}`);
    await c.writeValue(new TextEncoder().encode(String(text)));
  }

  async writeByte(key, b) {
    const c = this.chars[key];
    if (!c) throw new Error(`Missing characteristic ${key}`);
    await c.writeValue(Uint8Array.of(b & 0xff));
  }

  /**
   * Mirror firmware/Android sequence: optional params, then start 1 → start 0.
   * @param {object} opts
   */
  async runSweep(opts) {
    await this.enableAllNotifications();
    const w = (k, v) => this.writeUtf8(k, v).catch(() => {});

    if (opts.startFreq != null) await w("startFreq", opts.startFreq);
    if (opts.endFreq != null) await w("endFreq", opts.endFreq);
    if (opts.numPoints != null) await w("numPoints", opts.numPoints);
    if (opts.numCycles != null) await w("numCycles", opts.numCycles);
    if (opts.rcalVal != null) await w("rcalVal", opts.rcalVal);
    if (opts.biasVolt != null) await w("biasVolt", opts.biasVolt);
    if (opts.zeroVolt != null) await w("zeroVolt", opts.zeroVolt);
    if (opts.delaySecs != null) await w("delaySecs", opts.delaySecs);
    if (opts.extGain != null) await w("extGain", opts.extGain);
    if (opts.dacGain != null) await w("dacGain", opts.dacGain);

    await this.writeUtf8("rct", String(opts.rctEst ?? "5000"));
    await this.writeUtf8("rs", String(opts.rsEst ?? "100"));
    await this.writeByte("start", 1);
    await this.writeByte("start", 0);
  }
}
