// ---------------- CONFIG ----------------
//2389
const int AZI_DIR  = 2;
const int AZI_STEP = 3;

const int EL_DIR  = 8;
const int EL_STEP = 9;

const float STEPS_PER_DEG_AZI = 50000.0 / 90.0;
const float STEPS_PER_DEG_EL  = 1000000.0 / 90.0;

// Step timing (bigger = slower)
const unsigned long AZI_STEP_INTERVAL_US = 3000;
const unsigned long EL_STEP_INTERVAL_US  = 100;

// ---------------- STRUCTS ----------------
struct Stepper {
  int dirPin;
  int stepPin;
  long position;
  long target;
  unsigned long lastStepTime;
  float accumulator;
};

Stepper azi = {AZI_DIR, AZI_STEP, 0, 0, 0, 0.0};
Stepper el  = {EL_DIR,  EL_STEP,  0, 0, 0, 0.0};

// ---------------- LOW-LEVEL STEP ----------------
inline void pulseStep(Stepper &m) {
  digitalWrite(m.stepPin, HIGH);
  delayMicroseconds(2);
  digitalWrite(m.stepPin, LOW);
}

// ---------------- SETUP ----------------
void setup() {
  pinMode(AZI_DIR, OUTPUT);
  pinMode(AZI_STEP, OUTPUT);
  pinMode(EL_DIR, OUTPUT);
  pinMode(EL_STEP, OUTPUT);
  Serial.begin(115200);
}

// ---------------- STEP UPDATE ----------------
void updateStepper(Stepper &m, unsigned long stepInterval) {
  if (m.position == m.target) return;

  unsigned long now = micros();
  if (now - m.lastStepTime < stepInterval) return;
  m.lastStepTime = now;

  digitalWrite(m.dirPin, (m.target > m.position) ? HIGH : LOW);
  pulseStep(m);

  m.position += (m.target > m.position) ? 1 : -1;
}

// ---------------- CALIBRATION ----------------
void calibrateStepper(Stepper &m, long steps, bool dirForward, unsigned long stepInterval) {
  long startPos = m.position;
  m.target = startPos + (dirForward ? steps : -steps);

  while (m.position != m.target) {
    updateStepper(m, stepInterval);
  }

  m.target = startPos;
  while (m.position != m.target) {
    updateStepper(m, stepInterval);
  }
}

void calibrateAz(long steps, bool dirForward = true) {
  calibrateStepper(azi, steps, dirForward, AZI_STEP_INTERVAL_US);
}

void calibrateEl(long steps, bool dirForward = true) {
  calibrateStepper(el, steps, dirForward, EL_STEP_INTERVAL_US);
}

// ---------------- FRACTIONAL SPEED CONTROL ----------------
void spinStepper(Stepper &m, float speed) {
  m.accumulator += speed;

  long stepIncrement = (long)m.accumulator;
  m.accumulator -= stepIncrement;

  m.target += stepIncrement;
}

void spinAz(float speed) {
  spinStepper(azi, speed);
}

void spinEl(float speed) {
  spinStepper(el, speed);
}

void stepOnce(Stepper &m, bool forward) {
  digitalWrite(m.dirPin, forward ? HIGH : LOW);
  pulseStep(m);
  m.position += forward ? 1 : -1;
}

void moveAzDeg(float deg) {
  moveDegrees(azi, deg);
}

void moveElDeg(float deg) {
  moveDegrees(el, deg);
}

void moveDegrees(Stepper &m, float degrees) {
  long steps = lround(degrees * (
    (&m == &azi) ? STEPS_PER_DEG_AZI : STEPS_PER_DEG_EL
  ));

  m.target += steps;   // ONE-TIME target change
}

// ---------------- LOOP ----------------
void loop() {
  // VERY slow smooth motion
  
  spinAz(0.05);
  //spinEl(-2.5);

  const float R_AZI_DEG = 30.0;     // azimuth radius (deg)
  const float R_EL_DEG  = 30.0;     // elevation radius (deg)
  const float OMEGA     = 0.1;     // rad/sec (circle speed)

  static unsigned long lastTime = micros();
  unsigned long now = micros();
  float dt = (now - lastTime) * 1e-6;
  lastTime = now;

  static float theta = 0.0;
  theta += OMEGA * dt;

  float azi_deg_per_sec = -R_AZI_DEG * OMEGA * sin(theta);
  float el_deg_per_sec  =  R_EL_DEG  * OMEGA * cos(theta);

  float azi_dir = 1;
  float el_dir = 1;

  if (azi_deg_per_sec < 0) {azi_dir = -1;}
  if (el_deg_per_sec < 0) {el_dir = -1;}

  //spinAz(azi_dir * 0.05);
  spinEl(el_dir * 2.5);

  // spinAz(5*azi_deg_per_sec * STEPS_PER_DEG_AZI * dt);
  // spinEl(300 * el_deg_per_sec  * STEPS_PER_DEG_EL  * dt);
  //static bool done = false;

  // if (!done) {
  //   moveAzDeg(-1.0);   // move exactly +1 degree
  //   // moveElDeg(-1.0); // example
  //   done = true;
  // }

  updateStepper(azi, AZI_STEP_INTERVAL_US);
  updateStepper(el,  EL_STEP_INTERVAL_US);

}
