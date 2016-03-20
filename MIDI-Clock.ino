#include <TimerOne.h>

/*
 * For all features: if you do not define the pin, the feature will be disabled!
 */

/*
 * FEATURE: TAP BPM INPUT
 */
#define TAP_PIN 2

#define MINIMUM_TAPS 3
#define EXIT_MARGIN 150 // If no tap after 150% of last tap interval -> measure and set

/*
 * FEATURE: DIMMER BPM INPUT
 */
#define DIMMER_INPUT_PIN A0

#define DIMMER_CHANGE_MARGIN 2

/*
 * FEATURE: BLINK TEMPO LED
 */
#define BLINK_OUTPUT_PIN 5
#define BLINK_TIME 4 // How long to keep LED lit in CLOCK counts (so range is [0,24])

/*
 * FEATURE: SYNC PULSE OUTPUT
 */
#define SYNC_OUTPUT_PIN 9 // Can be used to drive sync analog sequencer (Korg Monotribe etc ...)

/*
 * FEATURE: Send MIDI start/stop
 */
#define START_STOP_INPUT_PIN A1

#define MIDI_START 0xFA
#define MIDI_STOP 0xFC

/*
 * FEATURE: EEPROM BPM storage
 */
#define EEPROM_ADDRESS 0 // Where to save BPM
#ifdef EEPROM_ADDRESS
#include <EEPROM.h>
#endif

/*
 * FEATURE: MIDI forwarding
 */
#define MIDI_FORWARD

/*
 * GENERAL PARAMETERS
 */
#define MIDI_TIMING_CLOCK 0xF8
#define CLOCKS_PER_BEAT 24
#define MINIMUM_BPM 40 // Used for debouncing
#define MAXIMUM_BPM 300 // Used for debouncing
#define DEBOUNCE_INTERVAL 500L // Milliseconds

long intervalMicroSeconds;
int bpm;

boolean initialized = false;
long minimumTapInterval = 60L * 1000 * 1000 / MAXIMUM_BPM;
long maximumTapInterval = 60L * 1000 * 1000 / MINIMUM_BPM;

volatile long firstTapTime = 0;
volatile long lastTapTime = 0;
volatile long timesTapped = 0;

volatile int blinkCount = 0;

int lastDimmerValue = 0;

boolean playing = false;
long lastStartStopTime = 0;

void setup() {
  Serial.begin(38400);
  //  Set MIDI baud rate:
  Serial1.begin(31250);

  // Set pin modes
#ifdef BLINK_OUTPUT_PIN
  pinMode(BLINK_OUTPUT_PIN, OUTPUT);
#endif
#ifdef SYNC_OUTPUT_PIN
  pinMode(SYNC_OUTPUT_PIN, OUTPUT);
#endif
#ifdef DIMMER_INPUT_PIN
  pinMode(DIMMER_INPUT_PIN, INPUT);
#endif
#ifdef START_STOP_INPUT_PIN
  pinMode(START_STOP_INPUT_PIN, INPUT);
#endif

#ifdef EEPROM_ADDRESS
  // Get the saved BPM value
  bpm = EEPROM.read(EEPROM_ADDRESS) + 40; // We're subtracting 40 when saving to have higher range
#endif

#ifdef TAP_PIN
  // Interrupt for catching tap events
  attachInterrupt(digitalPinToInterrupt(TAP_PIN), tapInput, RISING);
#endif

  // Attach the interrupt to send the MIDI clock and start the timer
  Timer1.initialize(intervalMicroSeconds);
  Timer1.setPeriod(calculateIntervalMicroSecs(bpm));
  Timer1.attachInterrupt(sendClockPulse);

#ifdef DIMMER_INPUT_PIN
  // Initialize dimmer value
  lastDimmerValue = analogRead(DIMMER_INPUT_PIN);
#endif
}

void loop() {
  long now = micros();

#ifdef TAP_PIN
  /*
   * Handle tapping of the tap tempo button
   */
  if (timesTapped > 0 && timesTapped < MINIMUM_TAPS && (now - lastTapTime) > maximumTapInterval) {
    // Single taps, not enough to calculate a BPM -> ignore!
    //    Serial.println("Ignoring lone taps!");
    timesTapped = 0;
  } else if (timesTapped >= MINIMUM_TAPS) {
    long avgTapInterval = (lastTapTime - firstTapTime) / (timesTapped - 1);
    if ((now - lastTapTime) > (avgTapInterval * EXIT_MARGIN / 100)) {
      bpm = 60L * 1000 * 1000 / avgTapInterval;

      updateBpm(now);

      timesTapped = 0;
    }
  }
#endif

#ifdef DIMMER_INPUT_PIN
  /*
   * Handle change of the dimmer input
   */
  int curDimValue = analogRead(DIMMER_INPUT_PIN);
  if (curDimValue > lastDimmerValue + DIMMER_CHANGE_MARGIN
      || curDimValue < lastDimmerValue - DIMMER_CHANGE_MARGIN) {
    // We've got movement!!
    bpm = map(curDimValue, 0, 1024, MINIMUM_BPM, MAXIMUM_BPM);

    updateBpm(now);
    lastDimmerValue = curDimValue;
  }
#endif

#ifdef START_STOP_INPUT_PIN
  /*
   * Check for start/stop button pressed
   */
  boolean startStopPressed = analogRead(START_STOP_INPUT_PIN) > 1024 / 2 ? true : false;
  if (startStopPressed && (lastStartStopTime + (DEBOUNCE_INTERVAL * 1000)) < now) {
    startOrStop();
    lastStartStopTime = now;
  }
#endif

#ifdef MIDI_FORWARD
  /*
   * Forward received serial data
   */
  while (Serial1.available()) {
    int b = Serial1.read();
    Serial1.write(b);
  }
#endif
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

void startOrStop() {
  if (!playing) {
    Serial.println("Start playing");
    Serial1.write(MIDI_START);
  } else {
    Serial.println("Stop playing");
    Serial1.write(MIDI_STOP);
  }
  playing = !playing;
}

void sendClockPulse() {
  // Write the timing clock byte
  Serial1.write(MIDI_TIMING_CLOCK);

  blinkCount = (blinkCount + 1) % CLOCKS_PER_BEAT;
  if (blinkCount == 0) {
    // Turn led on
#ifdef BLINK_OUTPUT_PIN
    analogWrite(BLINK_OUTPUT_PIN, 255);
#endif

#ifdef SYNC_OUTPUT_PIN
    // Set sync pin to HIGH
    analogWrite(SYNC_OUTPUT_PIN, 255);
#endif
  } else {
#ifdef SYNC_OUTPUT_PIN
    if (blinkCount == 1) {
      // Set sync pin to LOW
      analogWrite(SYNC_OUTPUT_PIN, 0);
    }
#ifdef BLINK_OUTPUT_PIN
#endif
    if (blinkCount == BLINK_TIME) {
      // Turn led on
      analogWrite(BLINK_OUTPUT_PIN, 0);
    }
#endif
  }
}

void updateBpm(long now) {
  // Update the timer
  long interval = calculateIntervalMicroSecs(bpm);
  Timer1.setPeriod(interval);
  blinkCount = (now - lastTapTime / interval) % CLOCKS_PER_BEAT;

#ifdef EEPROM_ADDRESS
  // Save the BPM
  EEPROM.write(EEPROM_ADDRESS, bpm - 40); // Save with offset 40 to have higher range
#endif

  Serial.print("Set BPM to: ");
  Serial.println(bpm);
}

long calculateIntervalMicroSecs(int bpm) {
  // Take care about overflows!
  return 60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT;
}

