#include <WiFi.h>
#include <WiFiClientSecure.h>
#define MQTT_MAX_PACKET_SIZE 1024
#include <PubSubClient.h>
#include <DHT.h>
#include <time.h>

// ===================== WIFI CONFIG =====================
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ===================== AWS IOT CONFIG =====================
const char* AWS_ENDPOINT = "YOUR_AWS_ENDPOINT.iot.us-east-1.amazonaws.com"; 
const int AWS_PORT = 8883;
const char* THING_NAME = "ESP32_Hamacheshir"; 

// Topics
String TOPIC_TELEMETRY_PLANT1 = String("iot/irrigation/") + THING_NAME + "/telemetry/plant1";
String TOPIC_TELEMETRY_PLANT2 = String("iot/irrigation/") + THING_NAME + "/telemetry/plant2";
String TOPIC_SHADOW_UPDATE = String("$aws/things/") + THING_NAME + "/shadow/update";

// ===================== CERTS =====================

// Amazon Root CA 1
static const char AWS_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
PASTE_YOUR_AMAZON_ROOT_CA1_HERE
-----END CERTIFICATE-----
)EOF";

// Device Certificate
static const char DEVICE_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
PASTE_YOUR_DEVICE_CERTIFICATE_HERE
-----END CERTIFICATE-----
)EOF";

// Device Private Key
static const char PRIVATE_KEY[] PROGMEM = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
PASTE_YOUR_PRIVATE_KEY_HERE
-----END RSA PRIVATE KEY-----
)EOF";

// ===================== PINS =====================
#define DHTPIN 27
#define DHTTYPE DHT22
#define SOIL1_ADC_PIN 35 
#define SOIL2_ADC_PIN 34 
#define RED_LED_PIN 16

DHT dht(DHTPIN, DHTTYPE);

// ===================== TIMING =====================
static const uint32_t PERIOD_MS = 30000; 
static const uint32_t WIFI_STATUS_MS = 10000; 
static const uint32_t BLINK_PERIOD_MS = 500;

// ===================== SOIL SAMPLING =====================
static const int SOIL_SAMPLES_PER_READ = 300;
static const int SOIL_LOOP_EVERY_N = 25;

uint32_t lastMeasureMs = 0;
uint32_t lastWiFiPrintMs = 0;
uint32_t lastBlinkMs = 0;
bool ledBlinkState = false;

// ===================== SOIL MAPPING =====================
static const int RAW_AT_100 = 995;
static const int RAW_AT_0 = 2600;
static const int THIRSTY_MAX_PCT = 10;
static const int ON_MAX_PCT = 30;

enum MoistureState { OK_STATE, ON_STATE, THIRSTY_STATE };
MoistureState state = OK_STATE;

// ===================== NETWORK OBJECTS =====================
WiFiClientSecure net;
PubSubClient mqtt(net);

// ===================== COST LIMIT =====================
static const int MAX_MESSAGES = 1000000;
int sentCount = 0;
bool publishingEnabled = true;

// ===================== STAGGERED PUBLISH =====================
static const uint32_t PLANT2_DELAY_MS = 5000;
bool plant2Pending = false;
uint32_t plant2DueMs = 0;
float plant2_tSend = -999.0f;
float plant2_hSend = -999.0f;
int plant2_soilRaw = 0;
int plant2_wateringPct = 0;
MoistureState plant2_state = OK_STATE;

// --------------------- Helpers ---------------------

static int clampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int soilPercent5(int raw) {
    int r = clampInt(raw, RAW_AT_100, RAW_AT_0);
    float pct = (float)(RAW_AT_0 - r) * 100.0f / (float)(RAW_AT_0 - RAW_AT_100);
    int pctInt = (int)(pct + 0.5f);
    int pct5 = (int)((pctInt + 2) / 5) * 5;
    return clampInt(pct5, 0, 100);
}

int readSoilAveraged(int adcPin, int samples = SOIL_SAMPLES_PER_READ) {
    int32_t sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(adcPin);
        // Keep MQTT alive during long reads
        if ((i % SOIL_LOOP_EVERY_N) == 0) {
            if (mqtt.connected()) mqtt.loop();
            yield(); 
        }
    }
    return (int)(sum / samples);
}

void setLedSolid(bool on) {
    digitalWrite(RED_LED_PIN, on ? HIGH : LOW);
}

void printWiFiStatus() {
    Serial.print("WiFi status: ");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("CONNECTED, IP=");
        Serial.print(WiFi.localIP());
        Serial.print(" RSSI=");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        Serial.println("NOT CONNECTED");
    }
}

bool testDNS() {
    IPAddress ip;
    bool ok = WiFi.hostByName("google.com", ip);
    if (ok) {
        Serial.print("DNS OK, google.com = ");
        Serial.println(ip);
    } else {
        Serial.println("DNS FAILED");
    }
    return ok;
}

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting WiFi");
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 20000) {
        Serial.print(".");
        delay(400);
    }
    Serial.println();
    printWiFiStatus();
    if (WiFi.status() == WL_CONNECTED) {
        testDNS();
    }
}

bool syncTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Syncing time");
    time_t now = time(nullptr);
    uint32_t t0 = millis();
    while (now < 1700000000 && (millis() - t0) < 20000) {
        Serial.print(".");
        delay(500);
        now = time(nullptr);
    }
    Serial.println();
    if (now < 1700000000) {
        Serial.println("Time sync FAILED (TLS may fail)");
        return false;
    }
    Serial.print("Time OK (UTC): ");
    Serial.println(ctime(&now));
    return true;
}

void setupTLS() {
    net.setCACert(AWS_ROOT_CA);
    net.setCertificate(DEVICE_CERT);
    net.setPrivateKey(PRIVATE_KEY);
    net.setHandshakeTimeout(60);
}

void mqttReconnect() {
    mqtt.setServer(AWS_ENDPOINT, AWS_PORT);
    while (!mqtt.connected()) {
        Serial.print("Connecting MQTT to AWS (TLS)... ");
        if (!net.connected()) {
            Serial.print("net.connect... ");
            if (!net.connect(AWS_ENDPOINT, AWS_PORT)) {
                Serial.println("FAILED");
                delay(5000);
                continue;
            }
            Serial.print("OK, ");
        }
        if (mqtt.connect(THING_NAME)) {
            Serial.println("MQTT OK");
            return;
        }
        Serial.print("MQTT FAILED rc=");
        Serial.print(mqtt.state());
        Serial.println(" retry in 5s");
        net.stop();
        delay(5000);
    }
}

String isoTimestampUTCPlus2() {
    time_t now = time(nullptr);
    if (now < 1700000000) return String("unknown");
    now += 2 * 3600; 
    struct tm tmLocal;
    gmtime_r(&now, &tmLocal);
    char buf[32]; 
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+02:00", &tmLocal);
    return String(buf);
}

uint64_t epochMsUTC() {
    time_t now = time(nullptr);
    if (now < 1700000000) return 0;
    return (uint64_t)now * 1000ULL;
}

const char* stateToStr(MoistureState s) {
    if (s == OK_STATE) return "OK";
    if (s == ON_STATE) return "NEEDS_WATER";
    return "THIRSTY";
}

void publishPlantTelemetry(const char* topic, const char* plantId, float tC, float hRH, int soilRaw, int wateringPct, MoistureState st) {
    bool thirsty = (wateringPct <= THIRSTY_MAX_PCT);
    String ts = isoTimestampUTCPlus2();
    uint64_t ms = epochMsUTC();
    char tele[512];
    snprintf(tele, sizeof(tele), "{\"thing\":\"%s\",\"plantId\":\"%s\",\"tempC\":%.1f,\"humRH\":%.1f,"
                                 "\"soilRaw\":%d,\"wateringPct\":%d,\"thirsty\":%s,\"state\":\"%s\","
                                 "\"timestamp\":\"%s\",\"epochMs\":%llu}",
             THING_NAME, plantId, tC, hRH, soilRaw, wateringPct, thirsty ? "true" : "false",
             stateToStr(st), ts.c_str(), (unsigned long long)ms);

    // Reconnect if connection dropped before publish
    if (!mqtt.connected()) {
        Serial.println("MQTT not connected before publish, reconnecting...");
        mqttReconnect();
    }
    
    bool ok = mqtt.publish(topic, tele, false);
    Serial.print("MQTT publish to ");
    Serial.print(topic);
    Serial.print(" => ");
    Serial.println(ok ? "OK" : "FAILED");
    
    if (!ok) {
        Serial.print("mqtt.connected=");
        Serial.print(mqtt.connected() ? "true" : "false");
        Serial.print(" mqtt.state=");
        Serial.println(mqtt.state());
        Serial.print("payload_len=");
        Serial.println(strlen(tele));
    }
}

// Shadow uses Plant 1 data only
void publishShadowFromPlant1(float tC, float hRH, int soilRaw1, int wateringPct1, MoistureState st1) {
    bool thirsty = (wateringPct1 <= THIRSTY_MAX_PCT);
    String ts = isoTimestampUTCPlus2();
    uint64_t ms = epochMsUTC();
    char sh[560];
    snprintf(sh, sizeof(sh), "{\"state\":{\"reported\":{\"tempC\":%.1f,\"humRH\":%.1f,"
                             "\"soilRaw\":%d,\"wateringPct\":%d,\"thirsty\":%s,\"state\":\"%s\","
                             "\"timestamp\":\"%s\",\"epochMs\":%llu}}}",
             tC, hRH, soilRaw1, wateringPct1, thirsty ? "true" : "false", stateToStr(st1), ts.c_str(), (unsigned long long)ms);
             
    mqtt.publish(TOPIC_SHADOW_UPDATE.c_str(), sh, false);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    dht.begin();
    pinMode(RED_LED_PIN, OUTPUT);
    setLedSolid(false);

    Serial.println("Starting AWS IoT stage...");
    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) syncTime();
    
    setupTLS();
    mqtt.setServer(AWS_ENDPOINT, AWS_PORT);
    mqtt.setBufferSize(MQTT_MAX_PACKET_SIZE); // Increase buffer for large JSON
    mqtt.setKeepAlive(60); 

    lastMeasureMs = millis() - PERIOD_MS;
    lastWiFiPrintMs = millis() - WIFI_STATUS_MS;
}

void loop() {
    uint32_t now = millis();

    if (now - lastWiFiPrintMs >= WIFI_STATUS_MS) {
        lastWiFiPrintMs = now;
        printWiFiStatus();
    }

    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
        if (WiFi.status() == WL_CONNECTED) syncTime();
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (!mqtt.connected()) mqttReconnect();
        mqtt.loop();
    }

    // Publish delayed Plant 2 data
    if (plant2Pending && WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        if ((int32_t)(now - plant2DueMs) >= 0) {
            publishPlantTelemetry(
                TOPIC_TELEMETRY_PLANT2.c_str(),
                "plant2",
                plant2_tSend,
                plant2_hSend,
                plant2_soilRaw,
                plant2_wateringPct,
                plant2_state
            );
            plant2Pending = false;
        }
    }

    if (now - lastMeasureMs >= PERIOD_MS) {
        lastMeasureMs = now;
        
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        int soilRaw1 = readSoilAveraged(SOIL1_ADC_PIN, SOIL_SAMPLES_PER_READ);
        int soilRaw2 = readSoilAveraged(SOIL2_ADC_PIN, SOIL_SAMPLES_PER_READ);
        int wateringPct1 = soilPercent5(soilRaw1);
        int wateringPct2 = soilPercent5(soilRaw2);

        // State logic (Plant 1 only)
        if (wateringPct1 <= THIRSTY_MAX_PCT) state = THIRSTY_STATE;
        else if (wateringPct1 <= ON_MAX_PCT) state = ON_STATE;
        else state = OK_STATE;

        Serial.println("-----");
        Serial.print("Temp (C): "); Serial.println(isnan(t) ? -999 : t, 1);
        Serial.print("Humidity (%): "); Serial.println(isnan(h) ? -999 : h, 1);
        Serial.print("Soil1 raw ADC: "); Serial.println(soilRaw1);
        Serial.print("Soil1 wateringPct: "); Serial.print(wateringPct1); Serial.println("%");
        Serial.print("Soil2 raw ADC: "); Serial.println(soilRaw2);
        Serial.print("Soil2 wateringPct: "); Serial.print(wateringPct2); Serial.println("%");
        Serial.print("State: ");
        if (state == OK_STATE) Serial.println("OK");
        else if (state == THIRSTY_STATE) Serial.println("THIRSTY");
        else Serial.println("NEEDS WATER");
        Serial.println("-----");

        if (publishingEnabled && WiFi.status() == WL_CONNECTED && mqtt.connected()) {
            float tSend = isnan(t) ? -999.0f : t;
            float hSend = isnan(h) ? -999.0f : h;

            // Publish Plant 1 immediately
            publishPlantTelemetry(
                TOPIC_TELEMETRY_PLANT1.c_str(),
                "plant1",
                tSend,
                hSend,
                soilRaw1,
                wateringPct1,
                state
            );

            // Schedule Plant 2
            plant2_tSend = tSend;
            plant2_hSend = hSend;
            plant2_soilRaw = soilRaw2;
            plant2_wateringPct = wateringPct2;
            plant2_state = state;
            plant2DueMs = millis() + PLANT2_DELAY_MS;
            plant2Pending = true;
            Serial.println("Plant2 telemetry scheduled +5s");

            publishShadowFromPlant1(tSend, hSend, soilRaw1, wateringPct1, state);

            // Only increment counter if actually connected
            if (mqtt.connected()) {
                sentCount += 2;
            }
            Serial.print("Published to AWS. Count=");
            Serial.println(sentCount);

            if (sentCount >= MAX_MESSAGES) {
                publishingEnabled = false;
                Serial.println("Max messages reached. Stopping publish.");
            }
        } else {
            Serial.println("Not publishing (Disabled or Disconnected).");
        }
    }

    // LED Logic
    if (state == OK_STATE) {
        setLedSolid(false);
    } else if (state == ON_STATE) {
        setLedSolid(true);
    } else {
        if (now - lastBlinkMs >= BLINK_PERIOD_MS) {
            lastBlinkMs = now;
            ledBlinkState = !ledBlinkState;
            digitalWrite(RED_LED_PIN, ledBlinkState ? HIGH : LOW);
        }
    }
}