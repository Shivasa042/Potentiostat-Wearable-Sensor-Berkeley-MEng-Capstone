import SwiftUI

struct AboutView: View {
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    Text("HELPStat Companion")
                        .font(.title2.weight(.bold))
                    Text("Native iOS client for the Berkeley MEng HELPStat wearable potentiostat. Connect over Bluetooth Low Energy, run EIS sweeps, and browse results grouped by Pacific calendar day.")
                        .foregroundStyle(.secondary)

                    GroupBox("Data") {
                        Text("History files live under Application Support (JSON per day). Export shares the same format as the web app for cross-device import.")
                    }

                    GroupBox("Hardware") {
                        Text("Advertised name: HELPStat. Uses the same GATT service and characteristics as the ESP32 firmware in this repository.")
                    }
                }
                .padding()
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .navigationTitle("About")
        }
    }
}

#Preview {
    AboutView()
}
