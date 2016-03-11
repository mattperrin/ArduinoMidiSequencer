#include <MIDI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
//#include "LiquidCrystal_I2C.h"
#define BACKLIGHT_PIN (3)
#define LED_ADDR (0x27) // might need to be 0x3F, if 0x27 doesn't work
LiquidCrystal_I2C lcd(LED_ADDR, 2, 1, 0, 4, 5, 6, 7, BACKLIGHT_PIN, POSITIVE);

class MusicNote {
  public:
    byte Velocity;
    bool Play = false;
    bool EndNote = false;
    bool Playing = false;
};

class Sequence {
  public:
    byte Pitch;
    MusicNote MusicNotes[16];
};

class Pattern {
  public:
    byte PatternTempo;
    Sequence Sequences[8];
};

class PatternsObject {
  public:
    Pattern Patterns[8];
};

const uint8_t noteOnChar[8] = { 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f };
const uint8_t noteOffChar[8] = { 0x1f, 0x11, 0x0, 0x0, 0x0, 0x11, 0x1f };
const uint8_t noteOffMajorChar[8] = { 0x1f, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1f };
const uint8_t playChar[8] = { 0x0, 0x8, 0xc, 0xe, 0xc, 0x8, 0x0 };
const int buttonPins[16] = { 53, 51, 49, 47, 45, 43, 41, 39, 52, 50, 48, 46, 44, 42, 40, 38 };

PatternsObject PatternsContainer;
byte currentPattern;
byte currentSequence;
byte currentMidiNote;
byte currentOctave;
byte currentPitch;
byte currentVelocity;
byte currentIntervalLength;
byte bpmTempo;

unsigned long currentMillis;
unsigned long previousMillis;
unsigned long differenceTiming;
long buttonTimerReset = 0;

// LCD TRACKERS FOR BPM FLASH AND PLAYHEAD POSITION
int bpmCount;
int playheadCount;
bool dirtySequenceFlag;
int sixteenInterval;

MIDI_CREATE_DEFAULT_INSTANCE();

void setup() {

  //SET DEFAULT VALUES
  currentPattern = (byte)0;
  currentSequence = (byte)0;
  currentMidiNote = (byte)6;
  currentOctave = (byte)4;
  currentPitch = (byte)42;
  currentVelocity = (byte)127;
  currentIntervalLength = (byte)16;
  bpmTempo = (byte)60;
  bpmCount = 0;
  playheadCount = 0;

  MIDI.begin();
  Serial.begin(115200);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  lcd.begin(20, 4);             // initialize the lcd

  lcd.createChar(1, (uint8_t *)noteOnChar);
  lcd.createChar(2, (uint8_t *)noteOffChar);
  lcd.createChar(3, (uint8_t *)noteOffMajorChar);
  lcd.createChar(4, (uint8_t *)playChar);

  sixteenInterval = ((1000 / (int)bpmTempo) * 60 / 2) / 4;

  lcd.home();
  lcd.print("MIDI Sequencer");
  lcd.clear();
  delay(3000);

  SetupButtons();
  SetupArrays();
  buttonTimerReset = 0;
  RedrawSequence();

}

void SetupButtons()
{
  for (int i = 0; i < 16; i++)
  {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
}

void SetupArrays()
{
  int arrsize = sizeof(PatternsContainer.Patterns) / sizeof(int);
  for (int i = 0; i < arrsize; i++)
  {
    Pattern pattern = PatternsContainer.Patterns[i];
    pattern.PatternTempo = (byte)60;
    arrsize = sizeof(pattern.Sequences) / sizeof(int);
    for (int j = 0; j < arrsize; j++)
    {
      Sequence sequence = pattern.Sequences[j];
      sequence.Pitch = (byte)currentPitch;
      arrsize = sizeof(sequence.MusicNotes) / sizeof(int);
      for (int k = 0; k < arrsize; k++)
      {
        MusicNote note = sequence.MusicNotes[k];
        note.EndNote = false;
        note.Play = false;
        note.Playing = false;
        note.Velocity = (byte)currentVelocity;
      }
    }
  }
}



// ========================================

void loop() {
  // put your main code here, to run repeatedly:

  dirtySequenceFlag = false;
  currentMillis = millis();
  differenceTiming = currentMillis - previousMillis;
  if (differenceTiming >= sixteenInterval)
  {
    PerformMidiCheck();
    lcd.home();

    //  CLEAR SIXTEENTH PLAYHEAD POSITION
    if (playheadCount == 16)
    {
      lcd.setCursor(17, 2);
      lcd.print(" ");
      playheadCount = 0;
    }

    // DRAW PLAYHEAD
    lcd.setCursor(1 + playheadCount, 2);
    lcd.print(" ");
    lcd.setCursor(2 + playheadCount, 2);
    lcd.print((char)4);

    // RESET BUTTON
    if (buttonTimerReset < -10000) {
      buttonTimerReset = 0;
    }
    buttonTimerReset -= differenceTiming;

    // CHECK BUTTONS
    if (buttonTimerReset < 0)
    {
      EvaluateSequenceButtons();
    }

    // REDRAW BUTTONS
    if (dirtySequenceFlag) {
      RedrawSequence();
      dirtySequenceFlag = false;
    }

    //ADVANCE COUNTERS / RESET TIMERS
    bpmCount++;
    playheadCount++;
    previousMillis = currentMillis;
  }
}



void PerformMidiCheck()
{
  //CHECK EVERY BUTTON IN THE SEQUENCE FOR MIDI START / END
  for (int i = 0; i < 16; i++)
  {
    if (PatternsContainer.Patterns[currentPattern].Sequences[i].MusicNotes[playheadCount].EndNote == true) {
      EndMidiNote(PatternsContainer.Patterns[currentPattern].Sequences[i].Pitch);
    }

    if (PatternsContainer.Patterns[currentPattern].Sequences[i].MusicNotes[playheadCount].Play == true) {
      StartMidiNote(PatternsContainer.Patterns[currentPattern].Sequences[i].Pitch,
                    PatternsContainer.Patterns[currentPattern].Sequences[i].MusicNotes[playheadCount].Velocity);
    }
  }
}


void StartMidiNote(int pitch, int velocity)
{
        lcd.setCursor(0,0);
      lcd.print("P:" + (String)pitch + " V:" + (String)velocity);
  MIDI.sendNoteOn(40, 100, 1); // Send a Note
}

void EndMidiNote(int pitch)
{
  //MIDI.sendNoteOff(pitch, 0, 1);  // Stop the note
}




void EvaluateSequenceButtons()
{
  for (int i = 0; i < 16; i++)
  {
    bool buttonState = digitalRead(buttonPins[i]);

    // CHECK THAT BUTTON WAS PRESSED
    if (!buttonState)
    {
      dirtySequenceFlag = true;
      buttonTimerReset = 300;
      SetMidiStartPoint(i);
      
      /*
      if (PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[i].Play == true)
      {
        ClearMidiSequences(i);
      }

      
      if (PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[i].Playing == true)
      {
        SetMidiStartPoint(i);
        int endPoint = SetMidiEndPoint(i);
        SetMidiPlayingPoints(i, endPoint);
      }
      
      else {
        SetMidiStartPoint(i);
        int endPoint = SetMidiEndPoint(i);
        SetMidiPlayingPoints(i, endPoint);
      }*/
    }
    Tester("Z" + String(i));
    digitalWrite(buttonPins[i], HIGH);
  }
}

// CLEAR MIDI SEQUENCES
void ClearMidiSequences(int startPoint)
{
  int currentPosition = startPoint;
  bool endPointFound = false;
  do
  {
    if (PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[currentPosition].EndNote) {
      endPointFound = true;
    }
    PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[currentPosition].Play = false;
    PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[currentPosition].Playing = false;
    PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[currentPosition].EndNote = false;
    currentPosition += 1;
  } while (!endPointFound);
}

void EndPreviousSequenceCollision(int position)
{
  int previousPosition = position - 1;
  if (previousPosition < 0) {
    previousPosition = 15;
  }
  PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[previousPosition].EndNote = true;
}


// SET MIDI SEQUENCES
void SetMidiStartPoint(int startPoint)
{
  PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[startPoint].Play = true;
  PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[startPoint].Playing = true;
  PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[startPoint].EndNote = false;
}

void SetMidiPlayingPoints(int startPoint, int endPoint)
{
  for (int i = startPoint + 1; i != endPoint; i++)
  {
    PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[i].Play = false;
    PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[i].Playing = true;
    PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[i].EndNote = false;
  }
}

int SetMidiEndPoint(int tempPlayheadCount)
{
  int endPoint = 0;
  switch (currentIntervalLength)
  {
    case 16:
      endPoint = tempPlayheadCount + 1;
      break;

    case 8:
      endPoint = tempPlayheadCount + 2;
      break;

    case 4:
      endPoint = tempPlayheadCount + 4;
      break;

    case 2:
      endPoint = tempPlayheadCount + 8;
      break;

    case 1:
      endPoint = tempPlayheadCount + 16;
      break;
  }

  if (endPoint > 16) {
    endPoint -= 16;
  }

  PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[endPoint].Play = false;
  PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[endPoint].Playing = false;
  PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[endPoint].EndNote = true;
  return endPoint;
}




void RedrawSequence()
{
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i + 2, 3);
    if (PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[i].Play) {
      // PRINT BEAT START
      lcd.print((char)1);
    }
    else if (PatternsContainer.Patterns[currentPattern].Sequences[currentSequence].MusicNotes[i].EndNote) {
      // PRINT BEAT END
      //lcd.print("X");
    }
    else {
      // PRINT OPEN BEAT
      if (i == 0 || i == 4 || i == 8 || i == 12) {
        lcd.print((char)3);
      }
      // PRINT SUB BEAT
      else {
        lcd.print((char)2);
      }
    }
  }
}


void Tester(String str)
{
  //lcd.setCursor(1,1);
  //lcd.print(str);
  //delay(500);
}


