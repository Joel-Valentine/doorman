#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include "secrets.h"

// -------------------------
//  Hall sensor
// -------------------------
const int hallPin = 34;  // Analog input pin (AH49E)
const int LOCKED_THRESHOLD   = 2000;
const int UNLOCKED_THRESHOLD = 1900;
bool locked = false;

// -------------------------
//  Logging system
// -------------------------
WebServer server(80);
String logBuffer = "";

// ---------- logging helper ----------
void logMsg(const String& msg) {
  Serial.println(msg);
  logBuffer += msg + "\n";
  if (logBuffer.length() > 4000) {
    // trim to keep RAM in check
    int cut = logBuffer.indexOf('\n', 1500);
    if (cut > 0) logBuffer.remove(0, cut + 1);
  }
}

// ---------- HTTP handlers ----------
void handleRoot() {
  // Simple auto-refreshing log UI
  static const char* page = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<title>Door Lock Logs</title>
<style>
  body{font-family:ui-monospace,monospace;margin:0;background:#111;color:#eee}
  header{padding:10px 14px;background:#222;position:sticky;top:0}
  pre{white-space:pre-wrap;word-wrap:break-word;padding:14px;margin:0}
  small{opacity:.7}
</style></head><body>
<header>Front Door Monitor <small>(auto updates bing bong)</small></header>
<pre id="out">Loading…</pre>
<script>
async function pull(){
  try{
    const r = await fetch('/logs', {cache:'no-store'});
    const t = await r.text();
    const el = document.getElementById('out');
    const atBottom = (window.innerHeight + window.scrollY) >= (document.body.offsetHeight - 4);
    el.textContent = t;
    if(atBottom) window.scrollTo(0, document.body.scrollHeight);
  }catch(e){}
}
pull(); setInterval(pull, 1000);
</script></body></html>
)HTML";
  server.send(200, "text/html; charset=utf-8", page);
}

void handleLogs() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain; charset=utf-8", logBuffer);
}

void handleFavicon() {
  server.send(204); // no content
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found. Try /\n");
}

// ---------- setup helpers ----------
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
    ESP.restart();
  }
  logMsg("\n✅ Wi-Fi connected! IP: " + WiFi.localIP().toString());
}

void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_HOSTNAME);
  ArduinoOTA
    .onStart([]() { logMsg("🔄 OTA update start..."); })
    .onEnd([]() { logMsg("✅ OTA update complete!"); })
    .onProgress([](unsigned int progress, unsigned int total) {
      char buf[32];
      snprintf(buf, sizeof(buf), "Progress: %u%%", progress / (total / 100));
      logMsg(buf);
    })
    .onError([](ota_error_t error) {
      logMsg("❌ OTA Error: " + String(error));
    });
  ArduinoOTA.begin();
  logMsg("🚀 OTA Ready");
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/logs", handleLogs);
  server.on("/favicon.ico", handleFavicon);
  server.onNotFound(handleNotFound);
  server.begin();
  logMsg("🌐 Web log UI at /  (live logs at /logs)");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Booting...");

  setupWiFi();
  setupOTA();
  setupWebServer();

  logMsg("Setup complete.");
  logMsg("Front door lock monitor is running as: " + String(DEVICE_HOSTNAME));
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  int value = analogRead(hallPin);

  if (!locked && value > LOCKED_THRESHOLD) {
    locked = true;
    logMsg("🔒 LOCKED (" + String(value) + ")");
  } 
  else if (locked && value < UNLOCKED_THRESHOLD) {
    locked = false;
    logMsg("🔓 UNLOCKED (" + String(value) + ")");
  }

  delay(100);
}
