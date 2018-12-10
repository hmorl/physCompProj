/*
  echo phone source code~~Harry Morley Dec 2018

  echo_check() modified from "NewPingEventTimer" example by Tim Eckel,
  from the NewPing library in order to not use delays in the sketch. (https://bitbucket.org/teckel12/arduino-new-ping/wiki/Home#!event-timer-sketch)
  
  debounceButton() uses code from the Debounce example in Arduino examples --> Digital --> Debounce
  
  mod() function (to work with negative numbers) taken from https://stackoverflow.com/a/4467559
*/

#include <NewPing.h>
#include <LiquidCrystal.h>

#define ECHO_PIN 7 // Echo Pin
#define TRIG_PIN 8 // Trigger Pin
#define MAX_DISTANCE 100 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.
#define BUTTON_PIN 6
#define PIEZO_PIN 9
#define MAX_PATTERNS 16
#define MAX_NOTES 32
#define MAX_NOTE_TIME 3000
#define SELSPEED_INIT 90

byte note[8] = {
  B00100,
  B00110,
  B00101,
  B00101,
  B00100,
  B00100,
  B11100,
  B11100,
}; //custom note char

byte noteIn[8] = {
  B00000,
  B00000,
  B01110,
  B11111,
  B11111,
  B01110,
  B00000,
  B00000,
}; //custom dot char

//---ultrasonic sensor
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.
unsigned int pingSpeed = 20; // frequency of ultrasonic sensor ping (in milliseconds)
unsigned long pingTimer;     // stores the next ping time.

int selSpeed = SELSPEED_INIT;
int selDuration;
byte pattSelector = 0;
unsigned long selTimer;
unsigned long selStartTime;
bool listening;
bool selecting;

//---recording
int patterns[MAX_NOTES][MAX_PATTERNS];
byte noteCount = 0;
byte pattCount = 0;
unsigned long prevMillis;
unsigned long startTime;
unsigned long pattDur;
bool recording;
int prevPattDur = 0;
byte buttonState;
byte prevButtonState = false;
//---playback
bool playing;
byte pbNoteIdx = 0;
byte pbNoteCount = 0;
byte pbCurrentPatt = 0;
unsigned long pbPlayhead;
int pbCurrentLen;
byte pbInterval = 100;
int pbNoteTime;
unsigned long pbNoteStartTime = 0;

//----button
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 25;
byte lastButtonReading = LOW;

//---display
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
byte blinked = false;
String blankRow = "                ";

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);   // initialize pushbutton pin as input:
  lcd.begin(16, 2);
  lcd.print("e ch o ph o n e");
  lcd.createChar(1, note);
  lcd.createChar(2, noteIn);

  pingTimer = millis(); // Start ping timer for ultrasonic sensor.
  listening = true;
  randomSeed(analogRead(0)); // seed the random (so not the same every time)

}

bool isPattEmpty(int idx) {
  bool temp = false;
  if (patterns[0][idx] == 0) {
    temp = true;
  }
  return temp;
}

int mod(int n, int m) {
  return ((n % m) + m) % m;
}

void loop() {
  buttonState = debounceButton(); //check button state (filter out bouncing)

  if (!recording) {
    //---Ultrasonic sensor pattern selection---------------------
    if (millis() >= pingTimer) {   // pingSpeed milliseconds since last ping, do another ping.
      pingTimer += pingSpeed;      // Set next ping time.
      sonar.ping_timer(echoCheck); // Send out the ping, calls echoCheck to monitor ping status.
    }
  }

  if (selecting) { // if selecting a pattern
    if (millis() >= selTimer) {
      if (random(100) < 70) {
        pattSelector = mod(pattSelector + 1, MAX_PATTERNS);
      } else {
        pattSelector = mod(pattSelector - 1, MAX_PATTERNS);
      }
      lcd.setCursor(0, 1);
      lcd.print(blankRow);
      lcd.setCursor(pattSelector, 1);
      lcd.print(pattSelector, HEX);
      selTimer += selSpeed;
      selSpeed += 10; //slow down selector speed
    }

    if ((millis() - selStartTime) >= selDuration) {
      selecting = false; //Stop moving selection carousel
      selSpeed = SELSPEED_INIT; //reset selector speed
      pbCurrentPatt = pattSelector;
      if (isPattEmpty(pattSelector)) {
        playing = false;
        analogWrite(PIEZO_PIN, 0); //silence
        lcd.setCursor(0, 0);               //
        lcd.print("empty           ");               //
      } else {
        playing = true;
        pbCurrentLen = patterns[0][pbCurrentPatt]; //set up first note duration for playback
        pbNoteStartTime = millis();
        pbNoteIdx = 0;
      }
    }
  }
  //---END---Ultrasonic sensor pattern selection-------------

  if (!recording) {

    if (buttonState == HIGH && prevButtonState == LOW && !selecting) { //if button pressed when not recording or selecying, start recording
      recording = true;

      for (int i = 0; i < MAX_NOTES; i++) {
        patterns[i][pbCurrentPatt] = 0; //resets all notes of pattern to 0 (required for when wraps around)
      }

      startTime = millis();
      playing = false;
      analogWrite(PIEZO_PIN, 0); //silence

      lcd.setCursor(0, 0);               //
      lcd.print(blankRow);               //
      lcd.setCursor(random(0, 16), 0);   //visualize the button press (a "*" in random pos on top row of lcd)
      lcd.write((byte)2);
    }

    if (playing) {
      pbPlayhead = millis() - pbNoteStartTime; // playhead = current position in note

      if (pbPlayhead < 50) { //50ms note time (not rhythmic value)
        analogWrite(PIEZO_PIN, random(0, 255)); //white(ish) noise!
      } else {
        analogWrite(PIEZO_PIN, 0); //silence
      }

      if (pbPlayhead < 200) { //200ms blink time
        if (!blinked) {
          lcd.setCursor(random(0, 16), 0);
          lcd.write((byte)1); //display note character
          blinked = true;
        }
      } else {
        lcd.setCursor(0, 0);
        lcd.print(blankRow);
      }

      if (pbPlayhead >= pbCurrentLen) { //if note finished, get next note or loop
        pbNoteIdx++;
        if (patterns[pbNoteIdx][pbCurrentPatt] == 0 || pbNoteIdx == MAX_NOTES) {
          pbNoteIdx = 0; //loop
        }
        pbNoteStartTime = millis();
        blinked = false;               //for playback visualisation (see line 85)
        pbCurrentLen = patterns[pbNoteIdx][pbCurrentPatt];
      }

    }
  } else { //Recording
    pattDur = millis() - startTime;
    byte l = map(pattDur, 0, MAX_NOTE_TIME, 0, 15);
    lcd.setCursor(l, 1);
    lcd.print(pbCurrentPatt, HEX);

    if (pattDur >= MAX_NOTE_TIME) { // if pattern reached max duration (3 secs)
      patterns[noteCount][pbCurrentPatt] =  MAX_NOTE_TIME - prevPattDur;
      recording = false;
      playing = true;
      pbCurrentLen = patterns[0][pbCurrentPatt]; //set up first note duration for playback
      pbNoteStartTime = millis();
      pbNoteIdx = 0;

      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print(pbCurrentPatt, HEX);
      lcd.setCursor(random(0, 16), 0);
      lcd.write((byte)1);
      blinked = true;

      noteCount = 0;
      prevPattDur = 0;
    } else if (buttonState == HIGH && prevButtonState == LOW) { //if button pressed when recording
      patterns[noteCount][pbCurrentPatt] = pattDur - prevPattDur; //new note is (pattern duration - prev patt duration)
      lcd.setCursor(0, 0);               //
      lcd.print(blankRow);               //
      lcd.setCursor(random(0, 16), 0);   //
      lcd.write((byte)2);                //visualize the button press (custom dot char in random pos on top row of lcd)
      prevPattDur = pattDur;
      noteCount++;
    }
  }

  prevButtonState = buttonState;
}

byte debounceButton() {
  byte reading = digitalRead(BUTTON_PIN);
  byte state;
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    state = reading;
  }
  lastButtonReading = reading;
  return state;
}

void echoCheck() { //Timer2 interrupt calls this function every 24uS where you can check the ping status (NewPing.h)
  if (sonar.check_timer()) { //Calculate distance and start selecting pattern, if ping received.
    int dist = sonar.ping_result / US_ROUNDTRIP_CM;
    if (dist < 50) {
      if (listening) {
        selTimer = millis(); //start timing;
        selStartTime = millis();
        selDuration = map(dist, 3, 50, 3000, 100);
        selecting = true;
        listening = false; // ignore further input for 3sec
      }
    }
  }

  if (!listening) {
    if ((millis() - selStartTime) > 3000) {
      listening = true;
    }
  }
}
