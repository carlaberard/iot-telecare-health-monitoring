// IoT Telecare System — Arduino UNO R4 WiFi
// Academic prototype; review pin compatibility and hardware configuration before deployment.
// Replace placeholder values locally. Do not commit real credentials to a public repository.

#include <ArduinoBLE.h>
#include <WiFiS3.h>
#include <SoftwareSerial.h>

BLEService telecareService("12345678-1234-5678-1234-56789abcdef0");
BLECharacteristic sensorDataCharacteristic(
    "12345678-1234-5678-1234-56789abcdef1", BLEWrite, 64);

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* server = "api.thingspeak.com";
const char* apiKey = "YOUR_THINGSPEAK_WRITE_API_KEY";
const char* alertPhone = "YOUR_ALERT_PHONE_NUMBER";

WiFiClient client;

#define MODEM_RX 0
#define MODEM_TX 1
SoftwareSerial sim900(MODEM_RX, MODEM_TX);

void onReceive(BLEDevice central, BLECharacteristic characteristic);
void sendToThingSpeak(int hr, int steps, float lat, float lon, int compass);
void sendSMS(String message);
String getValue(String data, String key);
void printWifiStatus();
void restartSIM900();

void setup() {
  Serial.begin(9600);
  sim900.begin(9600);

  if (!BLE.begin()) {
    Serial.println("BLE initialization failed");
    while (1) {}
  }

  BLE.setLocalName("ArduinoTeleasistencia");
  BLE.setDeviceName("ArduinoTeleasistencia");
  telecareService.addCharacteristic(sensorDataCharacteristic);
  BLE.addService(telecareService);
  BLE.setAdvertisedService(telecareService);
  sensorDataCharacteristic.setEventHandler(BLEWritten, onReceive);
  delay(500);
  BLE.advertise();
  Serial.println("BLE ready");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  printWifiStatus();

  delay(1000);
  sim900.println("AT");
  delay(100);
  sim900.println("AT+CMGF=1");
  delay(100);
  restartSIM900();
}

void loop() {
  BLE.poll();
}

void onReceive(BLEDevice central, BLECharacteristic characteristic) {
  // BLE values are byte arrays, not guaranteed to end with a null character.
  String data = "";
  for (int i = 0; i < characteristic.valueLength(); i++) {
    data += (char)characteristic.value()[i];
  }

  if (data.indexOf('{') < 0 || data.indexOf('}') < 0) return;
  Serial.print("Received: ");
  Serial.println(data);

  int hr = getValue(data, "\"hr\":").toInt();
  int steps = getValue(data, "\"steps\":").toInt();
  String gps = getValue(data, "\"gps\":\"");
  int compass = getValue(data, "\"compass\":").toInt();

  int comma = gps.indexOf(',');
  if (comma < 0) {
    Serial.println("Invalid GPS field");
    return;
  }
  float lat = gps.substring(0, comma).toFloat();
  float lon = gps.substring(comma + 1).toFloat();

  Serial.print("Heart rate: "); Serial.println(hr);
  Serial.print("Steps: "); Serial.println(steps);
  Serial.print("GPS: "); Serial.println(String(lat, 5) + ", " + String(lon, 5));
  Serial.print("Compass: "); Serial.println(compass);

  sendToThingSpeak(hr, steps, lat, lon, compass);

  // Demonstration thresholds; not clinically validated.
  if (hr > 120 || steps > 8000) {
    sendSMS("Alert: high heart rate or steps. HR: " + String(hr) +
            ", steps: " + String(steps) + ".");
  }
}

void sendToThingSpeak(int hr, int steps, float lat, float lon, int compass) {
  if (client.connect(server, 80)) {
    String request = String("GET /update?api_key=") + apiKey +
                     "&field1=" + hr +
                     "&field2=" + steps +
                     "&field3=" + String(lat, 5) +
                     "&field4=" + String(lon, 5) +
                     "&field5=" + compass +
                     " HTTP/1.1\r\nHost: " + server +
                     "\r\nConnection: close\r\n\r\n";
    client.print(request);
    Serial.println("ThingSpeak request sent");
    delay(1500);
    client.stop();
  } else {
    Serial.println("Could not connect to ThingSpeak");
  }
}

void sendSMS(String message) {
  sim900.println("AT+CMGS=\"" + String(alertPhone) + "\"");
  delay(100);
  sim900.println(message);
  sim900.write(26);
  delay(5000);

  while (sim900.available()) {
    Serial.write(sim900.read());
  }
  Serial.println("SMS command sent");
}

String getValue(String data, String key) {
  int start = data.indexOf(key);
  if (start < 0) return "";
  start += key.length();
  if (start >= (int)data.length()) return "";

  // GPS is quoted and contains a comma; read up to the closing quote.
  if (key.endsWith("\"")) {
    int end = data.indexOf('"', start);
    return (end < 0) ? "" : data.substring(start, end);
  }

  int comma = data.indexOf(',', start);
  int brace = data.indexOf('}', start);
  int end = comma;
  if (end < 0 || (brace >= 0 && brace < end)) end = brace;
  return (end < 0) ? "" : data.substring(start, end);
}

void printWifiStatus() {
  Serial.print("WiFi SSID: "); Serial.println(WiFi.SSID());
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
}

void restartSIM900() {
  Serial.println("Restarting SIM900...");
  sim900.println("AT");
  delay(500);
  sim900.println("AT+CFUN=1,1");
  delay(5000);
  sim900.println("AT+CMGF=1");
  delay(1000);
  Serial.println("SIM900 restart sequence complete");
}
