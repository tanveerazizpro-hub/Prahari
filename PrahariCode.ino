#include <SoftwareSerial.h>

// --- PIN CONFIGURATION ---
const int PIEZO_PIN = A0;
const int IR_PIN = 2;
const int LED_PIN = 13;
SoftwareSerial btSerial(10, 11); // RX on 10, TX on 11

// --- PIEZO (TRIPLE TAP) VARIABLES ---
float baseline = 0;
unsigned long lastTapTime = 0;
const int COOLDOWN_MS = 200; 
const int DEVIATION = 20;
int tapCount = 0;
const int TAP_WINDOW_MS = 1500; // STRICTLY 1.5 seconds to complete 3 taps
unsigned long sequenceStartTime = 0;

// --- IR (REMOVAL) VARIABLES ---
bool isIrRemoved = false; 
bool irSosTriggered = false;
unsigned long irRemovedStartTime = 0;
const int IR_TIMEOUT_MS = 3000; // 3 seconds continuous removal required

void setup() {
  Serial.begin(115200); // For USB debugging
  btSerial.begin(9600); // For HC-05 Bluetooth
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT);
  
  delay(2000); // Give sensors time to power up

  // Calibrate piezo baseline
  long sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogRead(PIEZO_PIN);
    delay(10);
  }
  baseline = sum / 50.0;
  
  Serial.println("Trinetra Master Firmware Ready.");
  Serial.println("Ready. Tap 3 times within 1.5s, or remove IR for 3s.");
  btSerial.println("TRINETRA_READY");
}

void loop() {
  unsigned long now = millis();

  // ==========================================
  // 1. IR SENSOR (REMOVAL DETECTION)
  // ==========================================
  int irState = digitalRead(IR_PIN);
  
  // HIGH (1) means no reflection (device removed)
  if (irState == HIGH) {
    if (!isIrRemoved) {
      // The exact moment it was removed
      isIrRemoved = true; 
      irSosTriggered = false;
      irRemovedStartTime = now;
      Serial.println("IR gap detected... timer started.");
    } 
    // If it has been removed for 3 seconds AND we haven't triggered yet
    else if (!irSosTriggered && (now - irRemovedStartTime >= IR_TIMEOUT_MS)) {
      irSosTriggered = true; // Prevent spamming
      
      Serial.println("=========================");
      Serial.println(">>> SOS <<< (IR 3s)");
      Serial.println("=========================");
      
      btSerial.println("SOS"); 
      
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
    }
  } 
  // LOW (0) means safe (strap detected)
  else {
    if (isIrRemoved) {
      isIrRemoved = false; 
      irSosTriggered = false;
      Serial.println("Device re-attached. IR Timer reset.");
    }
  }

  // ==========================================
  // 2. PIEZO SENSOR (TRIPLE TAP DETECTION)
  // ==========================================
  int val = analogRead(PIEZO_PIN);

  // Track baseline drift continuously
  baseline = baseline * 0.99 + val * 0.01; 

  // STRICT TAP-OR-DIE TIMER
  if (tapCount > 0 && (now - sequenceStartTime > TAP_WINDOW_MS)) {
    tapCount = 0;
    Serial.println(">>> DIED (Too slow) <<<");
  }

  // DETECT TAP
  if (abs(val - baseline) > DEVIATION && (now - lastTapTime > COOLDOWN_MS)) {
    lastTapTime = now;
    
    // Start the death timer on the very first tap
    if (tapCount == 0) {
      sequenceStartTime = now;
    }

    tapCount++;
    Serial.print("Tap ");
    Serial.println(tapCount);

    digitalWrite(LED_PIN, HIGH);
    delay(20);
    digitalWrite(LED_PIN, LOW);

    // SUCCESS
    if (tapCount >= 3) {
      Serial.println("=========================");
      Serial.println(">>> SOS <<<");
      Serial.println("=========================");
      
      btSerial.println("SOS"); 
      
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      
      tapCount = 0; 
    }
  }
  
  delay(5); // Keep loop stable
}
