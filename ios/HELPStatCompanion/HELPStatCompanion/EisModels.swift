import Foundation

struct EisPoint: Codable, Hashable {
    var f: Double
    var re: Double
    var im: Double
    var ph: Double
    var mag: Double

    enum CodingKeys: String, CodingKey {
        case f, re, im, ph, mag
    }
}

struct EisSweep: Codable, Identifiable {
    var id: String { "\(savedAtUtcMs)_\(points.count)" }
    var savedAtUtcMs: Int64
    var pstDateKey: String
    var pstTimeLabel: String
    var rct: String
    var rs: String
    var points: [EisPoint]
}

/// Root JSON object per day file (matches web / Android shape).
struct DayRecord: Codable {
    var pstDateKey: String
    var sweeps: [EisSweep]
    var version: Int
    var pstZone: String

    init(pstDateKey: String, sweeps: [EisSweep]) {
        self.pstDateKey = pstDateKey
        self.sweeps = sweeps
        self.version = 1
        self.pstZone = "America/Los_Angeles"
    }

    enum CodingKeys: String, CodingKey {
        case pstDateKey, sweeps, version, pstZone
    }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        pstDateKey = try c.decodeIfPresent(String.self, forKey: .pstDateKey) ?? ""
        sweeps = try c.decodeIfPresent([EisSweep].self, forKey: .sweeps) ?? []
        version = try c.decodeIfPresent(Int.self, forKey: .version) ?? 1
        pstZone = try c.decodeIfPresent(String.self, forKey: .pstZone) ?? "America/Los_Angeles"
    }

    func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: CodingKeys.self)
        try c.encode(pstDateKey, forKey: .pstDateKey)
        try c.encode(sweeps, forKey: .sweeps)
        try c.encode(version, forKey: .version)
        try c.encode(pstZone, forKey: .pstZone)
    }
}

struct SweepFormOptions {
    var rctEst: String = "5000"
    var rsEst: String = "100"
    var startFreq: String = ""
    var endFreq: String = ""
    var numPoints: String = ""
    var numCycles: String = ""
    var rcalVal: String = ""
    var biasVolt: String = ""
    var zeroVolt: String = ""
    var delaySecs: String = ""
    var extGain: String = ""
    var dacGain: String = ""
}
