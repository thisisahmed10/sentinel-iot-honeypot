#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WebSocketsClient.h>

const char *SSID = "ahmed";
const char *PASSWORD = "ahmed1234";

String backendServer = "http://10.149.160.145:5000/log";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WebSocketsClient backendWs;
LiquidCrystal_I2C lcd(0x27, 16, 2);

String currentAlert = "";
bool alertVisible = false;
unsigned long alertUntilMs = 0;
const int ledPin = 2; // onboard LED for connection indicator

void connectBackendWebSocket();

String truncateForLcd(const String &value, size_t maxLength) {
  return value.length() <= maxLength ? value : value.substring(0, maxLength);
}

bool parseBackendUrl(const String &url, String &host, uint16_t &port, String &path) {
  String cleaned = url;
  cleaned.replace("http://", "");
  cleaned.replace("https://", "");

  int slashIdx = cleaned.indexOf('/');
  path = slashIdx >= 0 ? cleaned.substring(slashIdx) : "/";
  String hostPort = slashIdx >= 0 ? cleaned.substring(0, slashIdx) : cleaned;

  int colonIdx = hostPort.indexOf(':');
  if (colonIdx >= 0) {
    host = hostPort.substring(0, colonIdx);
    port = hostPort.substring(colonIdx + 1).toInt();
  } else {
    host = hostPort;
    port = 80;
  }

  return host.length() > 0;
}

void showLcdIdle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
}

void showLcdAlert(const String &msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ALERT!");
  lcd.setCursor(0, 1);
  lcd.print(truncateForLcd(msg, 16));
}

void handleAttackEvent(JsonDocument &doc) {
  String srcIp = doc["src_ip"] | "";
  String dstPort = String((uint16_t)(doc["dst_port"] | 0));
  String payload = doc["payload"] | "";

  currentAlert = srcIp + ":" + dstPort;
  if (payload.length() > 0) {
    currentAlert += " " + truncateForLcd(payload, 4);
  }

  alertVisible = true;
  alertUntilMs = millis() + 5000;
  showLcdAlert(currentAlert);

  // Broadcast to web clients
  JsonDocument alertJson;
  alertJson["alert"] = currentAlert;
  String alertStr;
  serializeJson(alertJson, alertStr);
  ws.textAll(alertStr);
}

void connectBackendWebSocket() {
  String host;
  uint16_t port = 0;
  String path;

  if (!parseBackendUrl(backendServer, host, port, path)) {
    Serial.println("Invalid backend URL");
    return;
  }

  Serial.printf("Connecting WebSocket to %s:%d\n", host.c_str(), port);
  backendWs.begin(host.c_str(), port, "/ws");
  backendWs.onEvent([](WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_CONNECTED) {
      Serial.println("Backend WS connected");
      digitalWrite(ledPin, HIGH);
      return;
    }
    if (type == WStype_DISCONNECTED) {
      Serial.println("Backend WS disconnected");
      digitalWrite(ledPin, LOW);
      return;
    }
    if (type == WStype_TEXT) {
      // Forward raw payload to connected web clients
      if (length > 0 && payload != nullptr) {
        // Try parse for LCD update, but always forward the raw JSON
        DynamicJsonDocument doc(2048);
        DeserializationError err = deserializeJson(doc, (const char *)payload, length);
        if (!err) {
          if (doc.containsKey("src_ip")) {
            handleAttackEvent(doc);
          }
        }
        // Broadcast raw text to browser clients
        ws.textAll(reinterpret_cast<const char *>(payload), length);
      }
    }
  });
  backendWs.setReconnectInterval(5000);
}

String processor(const String &var) {
  if (var == "BACKEND_URL") return backendServer;
  if (var == "IP_ADDRESS_AP") return WiFi.softAPIP().toString();
  if (var == "IP_ADDRESS_STA") return WiFi.localIP().toString();
  return String();
}

void loadConfig() {
  if (LittleFS.exists("/config.json")) {
    File file = LittleFS.open("/config.json", "r");
    JsonDocument doc;
    if (deserializeJson(doc, file) == DeserializationError::Ok) {
      if (doc["backend"].is<String>()) {
        backendServer = doc["backend"].as<String>();
        Serial.println("Config loaded: " + backendServer);
      }
    }
    file.close();
  }
}

void saveConfig(String newUrl) {
  File file = LittleFS.open("/config.json", "w");
  JsonDocument doc;
  doc["backend"] = newUrl;
  serializeJson(doc, file);
  file.close();
  backendServer = newUrl;
  backendWs.disconnect();
  connectBackendWebSocket();
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    JsonDocument doc;
    doc["status"] = "connected";
    doc["alert"] = currentAlert;
    String json;
    serializeJson(doc, json);
    client->text(json);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Failed");
    return;
  }
  loadConfig();

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  lcd.setCursor(0, 1);
  lcd.print("Connecting...");

  unsigned long timeout = millis() + 20000;
  while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    Serial.println(WiFi.localIP());
    showLcdIdle();
    connectBackendWebSocket();
  } else {
    Serial.println("WiFi Failed!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
    delay(3000);
  }

  // Setup WebSocket
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Simple API to toggle DDoS shield; forwards command to backend WS
  server.on("/api/ddos", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("enabled")) {
      request->send(400, "text/plain", "missing enabled param");
      return;
    }
    String val = request->getParam("enabled")->value();
    bool enabled = (val == "1" || val.equalsIgnoreCase("true"));
    DynamicJsonDocument doc(256);
    doc["type"] = "control";
    doc["control"] = "ddos";
    doc["enabled"] = enabled;
    String out;
    serializeJson(doc, out);
    if (backendWs.isConnected()) {
      backendWs.sendTXT(out);
      request->send(200, "application/json", "{\"status\":\"sent\"}");
    } else {
      request->send(503, "application/json", "{\"status\":\"backend_unavailable\"}");
    }
  });

  // Admin endpoint
  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("url")) {
      saveConfig(request->getParam("url")->value());
      request->send(200, "text/plain", "Updated");
    } else if (LittleFS.exists("/admin.html")) {
      request->send(LittleFS, "/admin.html", String(), false, processor);
    } else {
      request->send(404);
    }
  });

  // Serve index through processor so placeholders are replaced
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", String(), false, processor);
    } else {
      request->send(404, "text/plain", "index.html not found");
    }
  });

  // Static files (other assets)
  server.serveStatic("/", LittleFS, "/");

  // Fallback to index (also processed)
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  server.begin();
}

void loop() {
  backendWs.loop();
  ws.cleanupClients();

  if (alertVisible && millis() > alertUntilMs) {
    alertVisible = false;
    currentAlert = "";
    showLcdIdle();
  }
}
