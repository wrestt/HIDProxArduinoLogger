#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

 // include the SD library:
#include <SPI.h>
#include <SD.h>

// SD CARD
const int chipSelect = 10;

#define MAX_BITS 100                 // max number of bits
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse.

unsigned char databits[MAX_BITS];    // stores all of the data bits
unsigned char bitCount;              // number of bits currently captured
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits
unsigned long facilityCode=0;        // decoded facility code
unsigned long cardCode=0;            // decoded card code

String dataString = "";
String cardString = "";
String facilityString = "";

int LED_GREEN = 5;                   // ORANGE
int LED_RED = 6;                     // BROWN WIRE
int BEEP_BEEP = 4;                   // YELLOW WIRE

// interrupt that happens when INTO goes low (0 bit)
void ISR_INT0() {
//  Serial.print("0");   // uncomment this line to display raw binary
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1() {
//  Serial.print("1");   // uncomment this line to display raw binary
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BEEP_BEEP, OUTPUT);
  digitalWrite(LED_RED, HIGH);       // High = Off
  digitalWrite(BEEP_BEEP, HIGH);     // High = off
  digitalWrite(LED_GREEN, HIGH);     // Low = On
  pinMode(2, INPUT);                 // DATA0 (INT0)
  pinMode(3, INPUT);                 // DATA1 (INT1)

  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  Serial.println("RFID Readers");

  // binds the ISR functions to the falling edge of INTO and INT1
  attachInterrupt(0, ISR_INT0, FALLING);
  attachInterrupt(1, ISR_INT1, FALLING);
  weigand_counter = WEIGAND_WAIT_TIME;

  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    //long beep for card failure:
    digitalWrite(BEEP_BEEP, LOW);
    delay(3000);
    digitalWrite(BEEP_BEEP, HIGH);
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
}

void loop() {
  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {
    if (--weigand_counter == 0) {
      flagDone = 1;
    }
  }

  // if we have bits and we the weigand counter went out
  if (bitCount > 0 && flagDone) {
    unsigned char i;
    if (bitCount == 35) {
      // 35 bit HID Corporate 1000 format
      // facility code = bits 2 to 14
      for (i=2; i<14; i++) {
         facilityCode <<=1;
         facilityCode |= databits[i];
         facilityString += String(databits[i], DEC);
      }

      // card code = bits 15 to 34
      for (i=14; i<34; i++) {
         cardCode <<=1;
         cardCode |= databits[i];
         cardString += String(databits[i], DEC);
      }

      printBits();
    } else if (bitCount == 26) {
      // standard 26 bit format
      // facility code = bits 2 to 9
      for (i=1; i<9; i++) {
        facilityCode <<=1;
        facilityCode |= databits[i];
        facilityString += String(databits[i], DEC);
      }

      // card code = bits 10 to 23
      for (i=9; i<25; i++) {
        cardCode <<=1;
        cardCode += databits[i];
        cardString += String(databits[i], DEC);
      }
      printBits();
    }

    // cleanup and get ready for the next card
    bitCount = 0;
    facilityCode = 0;
    cardCode = 0;
    dataString = "";
    cardString = "";
    facilityString = "";
    
    for (i=0; i<MAX_BITS; i++) {
     databits[i] = 0;
    }
  }
}

String checkZero(int number) {
  if (number >= 0 && number < 10) {
    return '0';
  }
  return '';
}

void printBits() {
  tmElements_t tm;

  if (RTC.read(tm)) {
    dataString += "Time,";
    dataString += checkZero(tm.Hour);
    dataString += tm.Hour;
    dataString += ':';
    dataString += checkZero(tm.Minute);
    dataString += tm.Minute;
    dataString += ':';
    dataString += checkZero(tm.Second);
    dataString += tm.Second;
    
    dataString += ",Date,";
    dataString += tm.Month;
    dataString += '/';
    dataString += tm.Day;
    dataString += '/';
    dataString += tmYearToCalendar(tm.Year);
  } else {
    dataString += "Time,error";
  }

  dataString += ",BC,";
  dataString += bitCount;
  dataString += ",FC,";
  dataString += facilityCode;
  dataString +=",CC,";
  dataString += cardCode;
  dataString += ",FCBit,";
  dataString += facilityString;
  dataString += ",CCBit,";
  dataString += cardString;
  dataString += ",TotalString,";
  dataString += facilityString;
  dataString += cardString;

  // turn the led back to red
  digitalWrite(LED_RED, LOW); // Red

  //error handleing:
  if(cardCode == 0) {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(BEEP_BEEP, LOW);
    delay(1000);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(BEEP_BEEP, HIGH);
  }

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  } else {
    // if the file isn't open, pop up an error:
    Serial.println("error opening datalog.txt");
  }
}

