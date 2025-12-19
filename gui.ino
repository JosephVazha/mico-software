#include <WiFi.h>

// ---------------- WIFI ----------------
const char* ssid = "MICO_Groundstation";
const char* password = "6767";

WiFiServer server(80);
String header;

// ---------------- CONFIG ----------------
const int AZI_DIR  = 25;
const int AZI_STEP = 26;
const int EL_DIR   = 27;
const int EL_STEP  = 14;

const float STEPS_PER_DEG_AZI = 50000.0 / 90.0;
const float STEPS_PER_DEG_EL  = 1000000.0 / 90.0;

const unsigned long AZI_STEP_INTERVAL_US = 3000;
const unsigned long EL_STEP_INTERVAL_US  = 100;

// ---------------- STRUCT ----------------
struct Stepper {
  int dirPin;
  int stepPin;
  long position;
  long target;
  unsigned long lastStepTime;
  float accumulator;
};

Stepper azi = {AZI_DIR, AZI_STEP, 0, 0, 0, 0};
Stepper el  = {EL_DIR,  EL_STEP,  0, 0, 0, 0};

// ---------------- MOTION STATE ----------------
float aziSpeed = 0.0;
float elSpeed  = 0.0;
bool circleMode = false;

// ---------------- LOW LEVEL STEP ----------------
inline void pulseStep(Stepper &m) {
  digitalWrite(m.stepPin, HIGH);
  delayMicroseconds(2);
  digitalWrite(m.stepPin, LOW);
}

// ---------------- STEPPER UPDATE ----------------
void updateStepper(Stepper &m, unsigned long interval) {
  if (m.position == m.target) return;

  unsigned long now = micros();
  if (now - m.lastStepTime < interval) return;
  m.lastStepTime = now;

  digitalWrite(m.dirPin, (m.target > m.position) ? HIGH : LOW);
  pulseStep(m);
  m.position += (m.target > m.position) ? 1 : -1;
}

// ---------------- FRACTIONAL SPEED ----------------
void spinStepper(Stepper &m, float speed) {
  m.accumulator += speed;
  long s = (long)m.accumulator;
  m.accumulator -= s;
  m.target += s;
}

void spinAz(float s) { spinStepper(azi, s); }
void spinEl(float s) { spinStepper(el, s); }

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(AZI_DIR, OUTPUT);
  pinMode(AZI_STEP, OUTPUT);
  pinMode(EL_DIR, OUTPUT);
  pinMode(EL_STEP, OUTPUT);

  // --- FORCE AP MODE ---
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  delay(100);

  // --- START AP WITH EXPLICIT CONFIG ---
  bool ok = WiFi.softAP(
    "MICO_Groundstation",   // SSID
    "67676767",                 // password
    6,                      // channel
    false,                  // hidden
    4                       // max connections
  );

  if (!ok) {
    Serial.println("Error: SoftAP failed to start");
  }

  Serial.print("AP SSID: ");
  Serial.println(WiFi.softAPSSID());
  Serial.print("📡 AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.begin();
}


// ---------------- LOOP ----------------
void loop() {
  // -------- Web Server --------
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c;

        if (c == '\n') {
          if (currentLine.length() == 0) {

            // -------- COMMANDS --------
            if (header.indexOf("GET /azi/inc") >= 0) aziSpeed += 0.05;
            if (header.indexOf("GET /azi/dec") >= 0) aziSpeed -= 0.05;
            if (header.indexOf("GET /el/inc")  >= 0) elSpeed  += 0.05;
            if (header.indexOf("GET /el/dec")  >= 0) elSpeed  -= 0.05;

            if (header.indexOf("GET /circle/on")  >= 0) circleMode = true;
            if (header.indexOf("GET /circle/off") >= 0) circleMode = false;

            if (header.indexOf("GET /stop") >= 0) {
              aziSpeed = 0;
              elSpeed  = 0;
            }

            // -------- RESPONSE --------
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            client.println(R"rawliteral(
            <!DOCTYPE html>
            <html>
            <head>
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <title>MICO Groundstation</title>

            <style>
            :root {
              --bg: #000000;
              --panel: #0a0a0a;
              --accent: #00ffcc;
              --danger: #ff3b3b;
              --muted: #888;
              --text: #ffffff;
            }

            html, body {
              margin: 0;
              padding: 0;
              background: var(--bg);
              color: var(--text);
              font-family: "JetBrains Mono", monospace;
              height: 100%;
            }

            .container {
              max-width: 900px;
              margin: auto;
              padding: 32px;
            }

            .header {
              text-align: center;
              margin-bottom: 40px;
            }

            .header h1 {
              font-size: 28px;
              letter-spacing: 3px;
              margin: 0;
            }

            .header span {
              color: var(--accent);
              font-size: 12px;
              letter-spacing: 2px;
            }

            .panel {
              background: var(--panel);
              border: 1px solid #1a1a1a;
              border-radius: 12px;
              padding: 24px;
              margin-bottom: 24px;
            }

            .panel h2 {
              margin-top: 0;
              font-size: 16px;
              letter-spacing: 1px;
              color: var(--accent);
            }

            .status {
              font-size: 14px;
              margin-bottom: 16px;
              color: var(--muted);
            }

            .controls {
              display: flex;
              gap: 12px;
              flex-wrap: wrap;
            }

            .button {
              flex: 1;
              min-width: 120px;
              background: #111;
              border: 1px solid #222;
              color: white;
              padding: 14px;
              font-size: 14px;
              border-radius: 8px;
              cursor: pointer;
              text-align: center;
              text-decoration: none;
              transition: 0.15s ease;
            }

            .button:hover {
              background: #1a1a1a;
            }

            .button.accent {
              border-color: var(--accent);
              color: var(--accent);
            }

            .button.danger {
              border-color: var(--danger);
              color: var(--danger);
            }

            .footer {
              text-align: center;
              font-size: 11px;
              color: var(--muted);
              margin-top: 40px;
            }
            </style>
            </head>

            <body>
            <div class="container">

              <div class="header">
                <h1>MICO GROUNDSTATION CONTROL</h1>
                <span>ESP32 GIMBAL INTERFACE</span>
              </div>

              <div class="panel">
                <h2>AZIMUTH CONTROL</h2>
                <div class="status">Speed: )rawliteral");
            client.print(aziSpeed);
            client.println(R"rawliteral(</div>
                <div class="controls">
                  <a class="button accent" href="/azi/inc">INCREASE</a>
                  <a class="button" href="/azi/dec">DECREASE</a>
                </div>
              </div>

              <div class="panel">
                <h2>ELEVATION CONTROL</h2>
                <div class="status">Speed: )rawliteral");
            client.print(elSpeed);
            client.println(R"rawliteral(</div>
                <div class="controls">
                  <a class="button accent" href="/el/inc">INCREASE</a>
                  <a class="button" href="/el/dec">DECREASE</a>
                </div>
              </div>

              <div class="panel">
                <h2>AUTONOMOUS MODES</h2>
                <div class="status">Circle Mode: )rawliteral");
            client.print(circleMode ? "ACTIVE" : "INACTIVE");
            client.println(R"rawliteral(</div>
                <div class="controls">
                  <a class="button accent" href="/circle/on">ENABLE CIRCLE</a>
                  <a class="button" href="/circle/off">DISABLE</a>
                </div>
              </div>

              <div class="panel">
                <h2>EMERGENCY</h2>
                <div class="controls">
                  <a class="button danger" href="/stop">STOP ALL MOTION</a>
                </div>
              </div>

              <div class="footer">
                MICO SYSTEMS • GROUNDSTATION • CONTROL AUTHORIZED
              </div>

            </div>
            </body>
            </html>
            )rawliteral");
            client.println("</body></html>");

            break;
          } else currentLine = "";
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
  }

  // -------- MOTION --------
  if (circleMode) {
    static float theta = 0;
    static unsigned long lastT = micros();
    float dt = (micros() - lastT) * 1e-6;
    lastT = micros();

    theta += 0.4 * dt;
    spinAz(-5.0 * sin(theta) * STEPS_PER_DEG_AZI * dt);
    spinEl( 5.0 * cos(theta) * STEPS_PER_DEG_EL  * dt);
  } else {
    spinAz(aziSpeed);
    spinEl(elSpeed);
  }

  updateStepper(azi, AZI_STEP_INTERVAL_US);
  updateStepper(el,  EL_STEP_INTERVAL_US);
}
