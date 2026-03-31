import Foundation

@MainActor
final class HistoryStore: ObservableObject {
    static let shared = HistoryStore()

    @Published private(set) var dayKeys: [String] = []

    private let dirName = "helpstat_history"
    private var dirURL: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
        let url = base.appendingPathComponent("HELPStatCompanion", isDirectory: true)
            .appendingPathComponent(dirName, isDirectory: true)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    private init() {
        reloadDayKeys()
    }

    func reloadDayKeys() {
        let fm = FileManager.default
        guard let names = try? fm.contentsOfDirectory(at: dirURL, includingPropertiesForKeys: nil) else {
            dayKeys = []
            return
        }
        dayKeys = names
            .filter { $0.pathExtension == "json" }
            .map { $0.deletingPathExtension().lastPathComponent }
            .sorted()
            .reversed()
    }

    func url(forDayKey key: String) -> URL {
        dirURL.appendingPathComponent("\(key).json")
    }

    func loadDay(_ key: String) -> DayRecord {
        let url = url(forDayKey: key)
        guard let data = try? Data(contentsOf: url),
              let decoded = try? JSONDecoder().decode(DayRecord.self, from: data)
        else {
            return DayRecord(pstDateKey: key, sweeps: [])
        }
        return decoded
    }

    func appendSweep(_ sweep: EisSweep) throws {
        let key = sweep.pstDateKey
        var record = loadDay(key)
        record.sweeps.append(sweep)
        record.version = 1
        record.pstZone = "America/Los_Angeles"
        let enc = JSONEncoder()
        enc.outputFormatting = [.prettyPrinted, .sortedKeys]
        let data = try enc.encode(record)
        try data.write(to: url(forDayKey: key), options: .atomic)
        reloadDayKeys()
    }

    /// Merge imported day JSON (from web export or Android).
    func mergeImport(data: Data) throws {
        let incoming = try JSONDecoder().decode(DayRecord.self, from: data)
        let key = incoming.pstDateKey.isEmpty ? (incoming.sweeps.first?.pstDateKey ?? "") : incoming.pstDateKey
        guard !key.isEmpty else { return }
        var existing = loadDay(key)
        var seen = Set<String>()
        for s in existing.sweeps {
            seen.insert("\(s.savedAtUtcMs)_\(s.points.count)")
        }
        for s in incoming.sweeps {
            let id = "\(s.savedAtUtcMs)_\(s.points.count)"
            if seen.insert(id).inserted {
                existing.sweeps.append(s)
            }
        }
        existing.sweeps.sort { $0.savedAtUtcMs < $1.savedAtUtcMs }
        existing.pstDateKey = key
        existing.version = 1
        existing.pstZone = "America/Los_Angeles"
        let enc = JSONEncoder()
        enc.outputFormatting = [.prettyPrinted, .sortedKeys]
        let out = try enc.encode(existing)
        try out.write(to: url(forDayKey: key), options: .atomic)
        reloadDayKeys()
    }

    func exportDayJSON(_ key: String) throws -> Data {
        let record = loadDay(key)
        var r = record
        r.pstDateKey = key
        let enc = JSONEncoder()
        enc.outputFormatting = [.prettyPrinted, .sortedKeys]
        return try enc.encode(r)
    }

    func writeTempExportFile(dayKey: String) throws -> URL {
        let data = try exportDayJSON(dayKey)
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("helpstat_\(dayKey).json")
        try data.write(to: url, options: .atomic)
        return url
    }
}
