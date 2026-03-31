import SwiftUI

struct LiveView: View {
    @EnvironmentObject private var ble: HelpStatBleManager
    @State private var options = SweepFormOptions()

    var body: some View {
        NavigationStack {
            Form {
                Section("Bluetooth") {
                    Text(ble.statusLine)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    HStack {
                        Button("Scan & connect") {
                            ble.startScan()
                        }
                        .disabled(!ble.bluetoothReady || ble.isSweeping)
                        Button("Disconnect") {
                            ble.disconnect()
                        }
                        .disabled(ble.phase == .idle)
                    }
                }

                Section("Sweep") {
                    Button("Run sweep") {
                        Task {
                            await ble.runSweep(options: options)
                        }
                    }
                    .disabled(!ble.isConnected || ble.isSweeping)
                }

                Section("Parameters (optional)") {
                    TextField("Rct est (Ω)", text: $options.rctEst)
                    TextField("Rs est (Ω)", text: $options.rsEst)
                    TextField("Start freq (Hz)", text: $options.startFreq)
                    TextField("End freq (Hz)", text: $options.endFreq)
                    TextField("Points / decade", text: $options.numPoints)
                    TextField("Num cycles", text: $options.numCycles)
                    TextField("Rcal (Ω)", text: $options.rcalVal)
                    TextField("Bias V", text: $options.biasVolt)
                    TextField("Zero V", text: $options.zeroVolt)
                    TextField("Delay (s)", text: $options.delaySecs)
                    TextField("Ext gain", text: $options.extGain)
                    TextField("DAC gain", text: $options.dacGain)
                }
            }
            .navigationTitle("HELPStat")
        }
    }
}

#Preview {
    LiveView()
        .environmentObject(HelpStatBleManager())
}
