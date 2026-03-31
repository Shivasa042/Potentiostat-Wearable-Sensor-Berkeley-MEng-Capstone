import SwiftUI

struct ContentView: View {
    @StateObject private var ble = HelpStatBleManager()

    var body: some View {
        TabView {
            LiveView()
                .environmentObject(ble)
                .tabItem { Label("Live", systemImage: "antenna.radiowaves.left.and.right") }

            HistoryView()
                .tabItem { Label("History", systemImage: "calendar") }

            AboutView()
                .tabItem { Label("About", systemImage: "info.circle") }
        }
    }
}

#Preview {
    ContentView()
}
