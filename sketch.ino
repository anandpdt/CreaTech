#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>

// ================= WIFI =================
const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* firebaseURL =
"https://createch-66f04-default-rtdb.firebaseio.com/gridNode.json";

// ================= PINS =================
#define INPUT_CURRENT_PIN 34
#define OUTPUT_CURRENT_PIN 35
#define VOLTAGE_PIN 32
#define LDR_PIN 33

#define DHT_PIN 4
#define DHT_TYPE DHT22
#define THEFT_SWITCH 27

#define RED_LED 25
#define GREEN_LED 26
#define BUZZER 14
#define RELAY 12

// ================= DISPLAY =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
LiquidCrystal_I2C lcd(0x27, 16, 2);

DHT dht(DHT_PIN, DHT_TYPE);

// ================= VARIABLES =================
float inputCurrent, outputCurrent, voltageValue;
float lossPercent, temperature;
int ldrValue;

bool theftDetected;
bool faultDetected;

unsigned long lastUpload = 0;
unsigned long lastDisplayUpdate = 0;

// ================= WIFI =================
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
}

// ================= FIREBASE =================
void sendToFirebase() {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
    http.begin(firebaseURL);
    http.addHeader("Content-Type", "application/json");

    String statusText = faultDetected ? "FAULT" : "NORMAL";

    String jsonData = "{";
    jsonData += "\"inputCurrent\":" + String(inputCurrent,2) + ",";
    jsonData += "\"outputCurrent\":" + String(outputCurrent,2) + ",";
    jsonData += "\"lossPercent\":" + String(lossPercent,2) + ",";
    jsonData += "\"temperature\":" + String(temperature,2) + ",";
    jsonData += "\"voltage\":" + String(voltageValue,2) + ",";
    jsonData += "\"ldr\":" + String(ldrValue) + ",";
    jsonData += "\"theft\":" + String(theftDetected ? "true" : "false") + ",";
    jsonData += "\"status\":\"" + statusText + "\"";
    jsonData += "}";

    int response = http.PUT(jsonData);

    Serial.print("Firebase Response: ");
    Serial.println(response);

    http.end();
  }
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  pinMode(THEFT_SWITCH, INPUT_PULLUP);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RELAY, OUTPUT);

  digitalWrite(RELAY, HIGH);

  dht.begin();
  Wire.begin(21, 22);

  // OLED Init
  if(!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while(true);
  }
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  // LCD Init
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Smart Grid Node");
  delay(2000);
  lcd.clear();

  connectWiFi();
}

// ================= LOOP =================
void loop() {

  // Reconnect WiFi if needed
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // ===== READ SENSORS =====
  inputCurrent  = (analogRead(INPUT_CURRENT_PIN)  / 4095.0) * 10.0;
  outputCurrent = (analogRead(OUTPUT_CURRENT_PIN) / 4095.0) * 10.0;
  voltageValue  = (analogRead(VOLTAGE_PIN)        / 4095.0) * 230.0;
  ldrValue      = analogRead(LDR_PIN);

  if (inputCurrent > 0.1)
    lossPercent = ((inputCurrent - outputCurrent) / inputCurrent) * 100.0;
  else
    lossPercent = 0;

  temperature = dht.readTemperature();
  if (isnan(temperature)) temperature = 0;

  theftDetected = (digitalRead(THEFT_SWITCH) == LOW);

  // ===== FAULT LOGIC =====
  faultDetected = false;
  if (lossPercent > 5.0) faultDetected = true;
  if (temperature > 60.0) faultDetected = true;
  if (theftDetected) faultDetected = true;

  // ===== OUTPUT CONTROL =====
  digitalWrite(RED_LED, faultDetected);
  digitalWrite(GREEN_LED, !faultDetected);
  digitalWrite(BUZZER, faultDetected);
  digitalWrite(RELAY, faultDetected ? LOW : HIGH);

  // ===== SERIAL PRINT =====
  Serial.println("========== SMART GRID NODE ==========");
  Serial.print("Input Current: "); Serial.print(inputCurrent); Serial.println(" A");
  Serial.print("Output Current: "); Serial.print(outputCurrent); Serial.println(" A");
  Serial.print("Voltage: "); Serial.print(voltageValue); Serial.println(" V");
  Serial.print("Loss %: "); Serial.println(lossPercent);
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println(" C");
  Serial.print("LDR Value: "); Serial.println(ldrValue);
  Serial.print("Theft: "); Serial.println(theftDetected ? "YES" : "NO");
  Serial.print("Status: "); Serial.println(faultDetected ? "FAULT" : "NORMAL");
  Serial.println("=====================================\n");

  // ===== DISPLAY UPDATE =====
  if (millis() - lastDisplayUpdate > 1500) {
    lastDisplayUpdate = millis();

    oled.clearDisplay();
    oled.setCursor(0,0);
    oled.println("ZONE A NODE");
    oled.print("Loss: "); oled.print(lossPercent,1); oled.println("%");
    oled.print("Temp: "); oled.print(temperature,1); oled.println("C");
    oled.print("Volt: "); oled.print(voltageValue,1); oled.println("V");
    oled.print("Theft: "); oled.println(theftDetected ? "YES" : "NO");
    oled.display();

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(faultDetected ? "!!! FAULT !!!" : "SYSTEM HEALTHY");
    lcd.setCursor(0,1);
    lcd.print(theftDetected ? "THEFT ALERT" : "GRID NORMAL");
  }

  // ===== FIREBASE UPDATE =====
  if (millis() - lastUpload > 5000) {
    lastUpload = millis();
    sendToFirebase();
  }

  delay(500);
}
