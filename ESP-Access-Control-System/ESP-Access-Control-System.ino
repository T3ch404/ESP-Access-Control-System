/*
 * HID RFID Wiegand Access Control System using ESP32
 * Created by @T3ch404, 6/24/2023
 * Wiegand protocol handling based on @dc540_nova's Github repo https://github.com/dc540/arduinohidprox
 * 
 * Version 0.2 adds a basic HTTP auth protected web interface.
 * This interface allows users to add, modify, and/or remove cards from the allow list.
 * 
 * SECURITY NOTE: HTTPS is not included in this program. This means all connections are sent and recieved in clear-text. 
 *                Make sure to keep this device on a separate and secure network segment to prevent network monitoring/plain-text password scraping.
 * 
 * TODO Items:
 *    Review/standardize comments and variable names
 *    HTTPS? - It looks like as of writing this there is no good option for HTTPS on the ESP32. 
 *             There are options but they do not allow the use of forms and limit to single connections
 *    Add logging to SD
 *    Add learn card functionality
*/

// ---------- Setup Libraries ----------
#include <FastLED.h>
#include <WiFi.h>
#include <ESPAsyncWebSrv.h>

// ---------- Setup global variables ----------
// Web interface
const char* ssid = "WiFi_SSID";
const char* password = "WiFi_Password";

const char* httpUsername = "admin";
const char* httpPassword = "password123";

AsyncWebServer server(80);

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

// GPIO/Relays
int RelayA = 32;
int RelayB = 33;

// Structure to hold card and card holder information
struct cardInfo {
  int id;
  String cardHolderName;
  unsigned long facilityCode;
  unsigned long cardCode;
};

// Allow list of cards and cardholder info
const int MAX_ALLOWED_CARDS = 99;
cardInfo allowedCards[MAX_ALLOWED_CARDS] = {
  {1, "T3ch404", 123, 123123}          // id, Name, Facility Code, Card Code
  // Add more pre-defined cards here
};

// interrupt that happens when DATA0 goes low (0 bit)
void ISR_INT0() {
  //Serial.print("0");   // uncomment this line to display raw binary (THIS DOES NOT WORK WITH ESP32)
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;  
}
 
// interrupt that happens when DATA1 goes low (1 bit)
void ISR_INT1() {
  //Serial.print("1");   // uncomment this line to display raw binary (THIS DOES NOT WORK WITH ESP32)
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;  
}

// this function runs when a card is scanned to check if the facilityCode/cardCode should be allowed access
void accessCheck() {
  // Serial logging
  Serial.print("FC = ");
  Serial.print(facilityCode);
  Serial.print(", CC = ");
  Serial.println(cardCode); 

  // If a null card is read, deny access
  if (facilityCode == 0 || cardCode == 0) {
    accessDenied();
    return;
  }

  // Check if the scanned card is in the allowedCards list
  for (int i = 0; i < MAX_ALLOWED_CARDS; i++) {
    if (facilityCode == allowedCards[i].facilityCode && cardCode == allowedCards[i].cardCode) {
      accessGranted();
      return;
    }
  }
  accessDenied();
  return;
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

bool authenticate(AsyncWebServerRequest* request) {
    if (!request->authenticate(httpUsername, httpPassword))
      return false;
    return true;
  }

void setup() {
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

  // WiFi setup
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Route handler for the home page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!authenticate(request))
      return request->requestAuthentication("Restricted Area");

    String html = "<h1>Welcome to the web interface!</h1>";
    
    // Display the list of allowed cards
    html += "<h2>Allowed Cards:</h2>";
    html += "<ul>";
    for (int i = 0; i < MAX_ALLOWED_CARDS; i++) {
      if (allowedCards[i].cardCode != 0) {
        html += "<li>";
        html += "ID: " + String(allowedCards[i].id) + ", ";
        html += "Name: " + String(allowedCards[i].cardHolderName) + ", ";
        html += "Facility Code: " + String(allowedCards[i].facilityCode) + ", ";
        html += "Card Code: " + String(allowedCards[i].cardCode);
        html += "</li>";
      }
    }
    html += "</ul>";

    // Add a form to add or modify a card
    html += "<h2>Add/Modify Card:</h2>";
    html += "<form method='POST' action='/addcard'>";
    html += "ID: <input type='number' name='id'><br>";
    html += "Name: <input type='text' name='name'><br>";
    html += "Facility Code: <input type='number' name='facilityCode'><br>";
    html += "Card Code: <input type='number' name='cardCode'><br>";
    html += "<input type='submit' value='Add/Modify Card'>";
    html += "</form>";

    // Add a form to remove a card
    html += "<h2>Remove Card:</h2>";
    html += "<form method='POST' action='/removecard'>";
    html += "ID: <input type='number' name='id'><br>";
    html += "<input type='submit' value='Remove Card'>";
    html += "</form>";

    request->send(200, "text/html", html);
  });

  // Route handler for the /addcard POST request. This allows users to add entries in the allowedCards list
  server.on("/addcard", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!authenticate(request))
      return request->requestAuthentication("Restricted Area");

    // Retrieve the card details from the form data
    String id = request->arg("id");
    String name = request->arg("name");
    String facilityCode = request->arg("facilityCode");
    String cardCode = request->arg("cardCode");

    // Convert String inputs to appropriate types
    int cardId = id.toInt();
    unsigned long fc = facilityCode.toInt();
    unsigned long cc = cardCode.toInt();

    // Check if the card ID is within range
    if ((id.toInt() < 0) || (id.toInt() > 99)) {
      request->send(400, "text/plain", "Card ID out of index range");
      return;
    }

    // Create a new cardInfo struct with the provided details
    cardInfo newCard;
    newCard.id = cardId;
    newCard.cardHolderName = name;
    newCard.facilityCode = fc;
    newCard.cardCode = cc;

    // Add the new card to the allowedCards list
    allowedCards[cardId] = newCard;
    //MAX_ALLOWED_CARDS++;

    // Redirect back to the main page
    request->redirect("/");
  });

  // Route handler for removing a card from the allowedCards list
  server.on("/removecard", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!authenticate(request))
      return request->requestAuthentication("Restricted Area");

    // Retrieve the card ID from the form data
    String id = request->arg("id");
    int cardId = id.toInt();

    // Find the card in the allowedCards list and remove it
    for (int i = 0; i < MAX_ALLOWED_CARDS; i++) {
      if (allowedCards[i].id == cardId) {
        allowedCards[i].cardHolderName = "";
        allowedCards[i].facilityCode = 0;
        allowedCards[i].cardCode = 0;
        break;
      }
    }

    // Redirect back to the main page
    request->redirect("/");
  });

  // Start the server
  server.begin();

  // Log that setup is complete
  delay(2000);
  Serial.print("Leaving setup\n");
  leds[0] = CRGB::Blue;
  FastLED.show();
}
 
void loop() {

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
     for (i=0; i<MAX_BITS; i++) 
     {
       databits[i] = 0;
     }
  }
}