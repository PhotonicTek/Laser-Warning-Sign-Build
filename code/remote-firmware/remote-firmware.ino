// ---- Debug serial (optional, off by default) ----------------------
// ATtiny84 has no hardware UART. Flip this to 1 for bench testing with
// SoftwareSerial on the two spare pins (PB0 = TX, PA0 = RX, unused).
// Leave it 0 for the final build in the remote -- saves flash and
// avoids the extra pins being tied up.
#define DEBUG_SERIAL 0

#if DEBUG_SERIAL
  #include <SoftwareSerial.h>
  #define DBG_RX PIN_PA0
  #define DBG_TX PIN_PB0
  SoftwareSerial dbg(DBG_RX, DBG_TX);
  #define DBG_BEGIN(baud)   dbg.begin(baud)
  #define DBG_PRINT(...)    dbg.print(__VA_ARGS__)
  #define DBG_PRINTLN(...)  dbg.println(__VA_ARGS__)
#else
  #define DBG_BEGIN(baud)
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
#endif

// RF24 auto-detects ATtiny84 (defines RF24_TINY internally) and drives
// the USI peripheral directly -- no <SPI.h> include needed here.
#include <RF24.h>

#define CE_PIN  PIN_PA3
#define CSN_PIN PIN_PA2
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "43561";

#define BTN_ON     PIN_PB1
#define BTN_OFF    PIN_PA7
#define BTN_ENABLE PIN_PB2

#define BURST_DURATION_MS  1500   // total time to repeat the command
#define BURST_INTERVAL_MS  15     // gap between repeats within a burst
#define DEBOUNCE_MS        50

void sendBurst(char c, const char* label) {
  DBG_PRINT("Sending '");
  DBG_PRINT(label);
  DBG_PRINTLN("' burst...");

  radio.stopListening();

  uint32_t start = millis();
  uint16_t sent = 0;
  uint16_t acked = 0;

  while (millis() - start < BURST_DURATION_MS) {
    bool ok = radio.write(&c, sizeof(c));
    sent++;
    if (ok) acked++;
    delay(BURST_INTERVAL_MS);
  }

  DBG_PRINT(label);
  DBG_PRINT(": sent ");
  DBG_PRINT(sent);
  DBG_PRINT(", acked ");
  DBG_PRINTLN(acked);
}

// Waits for the currently-pressed button to be released so a single
// press doesn't get interpreted as being held for the whole burst.
void waitForRelease(uint8_t pin) {
  while (digitalRead(pin) == HIGH) {
    delay(10);
  }
  delay(DEBOUNCE_MS);
}

void setup() {
  DBG_BEGIN(9600);
  delay(500);
  DBG_PRINTLN("=== TX starting ===");

  // External pull-down resistors are used on these buttons, so no
  // internal pull-up. Pins idle LOW and the button pulls them HIGH
  // when pressed.
  pinMode(BTN_ON, INPUT);
  pinMode(BTN_OFF, INPUT);
  pinMode(BTN_ENABLE, INPUT);

  if (!radio.begin()) {
    DBG_PRINTLN("ERROR: nRF24 not responding! Check wiring.");
    while (1) {} // halt
  }

  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.setRetries(5, 15);
  radio.setAutoAck(true);
  radio.stopListening();

  DBG_PRINTLN("Radio initialized. Waiting for button presses...");
}

void loop() {
  if (digitalRead(BTN_ON) == HIGH) {
    DBG_PRINTLN("Button: ON pressed");
    sendBurst('1', "ON");
    waitForRelease(BTN_ON);
  }

  if (digitalRead(BTN_OFF) == HIGH) {
    DBG_PRINTLN("Button: OFF pressed");
    sendBurst('0', "OFF");
    waitForRelease(BTN_OFF);
  }

  if (digitalRead(BTN_ENABLE) == HIGH) {
    DBG_PRINTLN("Button: ENABLE pressed");
    sendBurst('2', "ENABLE");
    waitForRelease(BTN_ENABLE);
  }
}
