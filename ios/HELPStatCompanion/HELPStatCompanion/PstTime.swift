import Foundation

enum PstTime {
    private static var tz: TimeZone { TimeZone(identifier: "America/Los_Angeles") ?? .current }

    static func dateKey(for date: Date = Date()) -> String {
        let cal = Calendar(identifier: .gregorian)
        var c = cal.dateComponents(in: tz, from: date)
        let y = c.year ?? 0
        let m = c.month ?? 0
        let d = c.day ?? 0
        return String(format: "%04d-%02d-%02d", y, m, d)
    }

    static func timeLabel(for date: Date = Date()) -> String {
        let f = DateFormatter()
        f.timeZone = tz
        f.dateStyle = .none
        f.timeStyle = .short
        f.locale = Locale(identifier: "en_US_POSIX")
        return f.string(from: date) + " " + (tz.abbreviation(for: date) ?? "")
    }
}
