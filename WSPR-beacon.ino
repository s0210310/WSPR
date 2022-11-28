// Adapted sketch of:
// "A simple mono-band WSPR beacon using an si5351 breakout for clock synthesis and a Ublox 6M for time sync. 
// Paul Taylor  VK3HN   4 Oct 2021  Version 1.0  Initial working version. "
//
// Adapted to use band-hopping and reduce RAM usage
// Minimum viable free dynamic RAM should be > 530 bytes to run
// Change  " // set up WSPR transmit data" for your callsign, locator and power
// Serguei Gontcharenko ON3DVP
// 16 Oct 2022     
//

#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <si5351.h>
#include <JTEncode.h>   //JTEncode  by NT7S https://github.com/etherkit/JTEncode

//-- compares GPS minutes, retrieves index of band_freq to use, adapt to own scheduling of band hop
//-- based on wsprnet bandhopping plan
//--every even minute, index { 0, 2, 4, 6, 8, 10,12,14,16, 18, 20,22,24,26,28,30,32,34,36, 38,40,42,44,46,48,50,52,54, 56, 58} 
const int bandschedule[29] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
const PROGMEM uint64_t band_freq[15] = {13750000ULL, 47570000ULL, 183810000ULL, 357010000ULL, 536620000ULL, 704010000ULL, 1014020000ULL, 1409710000ULL, 1810610000ULL, 2109610000ULL, 2492610000ULL, 2812610000ULL, 5029450000ULL, 14449000000ULL};
const char* band_str1[15] = {"137,", "475,", "1,838,", "3,570,", "5,366,", "7,040,", "10,140,", "14,097,", "18,106,", "21,096,", "24,926,", "28,126,", "50,294,", "144,490,"};
const int band_str[15] = {2190, 630, 160, 80, 60, 40, 30, 20, 17, 15, 12, 10, 6, 2};

//---------------------------------------------------------------------
// function to read ULLs frequencies from PROGMEM, saves RAM
uint64_t pgm_read_64( void *ptr )
{
  uint64_t result;
  memcpy_P( &result, ptr, sizeof(uint64_t) );
  return result;
}
//---------------------------------------------------------------------
uint64_t freq;   // frequency variable
int index = 0;   // index for bandhopping schedule 
//---------------------------------------------------------------------
// transmit windows that the beacon will use, as a percentage (0 == never transmit, 100 == always transmit)
#define TX_FRACTION 100   

//---------------------------------------------------------------------

#define GPS_RX_PIN 5  // receive serial data pin on the ublox GPS module
#define GPS_TX_PIN 4  // transmit serial data pin on the  ublox GPS module

enum E_LocatorOption {
  Manual,
  GPS
};

struct S_WSPRData
{
  char CallSign[7];                //Radio amateur Call Sign, zero terminated string can be four to six char in length + zero termination
  E_LocatorOption LocatorOption;   //If transmitted Maidenhead locator is based of GPS location or if it is using MaidneHead4 variable.
  char MaidenHead4[5];             //Maidenhead locator, must be 4 chars and a zero termination
  uint8_t TXPowerdBm;              //Power data in dBm min=0 max=60
};

S_WSPRData WSPRData;  // data structure for the WSPRPS data block

uint8_t tx_buffer[171];

byte GPSM; //GPS Minutes
byte GPSS; //GPS Seconds

LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

SoftwareSerial mySerial(GPS_RX_PIN, GPS_TX_PIN);  // create a Software Serial object

#define NMEA_BUFF_LEN 16
char nmea_buff[NMEA_BUFF_LEN];
char c = '0'; 
byte i, j;
bool GPS_locked = false;
boolean TXEnabled = true;  // can use this to inhibit transmit, for testing


Si5351 si5351;             // I2C address defaults to x60 in the NT7S lib

JTEncode jtencode;         // NT7S WSPR encoder lib


//Create a random seed by doing CRC32 on 100 analog values from port A0
unsigned long RandomSeed(void) {

  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  uint8_t ByteVal;
  unsigned long crc = ~0L;

  for (int index = 0 ; index < 100 ; ++index) {
    ByteVal = analogRead(A0);
    crc = crc_table[(crc ^ ByteVal) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (ByteVal >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}


void set_tx_buffer()
{
  // Clear out the transmit buffer
  memset(tx_buffer, 0, sizeof(tx_buffer));
  jtencode.wspr_encode(WSPRData.CallSign, WSPRData.MaidenHead4, WSPRData.TXPowerdBm, tx_buffer);
}



void SendWSPRBlock()
{
  // Loop through the string, transmitting one character at a time
  uint8_t i;
  unsigned long startmillis;
  unsigned long endmillis;

//  Serial.println("TX");
  si5351.output_enable(SI5351_CLK0, 1);  // turn the VFO on

  // Send WSPR for two minutes
  startmillis = millis();
  for (i = 0; i < 162; i++)  //162 WSPR symbols to transmit
  {
    endmillis = startmillis + ((i + 1) * (unsigned long) 683) ;   // Delay value for WSPR delay is 683 milliseconds
    uint64_t tonefreq;
    tonefreq = freq + ((tx_buffer[i] * 146));  //~1.46 Hz  Tone spacing in centiHz
    if (TXEnabled) 
    {
      si5351.set_freq(tonefreq, SI5351_CLK0);
    };

    Serial.print(i);     Serial.print(" "); Serial.println((unsigned long)tonefreq);  


    // wait until tone is transmitted for the correct amount of time
    while (millis() < endmillis){
      //Do nothing, just wait
    };

    byte k=(162-i);
    lcd.setCursor(13,1);
    if(k < 100) lcd.print(" ");
    if(k < 10)  lcd.print(" ");
    lcd.print(k-1);      
    
  };
  
  // Switch off Si5351a output
  si5351.output_enable(SI5351_CLK0, 0);  // turn the VFO off
  Serial.println(F("/TX"));
};



void Send(){
  if(!GPS_locked) return;
   Serial.println (F("Wait even minute"));
  if ((GPSS == 0) && ((GPSM % 2) == 0))   //If second is zero at even minute then start WSPR transmission
  {
    if(random(0, 101) >= (TX_FRACTION - 1) )
    {
      // skip this slot, wait for the next one
      Serial.println(F("  Skipping slot!"));
      lcd.setCursor(11,0);
      lcd.print(F("Skip!"));
    }
    else
    {
      // start a WSPR transmission
      Serial.println (F("start WSPR"));
       
      // read the minute, retrieve scheduled band, retrieves frequency to use at this tx window
      index = int(GPSM/2);
      int indexfreq = bandschedule[index];
      // read from progmem
      uint64_t local = pgm_read_64(&band_freq[indexfreq]); 
      //--------------------------------------------------------------
     
      freq = local;
      uint64_t freq1 = freq;      

      freq = freq + (100ULL * random (-100, 100)); //modify TX frequency with a random value beween -100 and +100 Hz
      Serial.print("f="); 
      Serial.println((unsigned long)(freq/100));

      // overwrite hertz 3 digits of the transmit frequency on the LCD with actual value
      byte hz = 100 - abs((int)((freq1 - freq)/100ULL));
      lcd.setCursor(0,1);
      lcd.print(F("               "));            //clear out last line on LCD
      lcd.setCursor(0,1);
      lcd.print(band_str1[bandschedule[index]]);
      lcd.setCursor(7,1);   
      if(hz < 100) lcd.print('0');
      if(hz < 10)  lcd.print('0');
      lcd.print(hz);

      set_tx_buffer();    // Encode the message in the transmit buffer
  
      delay(980);  // Transmissions nominally start one second into an even UTC minute: e.g., at hh:00:01, hh:02:01, etc.

      lcd.setCursor(8,0);
      lcd.print(F(" WSPR-Tx"));
      lcd.setCursor(8,1);

      SendWSPRBlock ();

      freq = freq + (100ULL * random (-100, 100));    // modify TX frequency with a random value beween -100 and +100 Hz

      /*lcd.setCursor(10,1);
      lcd.print(F("wait>"));
      for (byte n=9; n>0; n--){
        lcd.setCursor(15,1);
        lcd.print(n);
         Serial.println(n);
        delay(1000);       // to get us clear of the next even minute     
      }; */
      
      lcd.clear();         // slow operation

    }  // else start a WSPR transmission 
  } // even minute and zero second
} // Send()



void setup()
{
  Serial.begin(9600); 
  while (!Serial) ; // wait for serial port to connect, needed for native USB port only
  
  mySerial.begin(9600);
  lcd.init();                      // initialize the lcd 
  lcd.backlight();
  lcd.clear(); 
  lcd.setCursor(0,0); 
  lcd.print(F("GPS WSPRv1.0    "));
  delay(500);

  lcd.setCursor(0,1); 
  lcd.print(F("Arduino/ Ublox6M"));
  delay(1000);
  lcd.clear(); 

  pinMode(GPS_RX_PIN, INPUT);
  pinMode(GPS_TX_PIN, OUTPUT);
  random(RandomSeed());

  // set up WSPR transmit data
  WSPRData.CallSign[0] = 'O';    
  WSPRData.CallSign[1] = 'N';    
  WSPRData.CallSign[2] = '3';    
  WSPRData.CallSign[3] = 'D';    
  WSPRData.CallSign[4] = 'V';    
  WSPRData.CallSign[5] = 'P';    
  WSPRData.CallSign[6] = 0;    
  WSPRData.LocatorOption = GPS; 
  WSPRData.MaidenHead4[0] = 'J';          //Maidenhead locator, must be 4 chars and a zero termination
  WSPRData.MaidenHead4[1] = 'O';             
  WSPRData.MaidenHead4[2] = '1';             
  WSPRData.MaidenHead4[3] = '0';             
  WSPRData.MaidenHead4[4] = 0;             
  WSPRData.TXPowerdBm = 10;              // 10mW   Power data in dBm min=0 max=60, 8mA drive of the Si chip equals 10mW

  // set up the si5351
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);        // (0 is the default xtal freq of 25Mhz)
  si5351.set_correction(87000, SI5351_PLL_INPUT_XO); // trim the onboard oscillator, use si5351 library sketch to calibrate your chip
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA); // set drive strength, 8MA is max
}



void loop()
{
  i=0; 
  j=0; 
  c = '0'; 
  for(j=0; j<NMEA_BUFF_LEN; j++) nmea_buff[j]='\n';

  while(c != '\n') {
    if (mySerial.available()) {  // check SoftwareSerial port
      c = mySerial.read();       // read the next serial char from the GPS
      if(i<(NMEA_BUFF_LEN-1)) nmea_buff[i++] = c;  // append to NMEA buffer
    }    
  }

// we have a NMEA message, see what it is...
   if(nmea_buff[3] == 'R' &&
      nmea_buff[4] == 'M' &&
      nmea_buff[5] == 'C')
     {        
     j=0;
     while(nmea_buff[j]!='\n') Serial.print(nmea_buff[j++]);
     Serial.println();
//     Serial.println("]");

     if(nmea_buff[7] != ',') 
       GPS_locked = true;   // have decoded a timestamp 
     else 
       GPS_locked = false;  // havent decoded a timestamp yet
     
     lcd.setCursor(0,0);

     if(GPS_locked){
       lcd.setCursor(0,0);
       lcd.print(nmea_buff[7]);
       lcd.print(nmea_buff[8]);
       lcd.print(':');
       lcd.print(nmea_buff[9]);
       lcd.print(nmea_buff[10]);
       lcd.print(':');
       lcd.print(nmea_buff[11]);
       lcd.print(nmea_buff[12]);
       lcd.print(F("Z       ")); 
     
       String tokn(nmea_buff[11]);
       tokn.concat(nmea_buff[12]);
       byte sec = (byte)tokn.toInt(); 
       
       tokn="";
       tokn.concat(nmea_buff[9]); 
       tokn.concat(nmea_buff[10]); 
       byte mn = (byte)tokn.toInt(); 

       GPSS = sec;
       GPSM = mn;
     }
     else
       // No GPS timestamp decode yet...
       lcd.print(F("GPS scan"));
     index = int(GPSM/2);
     
     int nextindex = index; 
     if(index==15){nextindex=0;} else {nextindex = index+1;}
     lcd.setCursor(0,1);
     lcd.print(F("Next band: "));
     lcd.print(band_str[bandschedule[nextindex]]);
     lcd.print(F("M"));      

     // see if it's time to WSPR...
     Send();

  }
}
