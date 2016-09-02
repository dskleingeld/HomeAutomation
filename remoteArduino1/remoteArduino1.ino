//FOR REMOTE ARDUINO 1 (ARDUINO NANO), using termpin's 3 en 2 (data, sck) (PD3 en PD2)
#include <SPI.h>
#include "RF24.h"
#include "printf.h"
#include "humiditySensor.h"

typedef union
{
  int number;
  uint8_t bytes[2];
} INTUNION_t;

// Specify data and clock connections
const int term_dataPin = 3; //PD3
const int term_clockPin = 2; //PD2

const short pirA = 0b00010000;
const short pirB = 0b00100000;

//radio address
const uint8_t ADDRESSES[][4] = { "No1", "No2" };

RF24 radio(7,8); //Set up nRF24L01 radio on SPI bus plus pins 7 & 8 (cepin, cspin)
TempHumid thSen (term_dataPin, term_clockPin);
                                                           // 




int readPIRs(){
  int pirValues = 0 ;
  if ((PIND & pirA) != 0){ 
    pirValues = pirValues | 1;//pir1 signal
    }
  else if ((PIND & pirB) != 0){
    pirValues = pirValues | 2;//pir2 signal
    }
  return pirValues;
}

void readAndSendPIRs(byte pipeNo){
  //check the PIR sensors then ack with the value
  byte sendBuffer[5] = {255,0,0,0,0};
  
  sendBuffer[4] = readPIRs();
  radio.writeAckPayload(pipeNo, sendBuffer, 5);
}
  

void setup(){

  Serial.begin(115200);
  // Setup and configure radio

  printf_begin();
  radio.begin();
  
  radio.setAddressWidth(3);               //sets adress with to 3 bytes long
  radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.enableAckPayload();               // Allow optional ack payloads
  radio.setRetries(0,15);                 // Smallest time between retries, max no. of retries
  radio.setPayloadSize(2);                // Here we are sending 1-byte payloads to test the call-response speed
  
  radio.openWritingPipe(ADDRESSES[1]);  // Both radios on same pipes, but opposite addresses
  radio.openReadingPipe(1,ADDRESSES[0]);// Open a reading pipe on address 0, pipe 1
  radio.startListening();                 // Start listening
  radio.printDetails();                   // Dump the configuration of the rf unit for debugging
}
int counter = 1;  
void loop(void){
  byte pipeNo, gotByte;                          // Declare variables for the pipe & byte received
  byte sendBuffer[5] = {255,0,0,0,0};
  static const byte sendBuffer_def[5] = {255,0,0,0,0};
  float temp_raw, humidity_raw;
  INTUNION_t temp_c, humidity, temp2; 

  while (radio.available(&pipeNo)){              // Read all available payloads
    unsigned long time = micros();
    radio.read( &gotByte, 1 );
    radio.powerUp();//TODO does this fix radio going to low power mode?
    Serial.print(counter);
    Serial.print("\n");
    counter++;
                                                 // Respond directly with an ack payload.
    if (gotByte == 't'){
      Serial.print("got t");
      //reads temp and sends response while also checking the PIR
      //and sending its status along
      temp_raw = thSen.readTemperatureC(readAndSendPIRs, radio);
      humidity_raw = thSen.readHumidity(temp_raw, readAndSendPIRs, radio);

      Serial.print("temp:");
      Serial.print(temp_raw);
      Serial.print("humidity:");
      Serial.print(humidity_raw);
      Serial.print("\n");

      temp_c.number = int(temp_raw*100);
      humidity.number = int(humidity_raw*100);

      //TODO rebuild this for using 12 bit data structures (so we can fill 3 bytes instead of 4)
      //see http://stackoverflow.com/questions/29529979/10-or-12-bit-field-data-type-in-c
      
      //copy the 4 bytes of each float into the sendbuffer
      memcpy(sendBuffer, temp_c.bytes, 2);
      memcpy(sendBuffer+2, humidity.bytes, 2);//TODO dont empty this buffer ever, just keep the old values
      
      Serial.print("sendbuffer: \n");
      Serial.print(sendBuffer[0]);
      Serial.print("\n");
//      Serial.print(sendBuffer[1],HEX);
//      Serial.print("\n");
//      Serial.print(sendBuffer[2],HEX);
//      Serial.print("\n");
//      Serial.print(sendBuffer[3],HEX);
//      Serial.print("\n");
//      Serial.print(sendBuffer[4],HEX);
//      Serial.print("\ndone: \n");
      
      radio.writeAckPayload(pipeNo,sendBuffer, 5);
      unsigned long tim = micros();
      printf("Got response values in: %lu microseconds\n\r",tim-time);
      memcpy(sendBuffer, sendBuffer_def, sizeof(sendBuffer_def));//reset buffer for sending PIR data again
    }
    
    else{    
            
//      Serial.print("sendbuffer: \n");
//      Serial.print(sendBuffer[0],HEX);
//      Serial.print("\n");
//      Serial.print(sendBuffer[1],HEX);
//      Serial.print("\n");
//      Serial.print(sendBuffer[2],HEX);
//      Serial.print("\n");
//      Serial.print(sendBuffer[3],HEX);
//      Serial.print("\n");
//      Serial.print(sendBuffer[4],HEX);
//      Serial.print("\ndone: \n");     
      
      radio.writeAckPayload(pipeNo,sendBuffer, 5);    
      //readAndSendPIRs(pipeNo);      TODO temp disabled for debugging     
    }
 }

}
