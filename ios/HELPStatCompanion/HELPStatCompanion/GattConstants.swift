import CoreBluetooth

/// GATT layout — must match ESP32 HELPStat firmware (HELPStatLib / `HELPStat.h`).
enum GattConstants {
    static let deviceName = "HELPStat"

    static let serviceUUID = CBUUID(string: "4FAFC201-1FB5-459E-8FCC-C5C9C331914B")

    static let start = CBUUID(string: "BEB5483E-36E1-4688-B7F5-EA07361B26A8")
    static let rct = CBUUID(string: "A5D42EE9-0551-4A23-A1B7-74EEA28AA083")
    static let rs = CBUUID(string: "192FA626-1E5A-4018-8176-5DEBFF81A6C6")
    static let numCycles = CBUUID(string: "8117179A-B8EE-433C-96DA-65816C5C92DD")
    static let numPoints = CBUUID(string: "359A6D93-9007-41F6-BBBE-F92BC17DB383")
    static let startFreq = CBUUID(string: "5B0210D0-CD21-4011-9882-DB983BA7E1FC")
    static let endFreq = CBUUID(string: "3507ABDC-2353-486B-A3D5-DD831EE4BB18")
    static let rcalVal = CBUUID(string: "4F7D237E-A358-439E-8771-4AB7F81473FA")
    static let dacGain = CBUUID(string: "36377D50-6BA7-4CC1-825A-42746C4028DC")
    static let extGain = CBUUID(string: "E17E690A-16E8-4C70-B958-73E41D4AFFF0")
    static let zeroVolt = CBUUID(string: "60D57F7B-6E41-41E5-BD44-0E23638E90D2")
    static let biasVolt = CBUUID(string: "62DF1950-23F9-4ACD-8473-61A421D4CF07")
    static let delaySecs = CBUUID(string: "57A7466E-C0E1-4F6E-AEA4-99EF4F360D24")
    static let currFreq = CBUUID(string: "893028D3-54B4-4D59-A03B-ECE286572E4A")
    static let real = CBUUID(string: "67C0488C-E330-438C-A88D-59ABFCFBB527")
    static let imag = CBUUID(string: "E080F979-BB39-4151-8082-755E3AE6F055")
    static let phase = CBUUID(string: "6A5A437F-4E3C-4A57-BF99-C4859F6AC411")
    static let magnitude = CBUUID(string: "06192C1E-8588-4808-91B8-C4F1D650893D")

    /// Characteristics that should have notify enabled during a sweep.
    static let notifyUUIDs: [CBUUID] = [rct, rs, real, imag, currFreq, phase, magnitude]
}
