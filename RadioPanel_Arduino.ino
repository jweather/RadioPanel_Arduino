/*
 * Jeremy Weatherford <jweather@xidus.net>
 * Developed for PropWash Simulation
 */

// IO pins
// LCD panels via MAX7219
const int pinLCDLoad =    2;   // or CS
const int pinLCDData =    3;
const int pinLCDClock =   4;

const int pinSwitch =     5;
const int nEnc = 2;
const int pinEncA[nEnc] = {6, 8};  // coarse, then fine
const int pinEncB[nEnc] = {7, 9};  // coarse, then fine

const int pinLED =        30;

// state
char freq1[10], freq2[10];

int sw;

// encoder decoding
const int8_t encStates[] = {0,0,1,0,1,0,0,-1,0,0,0,0,0,0,-1,0}; // x2 counting (for PropWash Simulation dual-encoder)
int8_t oldEnc[4] = {0, 0, 0, 0};
int8_t rotaryDelta(byte idx) {
  oldEnc[idx] <<= 2;
  oldEnc[idx] |= (digitalRead(pinEncA[idx])<<1 | digitalRead(pinEncB[idx]));
  return encStates[oldEnc[idx] & 0x0f];
}


// LCD -- should be in a library
int lcdChips = 2;

byte charTable[] = {
  B01111110,B00110000,B01101101,B01111001,B00110011,B01011011,B01011111,B01110000,B01111111,B01111011
};

// define max7219 registers
byte max7219_reg_decodeMode  = 0x09;
byte max7219_reg_intensity   = 0x0a;
byte max7219_reg_scanLimit   = 0x0b;
byte max7219_reg_shutdown    = 0x0c;
byte max7219_reg_displayTest = 0x0f;

// bit bang data in
void putByte(byte data) {
 byte i = 8;
 byte mask;
 while(i > 0) {
   mask = 0x01 << (i - 1);  // get bitmask
   digitalWrite( pinLCDClock, LOW);   // tick
   if (data & mask){        // choose bit
     digitalWrite(pinLCDData, HIGH); // send 1
   }else{
     digitalWrite(pinLCDData, LOW);  // send 0
   }
   digitalWrite(pinLCDClock, HIGH);  // tock
   --i;                     // move to lesser bit
 }
}

void maxPut (byte reg, byte data) {
  putByte(reg);
  putByte(data);
}

// set register in all connected displays
void maxIni (byte reg, byte data) {
 digitalWrite(pinLCDLoad, HIGH);
 for ( int c =1; c<= lcdChips; c++) {
   maxPut(reg, data);   // use all 8 columns
 }
 digitalWrite(pinLCDLoad, LOW);
 digitalWrite(pinLCDLoad,HIGH);
}

void lcdClear() {
  for (int i=1; i<=8; i++) {    // empty registers, turn all LEDs off
    maxIni(i,0);
 }
}

void lcdSetup () {
 pinMode(pinLCDData,  OUTPUT);
 pinMode(pinLCDClock, OUTPUT);
 pinMode(pinLCDLoad,  OUTPUT);
 
 maxIni(max7219_reg_scanLimit, 0x05);   // digits per chip
 maxIni(max7219_reg_decodeMode, 0x00);  // no decode
 maxIni(max7219_reg_shutdown, 0x01);    // not in shutdown mode
 maxIni(max7219_reg_displayTest, 0x00); // no display test
 lcdClear();
 maxIni(max7219_reg_intensity, 0x0f & 0x0f);    // the first 0x0f is the value you can set
}

void writeLCDs(byte r1, byte d1, byte r2, byte d2) {
  digitalWrite(pinLCDLoad,HIGH);  
  maxPut(r1, d1);
  maxPut(r2, d2);
  digitalWrite(pinLCDLoad, LOW);
  digitalWrite(pinLCDLoad,HIGH);
}

void larsonSeg(int i) {
  int chip = 0;
  if (i >= 5) {
    i -= 5;
    chip++;
  }
  i = 6 - i; // 1-based, reversed

  writeLCDs((chip == 0) ? i : 0, 1, (chip == 1) ? i : 0, 1);
  delay(50);

  writeLCDs(i, 0, i, 0);
}

void larson() {
  for (int i=0; i<10; i++) {
    larsonSeg(i);
  }
  for (int i=9; i>=0; i--) {
    larsonSeg(i);
  }
}

byte segmentsForChar(char c) {
  if (c == ' ') return 0;
  if (c >= '0' && c <= '9') return charTable[c-'0'];
  return 0x80;
}

void lcdRefresh() {
  char *p1 = freq1+5, *p2 = freq2+5;
  byte d1, d2;
  
  for (int digit = 1; digit <= 5; digit++) {
    d1 = 0x0; d2 = 0x0;
    if (*p1 == '.') {
      d1 = 0x80;
      p1--;
    }
    if (*p2 == '.') {
      d2 = 0x80;
      p2--;
    }
    
    d1 |= segmentsForChar(*p1--);
    d2 |= segmentsForChar(*p2--);

    writeLCDs(digit, d1, digit, d2);
  }    
}

void setup() {
  pinMode(pinLED, OUTPUT);
  Serial.begin(115200);

  strcpy(freq1, " 88888"); 
  strcpy(freq2, " 88888");

  lcdSetup();

  pinMode(pinSwitch, INPUT_PULLUP);
  sw = digitalRead(pinSwitch);

  for (int i=0; i<nEnc; i++) {
    pinMode(pinEncA[i], INPUT_PULLUP);
    pinMode(pinEncB[i], INPUT_PULLUP);
  }
}

// encoders
#define DIR_CCW 0x10
#define DIR_CW 0x20
#define R_START 0x0

#define R_CCW_BEGIN 0x1
#define R_CW_BEGIN 0x2
#define R_START_M 0x3
#define R_CW_BEGIN_M 0x4
#define R_CCW_BEGIN_M 0x5
const byte ttable[6][4] = {
  {R_START_M,            R_CW_BEGIN,     R_CCW_BEGIN,  R_START},
  {R_START_M | DIR_CCW, R_START,        R_CCW_BEGIN,  R_START},
  {R_START_M | DIR_CW,  R_CW_BEGIN,     R_START,      R_START},
  {R_START_M,            R_CCW_BEGIN_M,  R_CW_BEGIN_M, R_START},
  {R_START_M,            R_START_M,      R_CW_BEGIN_M, R_START | DIR_CW},
  {R_START_M,            R_CCW_BEGIN_M,  R_START_M,    R_START | DIR_CCW},
};

byte state[nEnc];

byte encoderDelta(int _i) {
  byte pinstate = (digitalRead(pinEncA[_i]) << 1) | digitalRead(pinEncB[_i]);
  state[_i] = ttable[state[_i] & 0xf][pinstate];
  return (state[_i] & 0x30); // click?
}

int blink;
int lastBlink = 0;
int swDebounce = 0;
char buf[20];

void loop() {
  uint32_t val;

  // time-keeping and blink
  int now = millis();
  if (now - lastBlink > 1000) {
    digitalWrite(pinLED, blink);
    blink = !blink;
    lastBlink = now;
    lcdRefresh();
    Serial.write("id=1\r");
  }

 // rotary encoders
  for (int enc=0; enc<nEnc; enc++) {
    byte delta = encoderDelta(enc);
    
    if (delta == DIR_CW) {
      if (enc % 2 == 0) {
        Serial.write("+10\r");
      } else {
        Serial.write("+1\r");
      }
    } else if (delta == DIR_CCW) {
      if (enc % 2 == 0) {
        Serial.write("-10\r");
      } else {
        Serial.write("-1\r");
      }
    }
  }

  // switch
  val = digitalRead(pinSwitch);
  if (sw == 1 && val == 0) {
    if (swDebounce == 0 || now - swDebounce > 200) {
      Serial.write("switch\r");
      swDebounce = now;
    }
  }
  sw = val;
  if (swDebounce != 0 && now - swDebounce > 200)
    swDebounce = 0;

  // =A123.45\r or =B  1234\r, always 6 characters
  if (Serial.available() > 6) {
    while (Serial.read() != '=' && Serial.available()) {}
    char ch = Serial.read();
    val = Serial.readBytesUntil('\r', buf, 20);

    if (val == 6) {
      switch (ch) {
        case 'A': strcpy(freq1, buf); lcdRefresh(); break;
        case 'B': strcpy(freq2, buf); lcdRefresh(); break;
        break;
      }
    }
  }
}
