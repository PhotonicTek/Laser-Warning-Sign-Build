
#include <SPI.h>
#include <RF24.h>
#include <FastLED.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>

// ---------- Pin definitions ----------
#define CE_PIN        3   // PA3
#define CSN_PIN       2   // PA2
#define LED_DATA_PIN  10  // PB2
#define BOOST_EN_PIN  7   // PA7

// ---------- LED layout ----------
#define ZONE0_LEN       16
#define ZONE1_LEN       18
#define ZONE2_LEN       21

#define NUM_ZONES       3
#define NUM_LEDS        (ZONE0_LEN + ZONE1_LEN + ZONE2_LEN)  // 55
#define MAX_BRIGHTNESS  100   // hard cap, out of 255

CRGB leds[NUM_LEDS];

const uint16_t zoneLen[NUM_ZONES]   = { ZONE0_LEN, ZONE1_LEN, ZONE2_LEN };
const uint16_t zoneStart[NUM_ZONES] = {
  0,
  ZONE0_LEN,
  ZONE0_LEN + ZONE1_LEN
};

// ---------- Radio ----------
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "43561";

// ---------- Sleep / listen tuning ----------
#define RADIO_POWERUP_SETTLE_MS   5     // time for nRF24 to leave power-down
#define RADIO_LISTEN_WINDOW_MS    20   // how long to listen per wake

// ---------- State ----------
bool ledsEnabled = false;


ISR(WDT_vect) {
}

void setupWatchdogForSleep() {
  cli();
  wdt_reset();
  MCUSR &= ~(1 << WDRF);              // clear watchdog reset flag
  WDTCSR |= (1 << WDCE) | (1 << WDE); // enable config change window
  WDTCSR = (1 << WDIE) | (1 << WDP2) | (1 << WDP1);
  sei();
}

// ---------- Power control (LED boost converter) ----------
void boostEnable() {
  digitalWrite(BOOST_EN_PIN, LOW);   // LOW = MOSFET ON = boost enabled
}

void boostDisable() {
  digitalWrite(BOOST_EN_PIN, HIGH);  // HIGH = MOSFET OFF = boost disabled
}

// Turn power on only if it isn't already on, with a short settle
// delay so the boost converter's output is stable before we clock
// out LED data.
void powerOnIfNeeded() {
  if (!ledsEnabled) {
    boostEnable();
    delay(100);
    ledsEnabled = true;
  }
}

void powerOff() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  pinMode(LED_DATA_PIN, OUTPUT);
  digitalWrite(LED_DATA_PIN, LOW);

  boostDisable();
  ledsEnabled = false;
}

// ---------- Zone helpers ----------
// zoneIndex is 0/1/2 (zone1/zone2/zone3 in the command comments
// above). Uses the zoneStart/zoneLen tables so each zone can have a
// different pixel count.
void setZone(uint8_t zoneIndex, CRGB color) {
  uint16_t start = zoneStart[zoneIndex];
  uint16_t count = zoneLen[zoneIndex];
  for (uint16_t i = start; i < start + count; i++) {
    leds[i] = color;
  }
}

// ---------- Command handling ----------
void handleCommand(char cmd) {
  switch (cmd) {
    case '1':  // ON
      powerOnIfNeeded();
      setZone(0, CRGB::Green);
      setZone(1, CRGB::Black);
      setZone(2, CRGB::Yellow);
      FastLED.show();
      break;

    case '2':  // ENABLE
      powerOnIfNeeded();
      setZone(0, CRGB::Black);
      setZone(1, CRGB::Red);
      setZone(2, CRGB::Yellow);
      FastLED.show();
      break;

    case '0':  // OFF
      powerOff();
      break;

    default:
      // Unknown command byte - ignore.
      break;
  }
}

// ---------- Sleep cycle ----------
// Called from loop() whenever the sign is off. Puts the MCU into
// power-down sleep for ~1s, wakes, briefly powers up the radio and
// listens for a command, then either handles it or goes back to
// sleep. Returns once either a command has been handled or the
// listen window has expired with nothing received.
void enterSleepCycle() {
  // Drop the radio into its own low-power state before sleeping.
  radio.powerDown();

  setupWatchdogForSleep();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
#if defined(BODS) && defined(BODSE)
  sleep_bod_disable();  // best-effort brown-out-detector disable during sleep
#endif
  sleep_cpu();           // <-- MCU halts here until the WDT ISR fires
  sleep_disable();5

  // Woken up - bring the radio back and listen briefly.
  radio.powerUp();
  delay(RADIO_POWERUP_SETTLE_MS);
  radio.startListening();

  uint32_t listenStart = millis();
  while (millis() - listenStart < RADIO_LISTEN_WINDOW_MS) {
    if (radio.available()) {
      char cmd = 0;
      radio.read(&cmd, sizeof(cmd));
      handleCommand(cmd);

      if (ledsEnabled) {
        // Sign just got turned on - bail out of the sleep cycle and
        // let loop() take over with continuous polling. Radio is
        // already powered up and listening, nothing more to do.
        return;
      }
      // Command was OFF (or unknown) while already off - keep
      // listening out the rest of this window in case more packets
      // are queued, then fall through to going back to sleep.
    }
  }
  // Nothing (further) received this cycle - go back to sleep next
  // time loop() calls us.
}

// ---------- Setup ----------
void setup() {
  pinMode(BOOST_EN_PIN, OUTPUT);
  boostDisable();  // start with LEDs unpowered

  pinMode(LED_DATA_PIN, OUTPUT);
  digitalWrite(LED_DATA_PIN, LOW);

  FastLED.addLeds<WS2812, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(MAX_BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // We don't use the ADC at all - make sure it's off.
  power_adc_disable();

  // Radio init - retry a few times in case of a slow-starting
  // regulator/brownout on power-up.
  bool radioOk = false;
  for (uint8_t attempt = 0; attempt < 5 && !radioOk; attempt++) {
    radioOk = radio.begin();
    if (!radioOk) delay(50);
  }

  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.setRetries(5, 15);
  radio.setAutoAck(true);
  radio.startListening();

  if (!radioOk) {
    for (uint8_t i = 0; i < 3; i++) {
      setZone(0, CRGB::Red);
      FastLED.show();
      delay(150);
      setZone(0, CRGB::Black);
      FastLED.show();
      delay(150);
    }
  }

  // Start life in the low-power sleep/poll cycle (ledsEnabled is
  // already false at this point).
}

// ---------- Main loop ----------
void loop() {
  if (ledsEnabled) {
    // Fully awake mode
    while (radio.available()) {
      char cmd = 0;
      radio.read(&cmd, sizeof(cmd));
      handleCommand(cmd);
    }
  } else {
    enterSleepCycle();
  }
}
