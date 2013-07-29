#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Adafruit_NeoPixel.h>
#include "Flora_Pianoglove.h"
#include <SoftwareSerial.h>

// define the pins used
#define VS1053_RX  10 // This is the pin that connects to the RX pin on VS1053
#define VS1053_RESET 9 // This is the pin that connects to the RESET pin on VS1053
// Don't forget to connect the GPIO #0 to GROUND and GPIO #1 pin to 3.3V

// See http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf Pg 31
#define VS1053_BANK_DEFAULT 0x00
#define VS1053_BANK_DRUMS1 0x78
#define VS1053_BANK_DRUMS2 0x7F
#define VS1053_BANK_MELODY 0x79

// See http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf Pg 32 for more!
#define VS1053_GM1_OCARINA 80
#define VS1053_GM1_SQUARELEAD 81
#define VS1053_GM1_SAWLEAD 82
#define VS1053_GM1_CALLIOPELEAD 83
#define VS1053_GM1_CHIFFLEAD 84
#define VS1053_GM1_CHARANGLEAD 85
#define VS1053_GM1_VOICELEAD 86
#define VS1053_GM1_FIFTHSLEAD 87
#define VS1053_GM1_BASSANDLEAD 88
#define MIDI_NOTE_ON  0x90
#define MIDI_NOTE_OFF 0x80
#define MIDI_CHAN_MSG 0xB0
#define MIDI_CHAN_BANK 0x00
#define MIDI_CHAN_VOLUME 0x07
#define MIDI_CHAN_PROGRAM 0xC0

// we only play a note when the clear response is higher than a certain number 
#define CLEARTHRESHHOLD 2000
#define LOWTONE 1000
#define HIGHTONE 2000
#define LOWKEY 64   // high C
#define HIGHKEY 76  // double high C

// our RGB -> eye-recognized gamma color
byte gammatable[256];

int prevNote = -1;

// color sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
// one pixel on pin 6
Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, 6, NEO_GRB + NEO_KHZ800);

SoftwareSerial VS1053_MIDI(0, VS1053_RX); // TX only, do not use the 'rx' side

void setup() {
  
  Serial.begin(9600);
  Serial.println("Piano Glove MIDI!");

  //Check for color sensor
  if (tcs.begin()) {
    Serial.println("Found sensor");
  } 
  else {
    Serial.println("No TCS34725 found ... check your connections");
    while (1); // halt!
  }

  //init neopixel for color 'playback'
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  //Start MIDI
  VS1053_MIDI.begin(31250); // MIDI uses a 'strange baud rate'
  pinMode(VS1053_RESET, OUTPUT);
  digitalWrite(VS1053_RESET, LOW);
  delay(10);
  digitalWrite(VS1053_RESET, HIGH);
  delay(10);
  midiSetChannelBank(0, VS1053_BANK_MELODY);
  midiSetInstrument(0, VS1053_GM1_BASSANDLEAD);
  midiSetChannelVolume(0, 127);

  // thanks PhilB for this gamma table!
  // it helps convert RGB colors to what humans see
  for (int i=0; i<256; i++) {
    float x = i;
    x /= 255;
    x = pow(x, 2.5);
    x *= 255;

    gammatable[i] = x;      

  }
}


void loop() {
  uint16_t clear, red, green, blue;

  tcs.setInterrupt(false);      // turn on LED

  delay(60);  // takes 50ms to read 

  tcs.getRawData(&red, &green, &blue, &clear);

  tcs.setInterrupt(true);  // turn off LED

  // not close enough to colorful item
  if (clear < CLEARTHRESHHOLD) {
    playNote(-1);
    strip.setPixelColor(0, strip.Color(0, 0, 0)); // turn off the LED
    strip.show();
    return;
  }

  //  Serial.print("C:\t"); Serial.print(clear);
  //  Serial.print("\tR:\t"); Serial.print(red);
  //  Serial.print("\tG:\t"); Serial.print(green);
  //  Serial.print("\tB:\t"); Serial.print(blue);

  // Figure out some basic hex code for visualization
  uint32_t sum = red;
  sum += green;
  sum += blue;
  sum = clear;
  float r, g, b;
  r = red; 
  r /= sum;
  g = green; 
  g /= sum;
  b = blue; 
  b /= sum;
  r *= 256; 
  g *= 256; 
  b *= 256;
  if (r > 255) r = 255;
  if (g > 255) g = 255;
  if (b > 255) b = 255;

  //  Serial.print("\t");
  //  Serial.print((int)r, HEX); Serial.print((int)g, HEX); Serial.print((int)b, HEX); 
  //  Serial.println();


  // OK we have to find the two primary colors
  // check if blue is smallest. MEME: fix for 'white'
  float remove, normalize;
  if ((b < g) && (b < r)) {
    remove = b;
    normalize = max(r-b, g-b);
  } 
  else if ((g < b) && (g < r)) {
    remove = g;
    normalize = max(r-g, b-g);
  } 
  else {
    remove = r;
    normalize = max(b-r, g-r);
  }
  // get rid of minority report
  float rednorm = r - remove;
  float greennorm = g - remove;
  float bluenorm = b - remove;
  // now normalize for the highest number
  rednorm /= normalize;
  greennorm /= normalize;
  bluenorm /= normalize;

  //  Serial.println();
  strip.setPixelColor(0, strip.Color(gammatable[(int)r], gammatable[(int)g], gammatable[(int)b]));
  strip.show();

  //  Serial.print(rednorm); Serial.print(", "); 
  //  Serial.print(greennorm); Serial.print(", "); 
  //  Serial.print(bluenorm); Serial.print(" "); 
  //  Serial.println();

  float rainbowtone = 0;

  if (bluenorm <= 0.1) {
    // between red and green
    if (rednorm >= 0.99) {
      // between red and yellow
      rainbowtone = 0 + 0.2 * greennorm;
    } 
    else {
      // between yellow and green
      rainbowtone = 0.2 + 0.2 * (1.0 - rednorm);
    }
  } 
  else if (rednorm <= 0.1) {
    // between green and blue
    if (greennorm >= 0.99) {
      // between green and teal
      rainbowtone = 0.4 + 0.2 * bluenorm;
    } 
    else {
      // between teal and blue
      rainbowtone = 0.6 + 0.2 * (1.0 - greennorm);
    }
  } 
  else {
    // between blue and violet
    if (bluenorm >= 0.99) {
      // between blue and violet
      rainbowtone = 0.8 + 0.2 * rednorm;
    } 
    else {
      // between teal and blue
      rainbowtone = 0; 
    }
  }

  //  Serial.print("Scalar "); Serial.println(rainbowtone);
  float keynum = LOWKEY + (HIGHKEY - LOWKEY) * rainbowtone;
  //  Serial.print("Key #"); Serial.println(keynum);
  //  float freq = pow(2, (keynum - 49) / 12.0) * 440;
  //  Serial.print("Freq = "); Serial.println(freq);  
  //Serial.print((int)r ); Serial.print(" "); Serial.print((int)g);Serial.print(" ");  Serial.println((int)b );
  playNote(keynum);
}


void playNote(uint16_t n) {
  
  if (n == prevNote) return;
  
  if (n == -1){
    midiNoteOff(0, prevNote, 127);
  }
  
  //stop last note played
  midiNoteOff(0, prevNote, 127);
  
  //play new note
  midiNoteOn(0, n, 127);
  
  prevNote = n;
}


RgbColor HsvToRgb(HsvColor hsv){
  
  RgbColor rgb;
  unsigned char region, remainder, p, q, t;

  if (hsv.s == 0)
  {
    rgb.r = hsv.v;
    rgb.g = hsv.v;
    rgb.b = hsv.v;
    return rgb;
  }

  region = hsv.h / 43;
  remainder = (hsv.h - (region * 43)) * 6; 

  p = (hsv.v * (255 - hsv.s)) >> 8;
  q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
  t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

  switch (region)
  {
  case 0:
    rgb.r = hsv.v; 
    rgb.g = t; 
    rgb.b = p;
    break;
  case 1:
    rgb.r = q; 
    rgb.g = hsv.v; 
    rgb.b = p;
    break;
  case 2:
    rgb.r = p; 
    rgb.g = hsv.v; 
    rgb.b = t;
    break;
  case 3:
    rgb.r = p; 
    rgb.g = q; 
    rgb.b = hsv.v;
    break;
  case 4:
    rgb.r = t; 
    rgb.g = p; 
    rgb.b = hsv.v;
    break;
  default:
    rgb.r = hsv.v; 
    rgb.g = p; 
    rgb.b = q;
    break;
  }

  return rgb;
}


HsvColor RgbToHsv(RgbColor rgb){
  
  HsvColor hsv;
  unsigned char rgbMin, rgbMax;

  rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : 
  (rgb.g < rgb.b ? rgb.g : rgb.b);
  rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : 
  (rgb.g > rgb.b ? rgb.g : rgb.b);

  hsv.v = rgbMax;
  if (hsv.v == 0)
  {
    hsv.h = 0;
    hsv.s = 0;
    return hsv;
  }

  hsv.s = 255 * long(rgbMax - rgbMin) / hsv.v;
  if (hsv.s == 0)
  {
    hsv.h = 0;
    return hsv;
  }

  if (rgbMax == rgb.r)
    hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
  else if (rgbMax == rgb.g)
    hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
  else
    hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

  return hsv;
}


void midiSetInstrument(uint8_t chan, uint8_t inst) {
  if (chan > 15) return;
  inst --; // page 32 has instruments starting with 1 not 0 :(
  if (inst > 127) return;
  
  VS1053_MIDI.write(MIDI_CHAN_PROGRAM | chan);  
  VS1053_MIDI.write(inst);
}


void midiSetChannelVolume(uint8_t chan, uint8_t vol) {
  if (chan > 15) return;
  if (vol > 127) return;
  
  VS1053_MIDI.write(MIDI_CHAN_MSG | chan);
  VS1053_MIDI.write(MIDI_CHAN_VOLUME);
  VS1053_MIDI.write(vol);
}

void midiSetChannelBank(uint8_t chan, uint8_t bank) {
  if (chan > 15) return;
  if (bank > 127) return;
  
  VS1053_MIDI.write(MIDI_CHAN_MSG | chan);
  VS1053_MIDI.write((uint8_t)MIDI_CHAN_BANK);
  VS1053_MIDI.write(bank);
}

void midiNoteOn(uint8_t chan, uint8_t n, uint8_t vel) {
  if (chan > 15) return;
  if (n > 127) return;
  if (vel > 127) return;
  
  VS1053_MIDI.write(MIDI_NOTE_ON);
  VS1053_MIDI.write(n);
  VS1053_MIDI.write(vel);
}

void midiNoteOff(uint8_t chan, uint8_t n, uint8_t vel) {
  if (chan > 15) return;
  if (n > 127) return;
  if (vel > 127) return;
  
  VS1053_MIDI.write(MIDI_NOTE_OFF | chan);
  VS1053_MIDI.write(n);
  VS1053_MIDI.write(vel);
}

