#include <SPI.h>                                              // Harware SPI library
#include <Wire.h>
#include <mcp_can.h>                                          // CAN Library
#include <Adafruit_GFX.h>                                     // OLED GFX Library
#include <Adafruit_SSD1306.h>                                 // 1.3 inch OLED library
#include <EEPROM.h>                                           // EEPROM library

const byte led = 23;                                           // On board LED 23 will use for CAN connection indicator

//1.3 OLED PINS H/W SPI                                        // Define OLED Pins
//#define OLED_DC     9
//#define OLED_CS     11
//#define OLED_RESET  10
//Adafruit_SSD1306 display(OLED_DC, OLED_RESET, OLED_CS);

//2.4 OLED PINS S/W SPI                                       // Uncomment for 2.4 inch OLED
//#define OLED_MOSI   11
//#define OLED_CLK   10
//#define OLED_DC    9
//#define OLED_CS    12
//#define OLED_RESET 8
//Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

//I2C OLED DEFINE Pins 4 and 5
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//BUTTON CONFIG
const byte button = 4;
long buttonTimer = 0;
long longPressTime = 750;
boolean buttonActive = false;
boolean longPressActive = false;

//EEPROM CONFIG
byte addr = 1;                                                 // Initislise the EEPROM address
byte buttonPushCounter = EEPROM.read(1);                      // Counter for the number of button presses (get value from EEPROM)

//byte buttonPushCounter = 0;

//CAN CONFIG
unsigned char rxLen = 0;                                      // CAN message rx length
unsigned char rxBuf[8];                                       // CAN message number of bytes
MCP_CAN CAN0(10);                                             // Set CAN C/S pin
byte CANfilterset = 0;                                        // Check if the filter is set

//ECU VALUES
int VALUE1 = 0;                                                  // VALUE1 integer
int VALUE2 = 0;                                                  // VALUE2 integer
float FVALUE1 = 0;                                               // VALUE1 floating point (decemal place numbers)
float FVALUE2 = 0;                                               // VALUE2 floating point (decemal place numbers)

//SCREEN 1 HEADING, DATA AND MAX/MIN VALUES
int MAX1 = -10000;
int MAX2 = -10000;
int MIN1 = 10000;
int MIN2 = 10000;
float FMAX1 = -10000;
float FMAX2 = -10000;
float FMIN1 = 10000;
float FMIN2 = 10000;

int ScreenOff = 0;

void setup() {

  Serial.begin(115200);                                       // Serial Comms speed

 // pinMode(led, OUTPUT);                                       // Setup Pin 23 as Output Led

  if (buttonPushCounter >= 13)                                 // On first run EEPROM could be any value 0-254
  { 
    EEPROM.write(1, 0);                                        // If it is above 13, set it to zero to agree with the 'if' loop
    Serial.println("Reset EEPROM to Screen 0");
  }

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.setTextWrap(false);

  pinMode(button, INPUT_PULLUP);                              //Button config
  pinMode(6, OUTPUT);
  digitalWrite(6, LOW);

  display.setRotation(0);                                     // Set rotation (0) Normal, (2) 180 deg

  display.clearDisplay();
  display.setTextSize(3);                                     // Splash screen
  display.setTextColor(WHITE);
  display.setCursor(20, 5);
  display.println("ELITE");
  display.setCursor(20, 35);
  display.println("GAUGE");

  display.display();
  delay(1500);

  display.clearDisplay();

  while (CAN_OK != CAN0.begin(CAN_1000KBPS))                     // initiate CAN bus : baudrate = 1000k for Haltech
  {
    Serial.println(F("CAN BUS Shield init fail"));               // :(
    Serial.println(F(" Init CAN BUS Shield again"));             // Yes please
    delay(100);
  }
  Serial.println(F("CAN BUS Shield init ok!"));                  // YAY!

}

void loop() {

  if (digitalRead(button) == LOW) {

    if (buttonActive == false) {

      buttonActive = true;
      buttonTimer = millis();

    }

    if ((millis() - buttonTimer > longPressTime) && (longPressActive == false)) {

      // Clear the peak values

      longPressActive = true;

      CANfilterset = 0;

      resetValues();

      resetMaxMin();

      delay(50);
    }

  } else {

    if (buttonActive == true) {

      if (longPressActive == true) {

        longPressActive = false;

      } else {

        //INCREMENT THE BUTTON COUNTER AND SET THE FILTER
        buttonPushCounter++;

        if (buttonPushCounter >= 13) buttonPushCounter = 0; // Reset button counter to loop through the displays

        EEPROM.write(1, buttonPushCounter);               // Update the counter into EEPROM (address location 1)

        CANfilterset = 0;                                  // Reset the CAN filter for next CAN ID

        ScreenOff = 0;

        resetValues();

        resetMaxMin();

        display.clearDisplay();

        delay(50);

      }

      buttonActive = false;

    }
    display.clearDisplay();

   
    if (buttonPushCounter == 0) {                                      // buttonPushCounter is used to cylce through screens
      if (CANfilterset < 3) {
        CAN0.init_Mask(0, 0, 0xFFFF);                                 // CAN Mask 1 (allow all messages to be checked)
 //Display fails to init @line 82 when init_Filt is uncommented   
 //CAN0.init_Filt(0, 0, 0x0368);                                 // Only allow CAN ID 0x0368 to the buffer 1
        CAN0.init_Mask(1, 0, 0xFFFF);                                 // CAN Mask 2 (allow all messages to be checked)
    //    CAN0.init_Filt(2, 0, 0x03E9);                                 // Only allow CAN ID 0x03E9 to the buffer 2


        CANfilterset++;

        resetMaxMin();
      }

      if (CAN_MSGAVAIL == CAN0.checkReceive())                        // Check if data coming
      {
        digitalWrite(led, HIGH);                                      // turn the LED on, Data RX

        CAN0.readMsgBuf(&rxLen, rxBuf);                               // Read data,  rxLen: data length, rxBuf: data buf
        unsigned int canId = CAN0.getCanId();                         // Check CAN ID


        if (canId == 0x0368) {                                        // If CAN ID is 368 pull the following buffer info

          VALUE1 = word(rxBuf[0], rxBuf[1]);                          // First two of bytes ID368 are Lambda (buffer 0 + 1)
          FVALUE1 = (VALUE1 / 1000.0);                                // Divide by 1000 for correct scaling and make is a float number
        }

        if (canId == 0x03E9) {                                        // If CAN ID is 3E9 pull the following buffer info

          VALUE2 = word(rxBuf[4], rxBuf[5]);                          // Bytes 4 and 5 of ID3E9 are Target Lambda (buffer 4 + 5)
          FVALUE2 = (VALUE2 / 1000.0);                                // Divide by 1000 for correct scaling and make is a float number
        }

        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("LAMBDA");
        display.setCursor(0, 40);
        display.println("TARGET");
        display.setTextSize(3);
        display.setCursor(50, 0);
        display.println(FVALUE1, 2);                               // Show fvalue1 (Lambda) as a float number, 2 deciml places
        display.setCursor(50, 40);
        display.println(FVALUE2, 2);
        display.display();

      }
    }
  }
}

void resetValues() {

  //ECU VALUES
  VALUE1 = 0;
  VALUE2 = 0;
  FVALUE1 = 0;
  FVALUE2 = 0;
}

void resetMaxMin() {

  //SCREEN 1 HEADING, DATA AND MAX/MIN VALUES
  MAX1 = -10000;
  MAX2 = -10000;
  MIN1 = 10000;
  MIN2 = 10000;
  FMAX1 = -10000;
  FMAX2 = -10000;
  FMIN1 = 10000;
  FMIN2 = 10000;
}
