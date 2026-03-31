import CoreBluetooth
import Foundation

@MainActor
final class HelpStatBleManager: NSObject, ObservableObject {
    enum Phase: Equatable {
        case idle
        case scanning
        case connecting
        case connected
        case error(String)
    }

    @Published private(set) var phase: Phase = .idle
    @Published var statusLine = ""
    @Published private(set) var isSweeping = false

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var characteristics: [CBUUID: CBCharacteristic] = [:]

    private var listReal: [Float] = []
    private var listImag: [Float] = []
    private var listFreq: [Float] = []
    private var listPhase: [Float] = []
    private var listMagnitude: [Float] = []
    private var calculatedRct: String?
    private var calculatedRs: String?

    private var writeContinuation: CheckedContinuation<Void, Never>?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    var bluetoothReady: Bool {
        central.state == .poweredOn
    }

    var isConnected: Bool {
        if case .connected = phase { return true }
        return false
    }

    func startScan() {
        guard central.state == .poweredOn else {
            statusLine = "Turn on Bluetooth"
            return
        }
        disconnect()
        peripheral = nil
        characteristics.removeAll()
        resetBuffers()
        phase = .scanning
        statusLine = "Scanning for HELPStat…"
        central.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    func disconnect() {
        central.stopScan()
        if let p = peripheral {
            central.cancelPeripheralConnection(p)
        }
        peripheral = nil
        characteristics.removeAll()
        phase = .idle
        statusLine = "Disconnected"
    }

    func runSweep(options: SweepFormOptions) async {
        guard isConnected, let p = peripheral else {
            statusLine = "Not connected"
            return
        }
        guard !isSweeping else { return }
        isSweeping = true
        defer { isSweeping = false }

        resetBuffers()
        calculatedRct = nil
        calculatedRs = nil
        statusLine = "Sweep running…"

        for uuid in GattConstants.notifyUUIDs {
            if let c = characteristics[uuid] {
                p.setNotifyValue(true, for: c)
            }
        }
        try? await Task.sleep(nanoseconds: 400_000_000)

        func w(_ text: String, _ uuid: CBUUID) async {
            guard !text.isEmpty, let c = characteristics[uuid] else { return }
            guard let data = text.data(using: .utf8) else { return }
            await writeData(data, characteristic: c)
            try? await Task.sleep(nanoseconds: 60_000_000)
        }

        await w(options.startFreq, GattConstants.startFreq)
        await w(options.endFreq, GattConstants.endFreq)
        await w(options.numPoints, GattConstants.numPoints)
        await w(options.numCycles, GattConstants.numCycles)
        await w(options.rcalVal, GattConstants.rcalVal)
        await w(options.biasVolt, GattConstants.biasVolt)
        await w(options.zeroVolt, GattConstants.zeroVolt)
        await w(options.delaySecs, GattConstants.delaySecs)
        await w(options.extGain, GattConstants.extGain)
        await w(options.dacGain, GattConstants.dacGain)

        await w(options.rctEst, GattConstants.rct)
        await w(options.rsEst, GattConstants.rs)

        if let c = characteristics[GattConstants.start] {
            await writeData(Data([1]), characteristic: c)
            try? await Task.sleep(nanoseconds: 80_000_000)
            await writeData(Data([0]), characteristic: c)
        }

        statusLine = "Waiting for data…"
    }

    private func resetBuffers() {
        listReal.removeAll()
        listImag.removeAll()
        listFreq.removeAll()
        listPhase.removeAll()
        listMagnitude.removeAll()
    }

    private func writeData(_ data: Data, characteristic: CBCharacteristic) async {
        guard let p = peripheral else { return }
        await withCheckedContinuation { (cont: CheckedContinuation<Void, Never>) in
            writeContinuation = cont
            p.writeValue(data, for: characteristic, type: .withResponse)
        }
    }

    private func floatString(from data: Data) -> Float? {
        let s = String(data: data, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        return Float(s)
    }

    private func handleNotify(uuid: CBUUID, data: Data) {
        switch uuid {
        case GattConstants.real:
            if let v = floatString(from: data) { listReal.append(v) }
        case GattConstants.imag:
            if let v = floatString(from: data) { listImag.append(v) }
        case GattConstants.currFreq:
            if let v = floatString(from: data) { listFreq.append(v) }
        case GattConstants.phase:
            if var v = floatString(from: data) {
                if v > 180 { v -= 360 }
                else if v < -180 { v += 360 }
                listPhase.append(v)
            }
        case GattConstants.magnitude:
            if let v = floatString(from: data) { listMagnitude.append(v) }
        case GattConstants.rct:
            calculatedRct = String(data: data, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines)
        case GattConstants.rs:
            calculatedRs = String(data: data, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines)
            finalizeSweepIfComplete()
        default:
            break
        }
    }

    private func finalizeSweepIfComplete() {
        let n = listFreq.count
        guard n > 0,
              listReal.count == n, listImag.count == n,
              listPhase.count == n, listMagnitude.count == n
        else {
            statusLine = "Sweep ended (incomplete points)"
            return
        }

        let now = Date()
        let ms = Int64(now.timeIntervalSince1970 * 1000)
        var points: [EisPoint] = []
        points.reserveCapacity(n)
        for i in 0 ..< n {
            points.append(
                EisPoint(
                    f: Double(listFreq[i]),
                    re: Double(listReal[i]),
                    im: Double(listImag[i]),
                    ph: Double(listPhase[i]),
                    mag: Double(listMagnitude[i])
                )
            )
        }
        let sweep = EisSweep(
            savedAtUtcMs: ms,
            pstDateKey: PstTime.dateKey(for: now),
            pstTimeLabel: PstTime.timeLabel(for: now),
            rct: calculatedRct ?? "",
            rs: calculatedRs ?? "",
            points: points
        )
        do {
            try HistoryStore.shared.appendSweep(sweep)
            statusLine = "Saved — \(sweep.pstTimeLabel)"
        } catch {
            statusLine = "Save failed: \(error.localizedDescription)"
        }
    }
}

extension HelpStatBleManager: CBCentralManagerDelegate {
    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor in
            switch central.state {
            case .poweredOn:
                self.statusLine = "Bluetooth on — tap Scan"
            case .poweredOff:
                self.phase = .error("Bluetooth off")
                self.statusLine = "Bluetooth off"
            case .unauthorized:
                self.statusLine = "Bluetooth permission denied"
            default:
                break
            }
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String
        guard name == GattConstants.deviceName else { return }
        Task { @MainActor in
            central.stopScan()
            self.peripheral = peripheral
            peripheral.delegate = self
            self.phase = .connecting
            self.statusLine = "Connecting…"
            central.connect(peripheral, options: nil)
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        Task { @MainActor in
            self.phase = .connected
            self.statusLine = "Discovering service…"
            peripheral.discoverServices([GattConstants.serviceUUID])
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor in
            self.phase = .error(error?.localizedDescription ?? "Connect failed")
            self.statusLine = error?.localizedDescription ?? "Connect failed"
            self.peripheral = nil
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor in
            self.peripheral = nil
            self.characteristics.removeAll()
            self.phase = .idle
            self.statusLine = error?.localizedDescription ?? "Disconnected"
        }
    }
}

extension HelpStatBleManager: CBPeripheralDelegate {
    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        Task { @MainActor in
            if let error {
                self.statusLine = error.localizedDescription
                return
            }
            guard let svc = peripheral.services?.first(where: { $0.uuid == GattConstants.serviceUUID }) else {
                self.statusLine = "Service not found"
                return
            }
            peripheral.discoverCharacteristics(nil, for: svc)
        }
    }

    nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        Task { @MainActor in
            if let error {
                self.statusLine = error.localizedDescription
                return
            }
            for c in service.characteristics ?? [] {
                self.characteristics[c.uuid] = c
            }
            self.statusLine = "Ready — \(self.characteristics.count) characteristics"
        }
    }

    nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didWriteValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        Task { @MainActor in
            self.writeContinuation?.resume()
            self.writeContinuation = nil
            if let error {
                self.statusLine = "Write error: \(error.localizedDescription)"
            }
        }
    }

    nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        guard let data = characteristic.value else { return }
        let uuid = characteristic.uuid
        Task { @MainActor in
            self.handleNotify(uuid: uuid, data: data)
        }
    }
}
