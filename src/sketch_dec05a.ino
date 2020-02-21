#include <Arduino_LSM9DS1.h>
#include <PDM.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include "hsv.h"

#ifdef __AVR__
  #include <avr/power.h>
#endif
#define PIN 2
#define NUMPIXELS 16
#define DELAYVAL 500
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// buffer to read samples into, each sample is 16-bits
short sampleBuffer[256];
int maxValSeen = 1900;
int availableRange = 255 * 8; //8 pixels with 255 
// number of samples read
volatile int samplesRead;
long vuLastRender = 0;
const int VU_RENDER_INTERVALL = 60;

const int MODE_VU_METER = 1;
const int MODE_RAINBOW = 2;
const int MODE_FADE = 3;
const int MODE_CIRCLE_ROT = 4;
const int MODE_FALL_DOWN = 5;
const int MODE_CHASER = 6;
const int MODE_FLASH = 7;
const long MODE_LENGTH[]={1000*30, 1000, 500, 5000, 10000, 100, 400};

int mode = 1;
long modeLength = 0;
long lastModeChange = 0;

int step = 0;
// max Hue
#define MAXHUE 256*6

int position = 0;
int hue = 0;

int colorStep[] = {255, 255, 255};

void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);

  //while (!Serial);
  Serial.println("Started");
 /*if (!IMU.begin()) {
    Serial.println("Failed to initialized IMU!");
    while (1);
  }*/
  randomSeed(analogRead(0));
  // configure the data receive callback
  PDM.onReceive(onPDMdata);

  // initialize PDM with:
  // - one channel (mono mode)
  // - a 16 kHz sample rate
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }

  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
  #endif

  pixels.begin();
}

void loop() {
  long now = millis();
  switch (mode) {
    case MODE_VU_METER:
      renderVU();
    break;
    case MODE_RAINBOW:
      rainbow(random(4,10));
    break;
    case MODE_FADE:
      PlasmaPulse(random(10,20));
    break;
    case MODE_CIRCLE_ROT:
      renderCircleRot();      
    break;
    case MODE_FALL_DOWN:
      fallDown();      
    break;
    case MODE_CHASER:
      theaterChaseRainbow(100);      
    break;
    case MODE_FLASH:
      flash(); 
    break;


  }
  if(now > lastModeChange + modeLength) {
    int lastMode = mode;
    if(lastMode != 1)
      mode = 1;
    else {
      while(lastMode == mode)
        mode = random(1, 8);
    }
      
    lastModeChange = millis();
    modeLength = MODE_LENGTH[mode-1];
    step=0;
    Serial.printf("mode changed to %d modeLength %d\n" ,mode, modeLength);
  }
  
  
  // put your main code here, to run repeatedly:
  /*float x, y, z;  
  IMU.readAcceleration(x, y, z);
  
  Serial.printf("x: %.2f y: %.2f z: %.2f \n",x,y,z);
  if(x < 0)
    x = -x;
  float RValue = x * 255;
  Serial.printf("RV: %.2f\n",RValue);
  //map -1 to 1 to 0 and 255
  int RGB[]={round(RValue),0,0};
  setLedValue(RGB);
  delay(20);*/

  
  /*for(int i=0; i<NUMPIXELS; i++) {

    pixels.setPixelColor(i, pixels.Color(0, 150, 0));
    pixels.show();
    delay(DELAYVAL);
    Serial.println("light pixel "+i);
  }
  pixels.clear();*/
  //delay(1000);
  
  
  //delay(1000);
  //rgbFadeInAndOut(0, 0, 255, 20); // Blue
}

void setLedValue(int RGB[]) {
  int ledPins[] = {LEDR, LEDG, LEDB};
  // RGB LED's are pulled up, so the PWM needs to be inverted
  for(int i = 0; i < 3; i++) {
    int value = RGB[i];
    if (value == 0) {
      // special hack to clear LED
      analogWrite(ledPins[i], 256);
    } else {
      analogWrite(ledPins[i], 255 - value);
    }
  }
}


void renderVU() {
  long now = millis();
  if (samplesRead) {    
    // print samples to the serial monitor or plotter
    for (int i = 0; i < samplesRead; i++) {
      if(now > VU_RENDER_INTERVALL + vuLastRender ) {
        //Serial.println("update");
        vuLastRender = now;        
        short sample = sampleBuffer[i];
        if(sample < 0)
          sample = -sample; //only positive values pls
        //try log scale
        if(sample > maxValSeen)
          sample = maxValSeen;        
        sample = (sample / 2)-30;
        if(sample < 0)
          sample = 0;
        sample = log(sample) / log(1.004);       
        pixels.clear();
        
        //calculate value range fakto
        float factor = (float)availableRange / (float)maxValSeen;
        int convertedSample = round(sample * factor);
        //Serial.printf("sample: %d factor: %.2f conv: %d\n", sample, factor, convertedSample);
       
        if(convertedSample > 30) {
          if(convertedSample > availableRange)
            convertedSample = availableRange;
          //how many pixels are fully on
          int fullPix = (int)(convertedSample / 255)-1;
          int off = calcPixOffSet(fullPix);
          //Serial.printf("fullpix: %d Offset: %d\n",fullPix, off);
          int lastPixVal = convertedSample % 255;
          if(lastPixVal == 0)
            lastPixVal = 255;
          for(int i = 0; i < fullPix; i++) {
            pixels.setPixelColor(calcPixOffSet(i), pixels.Color(255, 255, 255));
            pixels.setPixelColor(calcPixOffSet2nd(i), pixels.Color(255, 255, 255));
          }
          pixels.setPixelColor(calcPixOffSet(fullPix), pixels.Color(lastPixVal, 0, 0));  
          pixels.setPixelColor(calcPixOffSet2nd(fullPix), pixels.Color(lastPixVal, 0, 0));
        }
        pixels.show();  
      }
    }
    // clear the read count
    samplesRead = 0;
  }
}

void flash() {
  Serial.printf("step %d \n",step);
  if(step == 0) {
    pixels.clear();
    step = 1;
  }
  else {
    for(int i = 0 ;i < 8; i++) {
      pixels.setPixelColor(calcPixOffSet(i), pixels.Color(255, 255, 255));
      pixels.setPixelColor(calcPixOffSet2nd(i), pixels.Color(255, 255, 255));      
    }
    step = 0;
  }
  pixels.show();  
  delay(40);
  
}
void fallDown() {
  Serial.printf("step %d \n",step);
  switch (step) {
    case 0:
      pixels.clear();
      pixels.setPixelColor(calcPixOffSet(4), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet2nd(4), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
    break;
    case 1:
      pixels.setPixelColor(calcPixOffSet(3), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet(5), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet2nd(3), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet2nd(5), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
    break;
    case 2:
      pixels.setPixelColor(calcPixOffSet(2), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet(6), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet2nd(2), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet2nd(6), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
    break;
    case 3:
      pixels.setPixelColor(calcPixOffSet(1), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet(7), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet2nd(1), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
      pixels.setPixelColor(calcPixOffSet2nd(7), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));
    break;
    case 4:
      pixels.setPixelColor(calcPixOffSet(0), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));      
      pixels.setPixelColor(calcPixOffSet2nd(0), pixels.Color(colorStep[0],colorStep[1],colorStep[2]));      
    break;
    case 5:
      pixels.setPixelColor(calcPixOffSet(4), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet2nd(4), pixels.Color(0,0,0));      
    break;
    case 6:
      pixels.setPixelColor(calcPixOffSet(3), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet(5), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet2nd(3), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet2nd(5), pixels.Color(0,0,0));
    break;
    case 7:
      pixels.setPixelColor(calcPixOffSet(2), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet(6), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet2nd(2), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet2nd(6), pixels.Color(0,0,0));
    break;
    case 8:
      pixels.setPixelColor(calcPixOffSet(1), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet(7), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet2nd(1), pixels.Color(0,0,0));
      pixels.setPixelColor(calcPixOffSet2nd(7), pixels.Color(0,0,0));
    break;
    case 9:
      pixels.setPixelColor(calcPixOffSet2nd(0), pixels.Color(0,0,0));      
      pixels.setPixelColor(calcPixOffSet(0), pixels.Color(0,0,0));      
    break;

  }
  pixels.show();
  step++;
  if(step==9) {
      colorStep[0]=random(40,180);
      colorStep[1]=random(0,120);
      colorStep[2]=random(0,255);
      step = 0;
  }
  delay(90);
}


int calcPixOffSet(int pix) {
 int newpix = pix + 6;
 if(newpix > 7)
    newpix = newpix - 8;
 return newpix;
}
int calcPixOffSet2nd(int pix) {
 int newpix = pix + 2;
 if(newpix > 7)
    newpix = newpix - 8;
 return newpix+8;
}

void renderCircleRot() {
  for (int i = 0; i < NUMPIXELS; i++)
    pixels.setPixelColor((i + position) % NUMPIXELS, getPixelColorHsv(i, hue, 255, pixels.gamma8(i * (255 / NUMPIXELS))));
  pixels.show();
  position++;
  position %= NUMPIXELS;
  hue += 12;
  hue %= MAXHUE;
  delay(50);
}


// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait)
{
    int firstPixelHue = 0; // First pixel starts at red (hue 0)
    for (int a = 0; a < 10; a++)
    { // Repeat 30 times...
        for (int b = 0; b < 3; b++)
        {                  //  'b' counts from 0 to 2...
            pixels.clear(); //   Set all pixels in RAM to 0 (off)
            // 'c' counts up from 'b' to end of strip in increments of 3...
            for (int c = b; c < pixels.numPixels(); c += 3)
            {
                // hue of pixel 'c' is offset by an amount to make one full
                // revolution of the color wheel (range 65536) along the length
                // of the strip (strip.numPixels() steps):
                int hue = firstPixelHue + c * 65536L / pixels.numPixels();
                uint32_t color = pixels.gamma32(pixels.ColorHSV(hue)); // hue -> RGB
                pixels.setPixelColor(c, color);                       // Set pixel 'c' to value 'color'
            }
            pixels.show();                // Update strip with new contents
            delay(wait);                 // Pause for a moment
            firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
        }
    }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait)
{
    // Hue of first pixel runs 5 complete loops through the color wheel.
    // Color wheel has a range of 65536 but it's OK if we roll over, so
    // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
    // means we'll make 5*65536/256 = 1280 passes through this outer loop:
    for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256)
    {
        for (int i = 0; i < pixels.numPixels(); i++)
        { // For each pixel in strip...
            // Offset pixel hue by an amount to make one full revolution of the
            // color wheel (range of 65536) along the length of the strip
            // (strip.numPixels() steps):
            int pixelHue = firstPixelHue + (i * 65536L / pixels.numPixels());
            // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
            // optionally add saturation and value (brightness) (each 0 to 255).
            // Here we're using just the single-argument hue variant. The result
            // is passed through strip.gamma32() to provide 'truer' colors
            // before assigning to each pixel:
            pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
        }
        pixels.show(); // Update strip with new contents
        delay(wait);  // Pause for a moment
    }
}


void PlasmaPulse(uint8_t wait) {
  uint16_t i, j;
  uint8_t brightness = 255;

  for(i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  }
  pixels.show();
  delay(wait);
  //Adjust 60 and 90 to the starting and ending colors you want to fade between. 
  for(j=170; j>=135; --j) {
    for(i=0; i<pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel((i+j) & 255));
    }
    pixels.show();
    brightness -= 6;
    pixels.setBrightness(brightness);
    delay(wait);
  }

  for(j=135; j<1170; j++) {
    for(i=0; i<pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel((i+j) & 255));
    }
    pixels.show();
    brightness += 6;
    pixels.setBrightness(brightness);
    delay(wait);
  }
  
  for(i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  }
  pixels.show();
  delay(wait);

}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}
void rgbFadeInAndOut(uint8_t red, uint8_t green, uint8_t blue, uint8_t wait) {
  for(uint8_t b = 0; b <255; b++) {
     for(uint8_t i=0; i < pixels.numPixels(); i++) {
        pixels.setPixelColor(i, red * b/255, green * b/255, blue * b/255);
     }

     pixels.show();
     delay(wait);
  };

  for(uint8_t b=255; b > 0; b--) {
     for(uint8_t i = 0; i < pixels.numPixels(); i++) {
        pixels.setPixelColor(i, red * b/255, green * b/255, blue * b/255);
     }
     pixels.show();
     delay(wait);
  };
};
void onPDMdata() {
  // query the number of bytes available
  int bytesAvailable = PDM.available();

  // read into the sample buffer
  PDM.read(sampleBuffer, bytesAvailable);

  // 16-bit, 2 bytes per sample
  samplesRead = bytesAvailable / 2;
}
