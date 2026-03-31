import Charts
import SwiftUI
import UniformTypeIdentifiers

struct HistoryView: View {
    @ObservedObject private var history = HistoryStore.shared

    @State private var selectedDay: String = ""
    @State private var selectedSweepIndex: Int = 0
    @State private var importVisible = false
    @State private var importError: String?

    private var sweeps: [EisSweep] {
        guard !selectedDay.isEmpty else { return [] }
        return history.loadDay(selectedDay).sweeps
    }

    private var selectedSweep: EisSweep? {
        guard selectedSweepIndex >= 0, selectedSweepIndex < sweeps.count else { return nil }
        return sweeps[selectedSweepIndex]
    }

    private var importAlertPresented: Binding<Bool> {
        Binding(
            get: { importError != nil },
            set: { if !$0 { importError = nil } }
        )
    }

    var body: some View {
        NavigationStack {
            VStack(alignment: .leading, spacing: 12) {
                Text("Days use America/Los_Angeles (PST/PDT).")
                    .font(.footnote)
                    .foregroundStyle(.secondary)

                HStack {
                    Picker("Day", selection: $selectedDay) {
                        Text("—").tag("")
                        ForEach(history.dayKeys, id: \.self) { k in
                            Text(k).tag(k)
                        }
                    }
                    .pickerStyle(.menu)

                    Picker("Sweep", selection: $selectedSweepIndex) {
                        if sweeps.isEmpty {
                            Text("—").tag(0)
                        } else {
                            ForEach(Array(sweeps.enumerated()), id: \.offset) { i, s in
                                Text("#\(i + 1) \(s.pstTimeLabel)").tag(i)
                            }
                        }
                    }
                    .pickerStyle(.menu)
                    .disabled(sweeps.isEmpty)
                }

                if let s = selectedSweep {
                    Text("Rct: \(s.rct.isEmpty ? "—" : s.rct) Ω    Rs: \(s.rs.isEmpty ? "—" : s.rs) Ω")
                        .font(.subheadline.weight(.semibold))
                }

                if let s = selectedSweep {
                    SweepChartsView(sweep: s)
                } else {
                    Spacer()
                    Text("No sweep")
                        .font(.title3)
                        .foregroundStyle(.secondary)
                    Text("Run a measurement on Live or import JSON.")
                        .font(.footnote)
                        .foregroundStyle(.tertiary)
                    Spacer()
                }

                HStack {
                    Button("Refresh") {
                        history.reloadDayKeys()
                        syncPickers()
                    }
                    Button("Import JSON…") {
                        importVisible = true
                    }
                    if !selectedDay.isEmpty, let url = try? HistoryStore.shared.writeTempExportFile(dayKey: selectedDay) {
                        ShareLink(item: url) {
                            Label("Export", systemImage: "square.and.arrow.up")
                        }
                    }
                }
                .font(.subheadline)
            }
            .padding()
            .navigationTitle("History")
            .onAppear {
                history.reloadDayKeys()
                syncPickers()
            }
            .onChange(of: history.dayKeys) { _ in
                syncPickers()
            }
            .onChange(of: selectedDay) { _ in
                let list = history.loadDay(selectedDay).sweeps
                selectedSweepIndex = list.isEmpty ? 0 : list.count - 1
            }
            .fileImporter(isPresented: $importVisible, allowedContentTypes: [.json]) { result in
                switch result {
                case .success(let url):
                    importJson(from: url)
                case .failure(let err):
                    importError = err.localizedDescription
                }
            }
            .alert("Import", isPresented: importAlertPresented) {
                Button("OK", role: .cancel) { importError = nil }
            } message: {
                Text(importError ?? "")
            }
        }
    }

    private func syncPickers() {
        if history.dayKeys.isEmpty {
            selectedDay = ""
            selectedSweepIndex = 0
            return
        }
        if selectedDay.isEmpty || !history.dayKeys.contains(selectedDay) {
            selectedDay = history.dayKeys[0]
        }
        let list = history.loadDay(selectedDay).sweeps
        selectedSweepIndex = list.isEmpty ? 0 : min(selectedSweepIndex, list.count - 1)
        if !list.isEmpty, selectedSweepIndex < 0 {
            selectedSweepIndex = list.count - 1
        }
    }

    private func importJson(from url: URL) {
        guard url.startAccessingSecurityScopedResource() else {
            importError = "Could not read file."
            return
        }
        defer { url.stopAccessingSecurityScopedResource() }
        do {
            let data = try Data(contentsOf: url)
            try history.mergeImport(data: data)
            history.reloadDayKeys()
            syncPickers()
        } catch {
            importError = error.localizedDescription
        }
    }
}

private struct SweepChartsView: View {
    let sweep: EisSweep

    private var ordered: [EisPoint] {
        sweep.points.sorted { $0.f < $1.f }
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text("Nyquist")
                    .font(.headline)
                Chart {
                    ForEach(Array(ordered.enumerated()), id: \.offset) { _, p in
                        LineMark(
                            x: .value("Z'", p.re),
                            y: .value("Z\"", p.im)
                        )
                        .foregroundStyle(.cyan)
                    }
                }
                .chartXAxisLabel("Z' (Ω)")
                .chartYAxisLabel("Z'' (Ω)")
                .frame(height: 240)

                Text("Bode magnitude")
                    .font(.headline)
                Chart {
                    ForEach(Array(ordered.enumerated()), id: \.offset) { _, p in
                        LineMark(
                            x: .value("log f", log10(max(p.f, 1e-12))),
                            y: .value("|Z|", p.mag)
                        )
                        .foregroundStyle(.green)
                    }
                }
                .chartXAxisLabel("log10(f)")
                .chartYAxisLabel("|Z| (Ω)")
                .frame(height: 200)

                Text("Bode phase")
                    .font(.headline)
                Chart {
                    ForEach(Array(ordered.enumerated()), id: \.offset) { _, p in
                        LineMark(
                            x: .value("log f", log10(max(p.f, 1e-12))),
                            y: .value("phi", p.ph)
                        )
                        .foregroundStyle(.purple)
                    }
                }
                .chartXAxisLabel("log10(f)")
                .chartYAxisLabel("Phase (°)")
                .frame(height: 200)
            }
        }
    }
}

#Preview {
    HistoryView()
}
