# Arduino MIDI clock with tap tempo

As seen on LittleBits:
http://littlebits.cc/projects/littlebits-arduino-midi-master-clock-with-tap-tempo

You can also do this with a regular Arduino, but you'll have some extra work soldering the button(s)/dimmer

## How to make a Arduino MIDI connector
See the excellent tutorial at this link:
http://www.instructables.com/id/Send-and-Receive-MIDI-with-Arduino/step3/Send-MIDI-Messages-with-Arduino-Hardware/ .

## Connections

### For MIDI out

- Connect MIDI ground to Arduino ground
- Connect MIDI 5V to Arduino 5V **with a 220 Ohm resistor in between**
- Connect MIDI signal line to Arduino serial input pin (D1)

### For tap in

Connect a button to D0

## Usage

- Upon startup, Arduino will start sending 100 BPM MIDI clock signal.
- Tap the tempo (minimum 3 times)
- After the last tap, clock tempo will be updated and MIDI clock signal will send new BPM

### Extra functionality:
- Connect a dimmer to A0 to set the tempo by twisting the knob!
- Tempo blinking LED on pin A5
- Sync signal on pin A9 (for example to sync with Korg Monotribe...)
- MIDI real-time start/stop is sent when button press is detected on A1 port
- Stores the BPM value and restores it on power up

# Branches

- master : Code for the Arduino Leonardo (or LittleBits Arduino)
- arduino-uno: Code for Arduino Uno and compatible devices (D0/D1 RX/TX pins are used for MIDI in/out, so no debug console!)
- olimex-midi-shield: Based on the arduino-uno branch, but with some tweaks to make the code work better with the Olimex MIDI shield (https://www.olimex.com/Products/Duino/Shields/SHIELD-MIDI/open-source-hardware) (Work in progress!)

# Enjoy! :)
