#include <TimerOne.h>
#include <EEPROM.h>

#define MIDI_TIMING_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP 0xFC

#define CLOCKS_PER_BEAT 24
#define PRINT_INTERVAL 10000
#define MINIMUM_BPM 40 // Used for debouncing
#define MAXIMUM_BPM 300 // Used for debouncing

#define TAP_PIN 2
#define MINIMUM_TAPS 3
#define EXIT_MARGIN 150 // If no tap after 150% of last tap interval -> measure and set

#define EEPROM_ADDRESS 0 // Where to save BPM

#define BLINK_OUTPUT_PIN 5
#define BLINK_TIME 4 // How long to keep LED lit in CLOCK counts (so range is [0,24])

#define SYNC_OUTPUT_PIN 9 // Can be used to drive sync analog sequencer (Korg Monotribe etc ...)

#define DIMMER_INPUT_PIN A0
#define DIMMER_CHANGE_MARGIN 2

#define START_STOP_INPUT_PIN A1
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
  //  Set MIDI baud rate:
  Serial1.begin(31250);

  // Set pin modes
  pinMode(BLINK_OUTPUT_PIN, OUTPUT);
  pinMode(SYNC_OUTPUT_PIN, OUTPUT);
  pinMode(DIMMER_INPUT_PIN, INPUT);
  pinMode(START_STOP_INPUT_PIN, INPUT);

  // Get the saved BPM value
  bpm = EEPROM.read(EEPROM_ADDRESS) + 40; // We're subtracting 40 when saving to have higher range
  
  // Interrupt for catching tap events
  attachInterrupt(digitalPinToInterrupt(TAP_PIN), tapInput, RISING);

  // Attach the interrupt to send the MIDI clock and start the timer
  Timer1.initialize(intervalMicroSeconds);
  Timer1.setPeriod(calculateIntervalMicroSecs(bpm));
  Timer1.attachInterrupt(sendClockPulse);

  // Initialize dimmer value
  lastDimmerValue = analogRead(DIMMER_INPUT_PIN);
}

void loop() {
  long now = micros();

  /*
   * Handle tapping of the tap tempo button
   */
  if (timesTapped > 0 && timesTapped < MINIMUM_TAPS && (now - lastTapTime) > maximumTapInterval) {
    // Single taps, not enough to calculate a BPM -> ignore!
//    Serial.println("Ignoring lone taps!");
    timesTapped = 0;
  } else if (timesTapped >= MINIMUM_TAPS) {
    long avgTapInterval = (lastTapTime - firstTapTime) / (timesTapped-1);
    if ((now - lastTapTime) > (avgTapInterval * EXIT_MARGIN / 100)) {
      bpm = 60L * 1000 * 1000 / avgTapInterval;

      updateBpm();

      timesTapped = 0;
    }
  }

  /*
   * Handle change of the dimmer input
   */
  int curDimValue = analogRead(DIMMER_INPUT_PIN);
  if (curDimValue > lastDimmerValue + DIMMER_CHANGE_MARGIN
      || curDimValue < lastDimmerValue - DIMMER_CHANGE_MARGIN) {
    // We've got movement!!
    bpm = map(curDimValue, 0, 1024, MINIMUM_BPM, MAXIMUM_BPM);

    updateBpm();
    lastDimmerValue = curDimValue;
  }

  /*
   * Check for start/stop button pressed
   */
  boolean startStopPressed = analogRead(START_STOP_INPUT_PIN) > 1024/2 ? true : false;
  if (startStopPressed && (lastStartStopTime+(DEBOUNCE_INTERVAL*1000)) < now) {
    startOrStop();
    lastStartStopTime = now;
  }
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

  blinkCount = (blinkCount+1) % CLOCKS_PER_BEAT;
  if (blinkCount == 0) {
    // Turn led on
    analogWrite(BLINK_OUTPUT_PIN, 255);

    // Set sync pin to HIGH
    analogWrite(SYNC_OUTPUT_PIN, 255);
  } else {
    if (blinkCount == 1) {
      // Set sync pin to LOW
      analogWrite(SYNC_OUTPUT_PIN, 0);
    }
    if (blinkCount == BLINK_TIME) {
      // Turn led on
      analogWrite(BLINK_OUTPUT_PIN, 0);
    }
  }
}

void updateBpm() {
    // Update the timer
  Timer1.setPeriod(calculateIntervalMicroSecs(bpm));

  // Save the BPM
  EEPROM.write(EEPROM_ADDRESS, bpm - 40); // Save with offset 40 to have higher range

  Serial.print("Set BPM to: ");
  Serial.println(bpm);
}

long calculateIntervalMicroSecs(int bpm) {
  // Take care about overflows!
  return 60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT;
}

