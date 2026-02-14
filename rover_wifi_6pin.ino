// ========================================
// ESP32 6x6 Rover - WiFi Web Controller
// REDUCED 6-PIN WIRING
// ========================================
#include <WiFi.h>
#include <WebServer.h>

// ----- WiFi Hotspot Credentials -----
const char* ssid     = "RoverControl";
const char* password = "rover1234";

WebServer server(80);

// PWM Settings
const int freq       = 5000;
const int resolution = 8;

// ===== 6 PINS TOTAL =====
// Left side  (TL + ML + RL wired together)
const int LEFT_ENA = 23;   // All left ENA pins shorted → GPIO 23
const int LEFT_IN1 = 13;   // All left IN1 pins shorted → GPIO 13
const int LEFT_IN2 = 12;   // All left IN2 pins shorted → GPIO 12

// Right side (TR + MR + RR wired together)
const int RIGHT_ENB = 26;  // All right ENB pins shorted → GPIO 26
const int RIGHT_IN3 = 25;  // All right IN3 pins shorted → GPIO 25
const int RIGHT_IN4 = 33;  // All right IN4 pins shorted → GPIO 33

int currentSpeed = 200;

// ===== MOTOR CONTROL =====
void leftSide(int spd, int dir) {
  if (dir == 1)       { digitalWrite(LEFT_IN1, HIGH); digitalWrite(LEFT_IN2, LOW);  ledcWrite(LEFT_ENA, spd); }
  else if (dir == -1) { digitalWrite(LEFT_IN1, LOW);  digitalWrite(LEFT_IN2, HIGH); ledcWrite(LEFT_ENA, spd); }
  else                { digitalWrite(LEFT_IN1, LOW);  digitalWrite(LEFT_IN2, LOW);  ledcWrite(LEFT_ENA, 0);   }
}

void rightSide(int spd, int dir) {
  if (dir == 1)       { digitalWrite(RIGHT_IN3, HIGH); digitalWrite(RIGHT_IN4, LOW);  ledcWrite(RIGHT_ENB, spd); }
  else if (dir == -1) { digitalWrite(RIGHT_IN3, LOW);  digitalWrite(RIGHT_IN4, HIGH); ledcWrite(RIGHT_ENB, spd); }
  else                { digitalWrite(RIGHT_IN3, LOW);  digitalWrite(RIGHT_IN4, LOW);  ledcWrite(RIGHT_ENB, 0);   }
}

void forward(int spd)   { leftSide(spd,  1); rightSide(spd,  1); }
void backward(int spd)  { leftSide(spd, -1); rightSide(spd, -1); }
void turnLeft(int spd)  { leftSide(spd, -1); rightSide(spd,  1); }
void turnRight(int spd) { leftSide(spd,  1); rightSide(spd, -1); }
void stopAll()          { leftSide(0,    0); rightSide(0,    0); }

// ===== WEB UI =====
const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no"/>
<title>ROVER CTRL</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@700;900&display=swap');
  :root {
    --bg:#0a0c10; --panel:#111418; --border:#1e2530;
    --accent:#00e5ff; --accent2:#ff3d00; --dim:#2a3040;
    --text:#c8d8e8; --glow:0 0 12px #00e5ff88;
  }
  *, *::before, *::after { box-sizing:border-box; margin:0; padding:0; }
  body {
    background:var(--bg); font-family:'Share Tech Mono',monospace; color:var(--text);
    min-height:100dvh; display:flex; flex-direction:column;
    align-items:center; justify-content:center; gap:18px; padding:20px; overflow:hidden;
  }
  body::before {
    content:''; position:fixed; inset:0;
    background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,0,0,0.18) 2px,rgba(0,0,0,0.18) 4px);
    pointer-events:none; z-index:999;
  }
  header { text-align:center; letter-spacing:.15em; }
  header h1 {
    font-family:'Orbitron',sans-serif; font-size:clamp(1.1rem,5vw,1.7rem);
    font-weight:900; color:var(--accent); text-shadow:var(--glow); text-transform:uppercase;
  }
  header p { font-size:.7rem; color:#4a6080; margin-top:2px; }
  .speed-wrap { width:100%; max-width:320px; display:flex; align-items:center; gap:10px; }
  .speed-label { font-size:.7rem; color:#4a6080; white-space:nowrap; min-width:55px; }
  .speed-track { flex:1; height:6px; background:var(--dim); border-radius:3px; overflow:hidden; }
  .speed-fill { height:100%; background:var(--accent); box-shadow:var(--glow); border-radius:3px; transition:width .2s ease; }
  .speed-val { font-size:.75rem; color:var(--accent); min-width:38px; text-align:right; }
  .dpad {
    display:grid; grid-template-columns:repeat(3,1fr); grid-template-rows:repeat(3,1fr);
    gap:8px; width:min(300px,90vw); height:min(300px,90vw);
  }
  .btn {
    background:var(--panel); border:1px solid var(--border); border-radius:10px;
    color:var(--text); font-family:'Share Tech Mono',monospace; font-size:clamp(.6rem,3vw,.85rem);
    cursor:pointer; display:flex; flex-direction:column; align-items:center; justify-content:center;
    gap:4px; transition:background .1s,border-color .1s,transform .08s,box-shadow .1s;
    user-select:none; -webkit-tap-highlight-color:transparent; touch-action:manipulation;
  }
  .btn svg { width:28px; height:28px; fill:var(--text); transition:fill .1s; }
  .btn:active, .btn.pressed {
    background:#0d1a22; border-color:var(--accent);
    box-shadow:var(--glow),inset 0 0 12px #00e5ff22; transform:scale(0.94);
  }
  .btn:active svg, .btn.pressed svg { fill:var(--accent); }
  .btn-stop { background:#1a0a08; border-color:#3a1a10; }
  .btn-stop svg { fill:var(--accent2); }
  .btn-stop:active, .btn-stop.pressed {
    background:#250d08; border-color:var(--accent2);
    box-shadow:0 0 14px #ff3d0088,inset 0 0 12px #ff3d0022;
  }
  .btn-stop:active svg, .btn-stop.pressed svg { fill:var(--accent2); }
  .speed-row { display:flex; gap:8px; width:min(300px,90vw); }
  .speed-row .btn { flex:1; height:52px; }
  .status { font-size:.7rem; color:#3a5070; letter-spacing:.1em; }
  #statusText { color:var(--accent); }
</style>
</head>
<body>
<header>
  <h1>&#9651; Rover Control</h1>
  <p>6&#215;6 ESP32 &bull; 6-PIN MODE</p>
</header>
<div class="speed-wrap">
  <span class="speed-label">SPEED</span>
  <div class="speed-track"><div class="speed-fill" id="speedFill" style="width:78%"></div></div>
  <span class="speed-val" id="speedPct">78%</span>
</div>
<div class="dpad">
  <div></div>
  <button class="btn" ontouchstart="cmd('forward')" ontouchend="cmd('stop')" onmousedown="cmd('forward')" onmouseup="cmd('stop')" onmouseleave="cmd('stop')">
    <svg viewBox="0 0 24 24"><path d="M12 4l8 8H4z"/></svg><span>FWD</span>
  </button>
  <div></div>
  <button class="btn" ontouchstart="cmd('left')" ontouchend="cmd('stop')" onmousedown="cmd('left')" onmouseup="cmd('stop')" onmouseleave="cmd('stop')">
    <svg viewBox="0 0 24 24"><path d="M4 12l8-8v16z"/></svg><span>LEFT</span>
  </button>
  <button class="btn btn-stop" onclick="cmd('stop')">
    <svg viewBox="0 0 24 24"><rect x="4" y="4" width="16" height="16" rx="2"/></svg><span>STOP</span>
  </button>
  <button class="btn" ontouchstart="cmd('right')" ontouchend="cmd('stop')" onmousedown="cmd('right')" onmouseup="cmd('stop')" onmouseleave="cmd('stop')">
    <svg viewBox="0 0 24 24"><path d="M20 12l-8 8V4z"/></svg><span>RIGHT</span>
  </button>
  <div></div>
  <button class="btn" ontouchstart="cmd('backward')" ontouchend="cmd('stop')" onmousedown="cmd('backward')" onmouseup="cmd('stop')" onmouseleave="cmd('stop')">
    <svg viewBox="0 0 24 24"><path d="M12 20l8-8H4z"/></svg><span>REV</span>
  </button>
  <div></div>
</div>
<div class="speed-row">
  <button class="btn" onclick="cmd('slower')">
    <svg viewBox="0 0 24 24"><path d="M5 13h14v-2H5z"/></svg><span>SLOWER</span>
  </button>
  <button class="btn" onclick="cmd('faster')">
    <svg viewBox="0 0 24 24"><path d="M11 5v6H5v2h6v6h2v-6h6v-2h-6V5z"/></svg><span>FASTER</span>
  </button>
</div>
<p class="status">STATUS: <span id="statusText">READY</span></p>
<script>
  const labels = {
    forward:'FWD', backward:'REV', left:'TURNING LEFT',
    right:'TURNING RIGHT', stop:'STOPPED', faster:'SPEED UP', slower:'SPEED DOWN'
  };
  function cmd(action) {
    fetch('/cmd?action=' + action).then(r => r.json()).then(data => {
      document.getElementById('statusText').textContent = labels[action] || action.toUpperCase();
      if (data.speed !== undefined) {
        const pct = Math.round((data.speed / 255) * 100);
        document.getElementById('speedFill').style.width = pct + '%';
        document.getElementById('speedPct').textContent  = pct + '%';
      }
    }).catch(() => { document.getElementById('statusText').textContent = 'ERR'; });
  }
  const keyMap = {
    'w':'forward','ArrowUp':'forward','s':'backward','ArrowDown':'backward',
    'a':'left','ArrowLeft':'left','d':'right','ArrowRight':'right',' ':'stop','x':'stop'
  };
  const held = new Set();
  document.addEventListener('keydown', e => {
    if (held.has(e.key)) return;
    if (keyMap[e.key]) { held.add(e.key); cmd(keyMap[e.key]); e.preventDefault(); }
  });
  document.addEventListener('keyup', e => {
    if (keyMap[e.key]) { held.delete(e.key); cmd('stop'); }
  });
</script>
</body>
</html>
)rawhtml";

// ===== HTTP HANDLERS =====
void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleCmd() {
  String action = server.arg("action");
  if      (action == "forward")  { forward(currentSpeed);  Serial.println("▲ Forward"); }
  else if (action == "backward") { backward(currentSpeed); Serial.println("▼ Backward"); }
  else if (action == "left")     { turnLeft(currentSpeed); Serial.println("◄ Left"); }
  else if (action == "right")    { turnRight(currentSpeed);Serial.println("► Right"); }
  else if (action == "stop")     { stopAll();              Serial.println("■ Stop"); }
  else if (action == "faster")   { currentSpeed = min(255, currentSpeed + 25); Serial.print("Speed: "); Serial.println(currentSpeed); }
  else if (action == "slower")   { currentSpeed = max(50,  currentSpeed - 25); Serial.print("Speed: "); Serial.println(currentSpeed); }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true,\"speed\":" + String(currentSpeed) + "}");
}

void handleNotFound() { server.send(404, "text/plain", "Not found"); }

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  ledcAttach(LEFT_ENA,  freq, resolution);
  ledcAttach(RIGHT_ENB, freq, resolution);

  pinMode(LEFT_IN1,   OUTPUT); pinMode(LEFT_IN2,   OUTPUT);
  pinMode(RIGHT_IN3,  OUTPUT); pinMode(RIGHT_IN4,  OUTPUT);

  stopAll();

  WiFi.softAP(ssid, password);
  IPAddress ip = WiFi.softAPIP();
  Serial.println("\n========== ROVER WiFi READY ==========");
  Serial.print("Hotspot:  "); Serial.println(ssid);
  Serial.print("Password: "); Serial.println(password);
  Serial.print("Open:     http://"); Serial.println(ip);
  Serial.println("======================================\n");

  server.on("/",    handleRoot);
  server.on("/cmd", handleCmd);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() { server.handleClient(); }
