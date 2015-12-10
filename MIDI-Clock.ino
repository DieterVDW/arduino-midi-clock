#include <TimerOne.h>

#define MIDI_TIMING_CLOCK 0xF8
#define CLOCKS_PER_BEAT 24
#define PRINT_INTERVAL 10000
#define MINIMUM_BPM 40 // Used for debouncing
#define MAXIMUM_BPM 300 // Used for debouncing
#define MINIMUM_TAPS 3
#define EXIT_MARGIN 150 // If no tap after 150% of last tap interval -> measure and set

long intervalMicroSeconds;
int bpm;

boolean initialized = false;
long minimumTapInterval = 60L * 1000 * 1000 / MAXIMUM_BPM;
long maximumTapInterval = 60L * 1000 * 1000 / MINIMUM_BPM;

volatile long firstTapTime = 0;
volatile long lastTapTime = 0;
volatile long timesTapped = 0;

void setup() {
  //  Set MIDI baud rate:
  Serial1.begin(31250);
  
  // Interrupt for catching tap events
  attachInterrupt(digitalPinToInterrupt(0), tapInput, RISING);
}

void initializeTimer() {
  // Attach the interrupt to send the MIDI clock and start the timer
  Timer1.initialize(intervalMicroSeconds);
  Timer1.attachInterrupt(sendClockPulse);
  initialized == true;
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

      if (!initialized) {
        initializeTimer();
      }
  
      // Update the timer
      Timer1.setPeriod(calculateIntervalMicroSecs(bpm));
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
  Serial1.write(MIDI_TIMING_CLOCK);
}

long calculateIntervalMicroSecs(int bpm) {
  // Take care about overflows!
  return 60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT;
}

