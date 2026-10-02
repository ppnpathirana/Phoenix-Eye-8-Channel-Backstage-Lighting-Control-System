#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================
// WIFI
// ============================================================



// ============================================================
// STATIC IP
// ============================================================

IPAddress local_IP(192, 168, 8, 132);
IPAddress gateway(192, 168, 8, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 8, 1);
IPAddress secondaryDNS(8, 8, 8, 8);

// ============================================================
// WEB SERVER
// ============================================================

WebServer server(80);

// ============================================================
// RELAY PINS
// ============================================================

constexpr uint8_t RELAY_COUNT = 8;
const uint8_t RELAY_PINS[RELAY_COUNT] = {25, 26, 27, 32, 33, 18, 19, 23};
bool relayStates[RELAY_COUNT] = {};
bool directStates[RELAY_COUNT] = {};

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

// ============================================================
// MODES
// ============================================================

enum LightMode {
  MODE_OFF,
  MODE_NORMAL,
  MODE_FAST_BLINK,
  MODE_SLOW_BLINK,
  MODE_RANDOM_BLINK,
  MODE_WINK_LEFT,
  MODE_WINK_RIGHT,
  MODE_ALTERNATE,
  MODE_DOUBLE_BLINK,
  MODE_HEARTBEAT,
  MODE_SCANNING,
  MODE_ANGRY,
  MODE_FIRE,
  MODE_WAKEUP,
  MODE_SLEEP,
  MODE_SHOW,
  MODE_RELAY_DIRECT   // <<< NEW: direct relay override
};

LightMode currentMode    = MODE_OFF;
LightMode previousMode   = MODE_OFF;  // restore after direct relay

// ============================================================
// DIRECT RELAY STATE (used in MODE_RELAY_DIRECT)
// ============================================================



// ============================================================
// TIMERS / STATE
// ============================================================

unsigned long lastAction  = 0;
unsigned long randomNext  = 0;
bool          lightState  = false;
uint8_t       stepNumber  = 0;
uint8_t       showStep    = 0;
unsigned long showStarted = 0;

// Physical pin state tracking for /status
bool pinStateLeft  = false;
bool pinStateRight = false;

// ============================================================
// RELAY HELPERS
// ============================================================

// Odd channels form the left group; even channels form the right group.
void writeRelay(uint8_t index, bool state) {
  if (index >= RELAY_COUNT) return;
  if (relayStates[index] != state) digitalWrite(RELAY_PINS[index], state ? RELAY_ON : RELAY_OFF);
  relayStates[index] = state;
  pinStateLeft = relayStates[0];
  pinStateRight = relayStates[1];
}
inline void leftEye(bool state) {
  for (uint8_t i=0; i<RELAY_COUNT; i+=2) writeRelay(i, state);
}
inline void rightEye(bool state) {
  for (uint8_t i=1; i<RELAY_COUNT; i+=2) writeRelay(i, state);
}
inline void bothEyes(bool state) { leftEye(state); rightEye(state); }
inline void allOff() { bothEyes(false); }

// ============================================================
// MODE CHANGE
// ============================================================

void setMode(LightMode newMode) {
  previousMode  = currentMode;
  currentMode   = newMode;
  lastAction    = millis();
  randomNext    = lastAction;
  stepNumber    = 0;
  lightState    = false;
  showStep      = 0;
  showStarted   = lastAction;
  if (newMode != MODE_RELAY_DIRECT) allOff();

  if (newMode == MODE_NORMAL)         bothEyes(true);
  else if (newMode == MODE_FIRE)      randomNext = millis() + random(20, 150);
  else if (newMode == MODE_RANDOM_BLINK) randomNext = millis() + random(300, 1500);
  else if (newMode == MODE_RELAY_DIRECT) {
    for (uint8_t i=0; i<RELAY_COUNT; ++i) writeRelay(i, directStates[i]);
  }
}

// ============================================================
// EFFECT RUNNERS
// ============================================================

void runNormal() { bothEyes(true); }

void runFastBlink() {
  unsigned long now = millis();
  if (now - lastAction >= 120) {
    lastAction  = now;
    lightState  = !lightState;
    bothEyes(lightState);
  }
}

void runSlowBlink() {
  unsigned long now = millis();
  if (now - lastAction >= 800) {
    lastAction = now;
    lightState = !lightState;
    bothEyes(lightState);
  }
}

void runRandomBlink() {
  unsigned long now = millis();
  if ((long)(now - randomNext) >= 0) {
    uint8_t eye = random(0, 3);
    if      (eye == 0) { leftEye(true);  rightEye(false); }
    else if (eye == 1) { leftEye(false); rightEye(true);  }
    else               { bothEyes(true); }
    randomNext = now + random(80, 700);
    if (random(0, 5) == 0) { allOff(); randomNext = now + random(80, 300); }
  }
}

void runWinkLeft() {
  unsigned long now = millis();
  rightEye(true);
  if (now - lastAction >= 180) {
    lastAction = now;
    lightState = !lightState;
    leftEye(lightState);
  }
}

void runWinkRight() {
  unsigned long now = millis();
  leftEye(true);
  if (now - lastAction >= 180) {
    lastAction = now;
    lightState = !lightState;
    rightEye(lightState);
  }
}

void runAlternate() {
  unsigned long now = millis();
  if (now - lastAction >= 220) {
    lastAction = now;
    stepNumber++;
    if (stepNumber & 1) { leftEye(false); rightEye(true);  }
    else                { leftEye(true);  rightEye(false); }
  }
}

void runDoubleBlink() {
  unsigned long now = millis();
  switch (stepNumber) {
    case 0: bothEyes(true);  if (now - lastAction >= 130) { stepNumber=1; lastAction=now; } break;
    case 1: bothEyes(false); if (now - lastAction >= 130) { stepNumber=2; lastAction=now; } break;
    case 2: bothEyes(true);  if (now - lastAction >= 130) { stepNumber=3; lastAction=now; } break;
    case 3: bothEyes(false); if (now - lastAction >= 650) { stepNumber=0; lastAction=now; } break;
  }
}

void runHeartbeat() {
  unsigned long now = millis();
  switch (stepNumber) {
    case 0: bothEyes(true);  if (now - lastAction >= 110) { stepNumber=1; lastAction=now; } break;
    case 1: bothEyes(false); if (now - lastAction >= 110) { stepNumber=2; lastAction=now; } break;
    case 2: bothEyes(true);  if (now - lastAction >= 110) { stepNumber=3; lastAction=now; } break;
    case 3: bothEyes(false); if (now - lastAction >= 650) { stepNumber=0; lastAction=now; } break;
  }
}

void runScanning() {
  unsigned long now = millis();
  if (now - lastAction >= 250) {
    lastAction = now;
    stepNumber++;
    if (stepNumber > 3) stepNumber = 0;
    switch (stepNumber) {
      case 0: allOff();                          break;
      case 1: leftEye(true);  rightEye(false);  break;
      case 2: bothEyes(true);                   break;
      case 3: leftEye(false); rightEye(true);   break;
    }
  }
}

void runAngry() {
  unsigned long now = millis();
  if (now - lastAction >= 70) {
    lastAction = now;
    lightState = !lightState;
    bothEyes(lightState);
  }
}

void runFire() {
  unsigned long now = millis();
  if ((long)(now - randomNext) >= 0) {
    switch (random(0, 6)) {
      case 0: bothEyes(true);                   break;
      case 1: leftEye(true);  rightEye(false);  break;
      case 2: leftEye(false); rightEye(true);   break;
      case 3: bothEyes(false);                  break;
      case 4: bothEyes(true);                   break;
      case 5: bothEyes(false);                  break;
    }
    randomNext = now + random(40, 250);
  }
}

void runWakeup() {
  unsigned long now = millis();
  switch (stepNumber) {
    case 0: allOff();        if (now - lastAction >= 800)  { stepNumber=1; lastAction=now; } break;
    case 1: leftEye(true);   if (now - lastAction >= 250)  { stepNumber=2; lastAction=now; } break;
    case 2: bothEyes(true);  if (now - lastAction >= 250)  { stepNumber=3; lastAction=now; } break;
    case 3: bothEyes(false); if (now - lastAction >= 120)  { stepNumber=4; lastAction=now; } break;
    case 4: bothEyes(true);  if (now - lastAction >= 1000) { stepNumber=0; lastAction=now; } break;
  }
}

void runSleep() {
  unsigned long now = millis();
  switch (stepNumber) {
    case 0: allOff();       if (now - lastAction >= 3000) { stepNumber=1; lastAction=now; } break;
    case 1: bothEyes(true); if (now - lastAction >= 100)  { stepNumber=2; lastAction=now; } break;
    case 2: bothEyes(false);if (now - lastAction >= 3000) { stepNumber=1; lastAction=now; } break;
  }
}

void runRelayDirect() {
  for (uint8_t i=0; i<RELAY_COUNT; ++i) writeRelay(i, directStates[i]);
}

void runShow() {
  unsigned long now = millis();
  if (now - showStarted >= 7000) {
    showStarted = now;
    showStep++;
    if (showStep >= 8) showStep = 0;
    stepNumber = 0;
    lastAction = now;
    allOff();
  }
  switch (showStep) {
    case 0: runNormal();      break;
    case 1: runSlowBlink();   break;
    case 2: runDoubleBlink(); break;
    case 3: runAlternate();   break;
    case 4: runHeartbeat();   break;
    case 5: runFire();        break;
    case 6: runScanning();    break;
    case 7: runAngry();       break;
  }
}

// ============================================================
// RUN CURRENT MODE
// ============================================================

void runCurrentMode() {
  switch (currentMode) {
    case MODE_OFF:          allOff();          break;
    case MODE_NORMAL:       runNormal();       break;
    case MODE_FAST_BLINK:   runFastBlink();    break;
    case MODE_SLOW_BLINK:   runSlowBlink();    break;
    case MODE_RANDOM_BLINK: runRandomBlink();  break;
    case MODE_WINK_LEFT:    runWinkLeft();     break;
    case MODE_WINK_RIGHT:   runWinkRight();    break;
    case MODE_ALTERNATE:    runAlternate();    break;
    case MODE_DOUBLE_BLINK: runDoubleBlink();  break;
    case MODE_HEARTBEAT:    runHeartbeat();    break;
    case MODE_SCANNING:     runScanning();     break;
    case MODE_ANGRY:        runAngry();        break;
    case MODE_FIRE:         runFire();         break;
    case MODE_WAKEUP:       runWakeup();       break;
    case MODE_SLEEP:        runSleep();        break;
    case MODE_SHOW:         runShow();         break;
    case MODE_RELAY_DIRECT: runRelayDirect();  break;
  }
}

// ============================================================
// MODE NAME
// ============================================================

const char* modeName() {
  switch (currentMode) {
    case MODE_OFF:          return "ALL OFF";
    case MODE_NORMAL:       return "BOTH EYES ON";
    case MODE_FAST_BLINK:   return "FAST BLINK";
    case MODE_SLOW_BLINK:   return "SLOW BLINK";
    case MODE_RANDOM_BLINK: return "RANDOM BLINK";
    case MODE_WINK_LEFT:    return "LEFT WINK";
    case MODE_WINK_RIGHT:   return "RIGHT WINK";
    case MODE_ALTERNATE:    return "ALTERNATING";
    case MODE_DOUBLE_BLINK: return "DOUBLE BLINK";
    case MODE_HEARTBEAT:    return "HEARTBEAT";
    case MODE_SCANNING:     return "SCANNING";
    case MODE_ANGRY:        return "ANGRY PHOENIX";
    case MODE_FIRE:         return "FIRE SPIRIT";
    case MODE_WAKEUP:       return "WAKE UP";
    case MODE_SLEEP:        return "SLEEP MODE";
    case MODE_SHOW:         return "AUTOMATIC SHOW";
    case MODE_RELAY_DIRECT: return "RELAY DIRECT";
  }
  return "UNKNOWN";
}

// ============================================================
// CORS HEADERS (so the HTML file can be served from disk too)
// ============================================================

void addCORS() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ============================================================
// HANDLER: /status  ->  JSON
// ============================================================

void handleStatus() {
  addCORS();
  StaticJsonDocument<768> doc;
  JsonArray states = doc.createNestedArray("relays");
  JsonArray pins = doc.createNestedArray("pins");
  for (uint8_t i=0; i<RELAY_COUNT; ++i) { states.add(relayStates[i]); pins.add(RELAY_PINS[i]); }
  doc["mode"]        = modeName();
  doc["modeId"]      = (int)currentMode;
  doc["leftEye"]     = pinStateLeft;
  doc["rightEye"]    = pinStateRight;
  doc["ip"]          = WiFi.localIP().toString();
  doc["rssi"]        = WiFi.RSSI();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ============================================================
// HANDLER: /mode?m=X
// ============================================================

void handleMode() {
  addCORS();
  if (!server.hasArg("m")) {
    server.send(400, "text/plain", "Missing mode");
    return;
  }
  String m = server.arg("m");

  if      (m == "off")       setMode(MODE_OFF);
  else if (m == "normal")    setMode(MODE_NORMAL);
  else if (m == "fast")      setMode(MODE_FAST_BLINK);
  else if (m == "slow")      setMode(MODE_SLOW_BLINK);
  else if (m == "random")    setMode(MODE_RANDOM_BLINK);
  else if (m == "leftwink")  setMode(MODE_WINK_LEFT);
  else if (m == "rightwink") setMode(MODE_WINK_RIGHT);
  else if (m == "alternate") setMode(MODE_ALTERNATE);
  else if (m == "double")    setMode(MODE_DOUBLE_BLINK);
  else if (m == "heartbeat") setMode(MODE_HEARTBEAT);
  else if (m == "scan")      setMode(MODE_SCANNING);
  else if (m == "angry")     setMode(MODE_ANGRY);
  else if (m == "fire")      setMode(MODE_FIRE);
  else if (m == "wakeup")    setMode(MODE_WAKEUP);
  else if (m == "sleep")     setMode(MODE_SLEEP);
  else if (m == "show")      setMode(MODE_SHOW);
  else {
    server.send(400, "text/plain", "Invalid mode");
    return;
  }

  server.send(200, "text/plain", modeName());
}

// ============================================================
// HANDLER: /relay?eye=left|right|both&on=1|0
// ============================================================

void handleRelay() {
  addCORS();
  if (!server.hasArg("on") || (server.arg("on") != "0" && server.arg("on") != "1")) {
    server.send(400, "text/plain", "on must be 0 or 1"); return;
  }
  String channel = server.arg("ch");
  String eye = server.arg("eye");
  int selected = -1;
  if (server.hasArg("ch")) {
    if (channel != "all") {
      if (channel.length()!=1 || channel[0]<'1' || channel[0]>'8') {
        server.send(400, "text/plain", "ch must be 1..8 or all"); return;
      }
      selected=channel[0]-'1';
    }
  } else if (eye != "left" && eye != "right" && eye != "both") {
    server.send(400, "text/plain", "Use /relay?ch=1..8|all&on=0|1"); return;
  }
  bool on=server.arg("on")=="1";
  for (uint8_t i=0; i<RELAY_COUNT; ++i) {
    directStates[i]=relayStates[i];
    bool target=server.hasArg("ch") ? (selected<0 || selected==i)
      : (eye=="both" || (eye=="left" && i%2==0) || (eye=="right" && i%2==1));
    if (target) directStates[i]=on;
  }
  setMode(MODE_RELAY_DIRECT);
  handleStatus();
}

// ============================================================
// WEB PAGE  (served from PROGMEM)
// ============================================================

// The dashboard HTML is kept in a separate file for clarity.
// Copy the contents of phoenix_eye_dashboard.html into this string,
// OR serve it from SPIFFS if you have it configured.
// For this build, the full HTML is embedded below.

const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Phoenix Eye | 8 Relays</title>
<style>body{margin:0;background:#101116;color:#eee;font:16px system-ui}main{max-width:900px;margin:auto;padding:24px}h1{color:#ff974a}section{margin:24px 0}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}.card{padding:18px;background:#1c1e27;border:1px solid #393d4e;border-radius:12px}button{padding:12px;margin:4px;border:1px solid #50576a;border-radius:8px;background:#282c39;color:white;cursor:pointer}button.active,.on{border-color:#47dd94;color:#47dd94}.off{color:#aaa}#message{min-height:24px;color:#ffbe70}small{color:#aab1c3}</style>
<main><h1>PHOENIX EYE / 8 CHANNELS</h1><p id="connection">Connecting...</p><p>Mode: <strong id="mode">Unknown</strong></p><p id="message" role="status"></p><section><h2>Individual relays</h2><div id="channels" class="grid"></div><button onclick="command('/relay?ch=all&on=1')">All ON</button><button onclick="command('/mode?m=off')">All OFF / Stop</button></section><section><h2>Light effects</h2><small>Left group: 1, 3, 5, 7 / Right group: 2, 4, 6, 8</small><div><button data-mode="off" onclick="command('/mode?m=off')">All OFF</button><button data-mode="normal" onclick="command('/mode?m=normal')">All ON</button><button data-mode="fast" onclick="command('/mode?m=fast')">Fast blink</button><button data-mode="slow" onclick="command('/mode?m=slow')">Slow blink</button><button data-mode="random" onclick="command('/mode?m=random')">Random blink</button><button data-mode="leftwink" onclick="command('/mode?m=leftwink')">Left wink</button><button data-mode="rightwink" onclick="command('/mode?m=rightwink')">Right wink</button><button data-mode="alternate" onclick="command('/mode?m=alternate')">Alternate groups</button><button data-mode="double" onclick="command('/mode?m=double')">Double blink</button><button data-mode="heartbeat" onclick="command('/mode?m=heartbeat')">Heartbeat</button><button data-mode="scan" onclick="command('/mode?m=scan')">Scan groups</button><button data-mode="angry" onclick="command('/mode?m=angry')">Angry</button><button data-mode="fire" onclick="command('/mode?m=fire')">Fire</button><button data-mode="wakeup" onclick="command('/mode?m=wakeup')">Wake up</button><button data-mode="sleep" onclick="command('/mode?m=sleep')">Sleep</button><button data-mode="show" onclick="command('/mode?m=show')">Auto show</button></div></section></main>
<script>
const pins=[25,26,27,32,33,18,19,23];
document.getElementById('channels').innerHTML=pins.map((p,i)=>`<div class="card"><strong>Relay ${i+1}</strong><p><small>GPIO ${p}</small></p><p id="state${i}">Unknown</p><button id="on${i}" onclick="command('/relay?ch=${i+1}&on=1')">ON</button><button id="off${i}" onclick="command('/relay?ch=${i+1}&on=0')">OFF</button></div>`).join('');
async function request(url){const r=await fetch(url,{cache:'no-store',signal:AbortSignal.timeout(4000)});if(!r.ok)throw Error(await r.text());return r;}
function apply(d){document.getElementById('mode').textContent=d.mode;document.getElementById('connection').textContent=`${d.ip} / Online / RSSI ${d.rssi} dBm`;d.relays.forEach((on,i)=>{let el=document.getElementById('state'+i);el.textContent=on?'ON':'OFF';el.className=on?'on':'off';document.getElementById('on'+i).classList.toggle('active',on);document.getElementById('off'+i).classList.toggle('active',!on);});const keys=['off','normal','fast','slow','random','leftwink','rightwink','alternate','double','heartbeat','scan','angry','fire','wakeup','sleep','show'];document.querySelectorAll('[data-mode]').forEach(b=>b.classList.toggle('active',b.dataset.mode===keys[d.modeId]));}
async function command(url){try{await request(url);apply(await(await request('/status')).json());document.getElementById('message').textContent='Command applied';}catch(e){document.getElementById('message').textContent='Command failed: '+e.message;}}
async function poll(){try{apply(await(await request('/status')).json());}catch(e){document.getElementById('connection').textContent='Offline / check Wi-Fi and controller';}setTimeout(poll,1000);}poll();
</script></html>
)rawliteral";

// ============================================================
// HANDLER: /  (root)
// ============================================================

void handleRoot() {
  server.send_P(200, "text/html", MAIN_PAGE);
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  // Relays
  for (uint8_t i=0; i<RELAY_COUNT; ++i) {
    digitalWrite(RELAY_PINS[i], RELAY_OFF);
    pinMode(RELAY_PINS[i], OUTPUT);
  }
  allOff();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);

  Serial.println("\n================================");
  Serial.println("🔥 PHOENIX EYE CONTROLLER");
  Serial.println("ESP32 DOIT V1  |  ZERO-BLOCKING");
  Serial.println("================================");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Connecting");

  WiFi.begin(SSID, PASSWORD);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    Serial.print(".");
    yield();
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi CONNECTED");
    Serial.print("IP   : "); Serial.println(WiFi.localIP());
    Serial.print("GW   : "); Serial.println(WiFi.gatewayIP());
    Serial.print("RSSI : "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
  } else {
    Serial.println("WiFi CONNECTION FAILED");
  }

  // Routes
  server.on("/",       HTTP_GET, handleRoot);
  server.on("/mode",   HTTP_GET, handleMode);
  server.on("/relay",  HTTP_GET, handleRelay);   // <<< NEW
  server.on("/status", HTTP_GET, handleStatus);  // <<< NEW

  server.begin();
  Serial.println("WEB SERVER STARTED");
  Serial.println("http://192.168.8.132");
  Serial.println("================================");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  server.handleClient();
  runCurrentMode();
}

