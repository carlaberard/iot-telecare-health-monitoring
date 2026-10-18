// IoT Telecare System — Bangle.js / Espruino
// Academic prototype. Sends wearable sensor data to the Arduino over BLE.

Bangle.setHRMPower(1);
Bangle.setGPSPower(1);
Bangle.setCompassPower(1);

let currentHeartRate = 0;
let totalSteps = 0;
let gpsCoordinates = "0.00000,0.00000";
let compassHeading = 0;

Bangle.on('HRM', hrm => {
  currentHeartRate = hrm.bpm;
});
Bangle.on('step', () => totalSteps++);
Bangle.on('GPS', fix => {
  if (fix.fix) {
    gpsCoordinates = fix.lat.toFixed(5) + "," + fix.lon.toFixed(5);
  }
});
Bangle.on('mag', () => {
  compassHeading = Math.round(Bangle.getCompass().heading);
});

let gattConnection = null;
let bleService = null;
let bleCharacteristic = null;

NRF.requestDevice({
  filters: [{ services: ["12345678-1234-5678-1234-56789abcdef0"] }]
}).then(device => {
  console.log("Device found:", device);
  return device.gatt.connect();
}).then(gatt => {
  gattConnection = gatt;
  return gatt.getPrimaryService("12345678-1234-5678-1234-56789abcdef0");
}).then(service => {
  bleService = service;
  return service.getCharacteristic("12345678-1234-5678-1234-56789abcdef1");
}).then(characteristic => {
  bleCharacteristic = characteristic;

  // Transmit a JSON payload every 10 seconds.
  setInterval(() => {
    const message = JSON.stringify({
      hr: currentHeartRate,
      steps: totalSteps,
      gps: gpsCoordinates,
      compass: compassHeading
    });
    bleCharacteristic.writeValue(message).then(() => {
      console.log("Sent:", message);
    }).catch(error => console.log("BLE write failed:", error));
  }, 10000);
}).catch(error => {
  console.log("BLE connection failed:", error);
});
