/*
 * HID RFID Wiegand Interface for ESP32
 * Based on @dc540_nova's Github repository for basic Arduino interface
 * https://github.com/dc540/arduinohidprox
 * Modified by @T3ch404, 6/23/2023 
 * 
 * Version 0.1 adds a card info structure along with an allow list in order to make adding new 
 * allowed users/cards easier and scalable.
*/

// ---------- Setup Libraries ----------
#include <FastLED.h>

// ---------- Setup global variables ----------
// FastLED
#define NUM_LEDS 1                   // number of status LEDs
#define DATA_PIN 4                   // data pin to controll status LEDs
CRGB leds[NUM_LEDS];                 // FastLED array

// Weigand/HID
#define DATA0 16                     // DATA0 connected to pin 16
#define DATA1 13                     // DATA1 connected to pin 13
#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse.  
unsigned char databits[MAX_BITS];    // stores all of the data bits
unsigned char bitCount;              // number of bits currently captured
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits
unsigned long facilityCode=0;        // decoded facility code
unsigned long cardCode=0;            // decoded card code
unsigned long keyPress=0;
char userName[10] = "         ";

// GPIO/Relays
int RelayA = 32;
int RelayB = 33;

// interrupt that happens when DATA0 goes low (0 bit)
void ISR_INT0() {
  //Serial.print("0");   // uncomment this line to display raw binary (THIS DOES NOT WORK WITH ESP32)
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;  
}
 
// interrupt that happens when DATA1 goes low (1 bit)
void ISR_INT1()
{
  //Serial.print("1");   // uncomment this line to display raw binary (THIS DOES NOT WORK WITH ESP32)
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;  
}

// this function runs when a card is scanned to check if the facilityCode/cardCode should be allowed access
void accessCheck()
{
      // Serial logging
      Serial.print("FC = ");
      Serial.print(facilityCode);
      Serial.print(", CC = ");
      Serial.println(cardCode); 

      // TODO: This is a simple single card check. This should reference a library variable or query a server.
      if ((facilityCode == 123) && (cardCode == 123123)) 
      {
        accessGranted();
      } else
        accessDenied();
}

// this function is run if a card has been scanned that has permissions to access
void accessGranted(void) {
  // Serial logging
  Serial.print("Access Granted!\n");
  Serial.print("FC: ");
  Serial.print(facilityCode);
  Serial.print("\nCC: ");
  Serial.print(cardCode);

  // Power RelayA and flash the status LED Green
  digitalWrite(RelayA, LOW);
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(500);
  digitalWrite(RelayA, HIGH);
  leds[0] = CRGB::Blue;
  FastLED.show();
  delay(500);
}

// this function is run if a card has been scanned that does not have permission to access
void accessDenied(void) {
  // Serial logging
  Serial.println("Unknown card. Access denied!");
  Serial.print("FC: ");
  Serial.println(facilityCode);
  Serial.print("CC: ");
  Serial.println(cardCode);

  // flash the status LED Red
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(500);
  leds[0] = CRGB::Blue;
  FastLED.show();
}

void setup()
{
  // Serial setup
  Serial.begin(9600);
  Serial.println("Entering setup...");

  // Status LED setup
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
  FastLED.setBrightness(5);
  leds[0] = CRGB::Red;
  FastLED.show();

  // GPIO Relay setup
  pinMode(RelayA, OUTPUT);
  digitalWrite(RelayA, HIGH);

  Serial.println("\n\nRFID Readers");

  // Wiegand pin setup
  pinMode(DATA0, INPUT_PULLUP);
  pinMode(DATA1, INPUT_PULLUP);
  attachInterrupt(DATA0, ISR_INT0, FALLING);  // binds the ISR functions to the falling edge of DATA0 and DATA1
  attachInterrupt(DATA1, ISR_INT1, FALLING);
  weigand_counter = WEIGAND_WAIT_TIME;

  // Log that setup is complete
  delay(2000);
  Serial.print("Leaving setup\n");
  leds[0] = CRGB::Blue;
  FastLED.show();
}
 
void loop()
{
  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {
    if (--weigand_counter == 0)
      flagDone = 1;  
  }
 
  // if we have bits and we the weigand counter went out
  if (bitCount > 0 && flagDone) {
    unsigned char i;
 
    // Serial logging
    Serial.print("Read ");
    Serial.print(bitCount);
    Serial.println(" bits");
 
    // we will decode the bits differently depending on how many bits we have
    // see www.pagemac.com/azure/data_formats.php for mor info
    if (bitCount == 34)
    {
      // 35 bit HID Corporate 1000 format
      // facility code = bits 2 to 14
      for (i=2; i<17; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
 
      // card code = bits 15 to 34
      for (i=14; i<33; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }
 
      accessCheck();
    }
    else if (bitCount == 26)
    {
      // standard 26 bit format
      // facility code = bits 2 to 9
      for (i=1; i<9; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
 
      // card code = bits 10 to 23
      for (i=9; i<25; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }
 
      accessCheck();  
    }
      // https://www.ooaccess.com/kb/37-bit-fc/#:~:text=The%20HID%2037%20bit%20Wiegand,19%20bit%20Cardholder%20ID%20fields.2
    else if (bitCount == 37)
    {
      for (i=1; i<=16; i++) {
        facilityCode <<=1;
        facilityCode |= databits[i];
      }

      for (i=17;i<=35;i++) {
        cardCode <<=1;
        cardCode |= databits[i];
      }
      accessCheck();
    }
 
     // cleanup and get ready for the next card
     bitCount = 0;
     facilityCode = 0;
     cardCode = 0;
     keyPress = 0;
     for (i=0; i<MAX_BITS; i++) 
     {
       databits[i] = 0;
     }
  }
}