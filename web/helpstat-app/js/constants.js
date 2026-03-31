/** GATT layout — must match ESP32 HELPStat firmware (HELPStatLib). */

export const DEVICE_NAME_FILTER = "HELPStat";

export const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";

/** All characteristics we may read/write or notify. */
export const CHAR = {
  start: "beb5483e-36e1-4688-b7f5-ea07361b26a8",
  rct: "a5d42ee9-0551-4a23-a1b7-74eea28aa083",
  rs: "192fa626-1e5a-4018-8176-5debff81a6c6",
  numCycles: "8117179a-b8ee-433c-96da-65816c5c92dd",
  numPoints: "359a6d93-9007-41f6-bbbe-f92bc17db383",
  startFreq: "5b0210d0-cd21-4011-9882-db983ba7e1fc",
  endFreq: "3507abdc-2353-486b-a3d5-dd831ee4bb18",
  rcalVal: "4f7d237e-a358-439e-8771-4ab7f81473fa",
  dacGain: "36377d50-6ba7-4cc1-825a-42746c4028dc",
  extGain: "e17e690a-16e8-4c70-b958-73e41d4afff0",
  zeroVolt: "60d57f7b-6e41-41e5-bd44-0e23638e90d2",
  biasVolt: "62df1950-23f9-4acd-8473-61a421d4cf07",
  delaySecs: "57a7466e-c0e1-4f6e-aea4-99ef4f360d24",
  fileName: "d07519f0-1c45-461a-9b8e-fcaad4e53f0c",
  folderName: "02193c1e-4afe-4211-b64f-e878e9d6c0a4",
  sweepIndex: "8c5cf012-5717-4e4b-a702-4d34ba80dc9a",
  currFreq: "893028d3-54b4-4d59-a03b-ece286572e4a",
  real: "67c0488c-e330-438c-a88d-59abfcfbb527",
  imag: "e080f979-bb39-4151-8082-755e3ae6f055",
  phase: "6a5a437f-4e3c-4a57-bf99-c4859f6ac411",
  magnitude: "06192c1e-8588-4808-91b8-c4f1d650893d",
};

export const NOTIFY_KEYS = ["rct", "rs", "real", "imag", "currFreq", "phase", "magnitude"];
