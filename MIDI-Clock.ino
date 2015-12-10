#include <TimerOne.h>
#include <EEPROM.h>

#define MIDI_TIMING_CLOCK 0xF8
#define CLOCKS_PER_BEAT 24
#define PRINT_INTERVAL 10000
#define MINIMUM_BPM 40 // Used for debouncing
#define MAXIMUM_BPM 300 // Used for debouncing
#define MINIMUM_TAPS 3
#define EXIT_MARGIN 150 // If no tap after 150% of last tap interval -> measure and set

#define EEPROM_ADDRESS 0 // Where to save BPM

#define BLINK_OUTPUT_PIN 5
#define BLINK_TIME 4 // How long to keep LED lit in CLOCK counts (so range is [0,24])

long intervalMicroSeconds;
int bpm;

boolean initialized = false;
long minimumTapInterval = 60L * 1000 * 1000 / MAXIMUM_BPM;
long maximumTapInterval = 60L * 1000 * 1000 / MINIMUM_BPM;

volatile long firstTapTime = 0;
volatile long lastTapTime = 0;
volatile long timesTapped = 0;

volatile int blinkCount = 0;
volatile boolean blinkState = false;

void setup() {
  //  Set MIDI baud rate:
  Serial1.begin(31250);

  // Set pin modes
  pinMode(BLINK_OUTPUT_PIN, OUTPUT);

  // Get the saved BPM value
  bpm = EEPROM.read(EEPROM_ADDRESS) + 40; // We're subtracting 40 when saving to have higher range
  
  // Interrupt for catching tap events
  attachInterrupt(digitalPinToInterrupt(0), tapInput, RISING);

  // Attach the interrupt to send the MIDI clock and start the timer
  Timer1.initialize(intervalMicroSeconds);
  Timer1.setPeriod(calculateIntervalMicroSecs(bpm));
  Timer1.attachInterrupt(sendClockPulse);
}

void loop() {
  long now = micros();
  if (timesTapped > 0 && timesTapped < MINIMUM_TAPS && (now - lastTapTime) > maximumTapInterval) {
    // Single taps, not enough to calculate a BPM -> ignore!
//    Serial.println("Ignoring lone taps!");
    timesTapped = 0;
  } else if (timesTapped >= MINIMUM_TAPS) {
    long avgTapInterval = (lastTapTime - firstTapTime) / (timesTapped-1);
    if ((now - lastTapTime) > (avgTapInterval * EXIT_MARGIN / 100)) {
      bpm = 60L * 1000 * 1000 / avgTapInterval;

      // Update the timer
      Timer1.setPeriod(calculateIntervalMicroSecs(bpm));

      // Save the BPM
      EEPROM.write(EEPROM_ADDRESS, bpm - 40); // Save with offset 40 to have higher range

      Serial.print("Set BPM to: ");
      Serial.println(bpm);

      timesTapped = 0;
    }
  }
  delay(100);
}

void tapInput() {
  long now = micros();
  if (now - lastTapTime < minimumTapInterval) {
    return; // Debounce
  }

  if (timesTapped == 0) {
    firstTapTime = now;
  }

  timesTapped++;
  lastTapTime = now;
  Serial.println("Tap!");
}

void sendClockPulse() {
  // Write the timing clock byte
  Serial1.write(MIDI_TIMING_CLOCK);

  blinkCount = (blinkCount+1) % CLOCKS_PER_BEAT;
  if (blinkState != (blinkCount < BLINK_TIME)) {
    // Change the led state
    blinkState = !blinkState;
    analogWrite(BLINK_OUTPUT_PIN, blinkState ? 255 : 0);
  }
}

long calculateIntervalMicroSecs(int bpm) {
  // Take care about overflows!
  return 60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT;
}

