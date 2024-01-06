#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

#define ONE_WIRE_BUS 13

float lowerTemperatureThreshold = 20.0;
float upperTemperatureThreshold = 30.0;

unsigned long cooldownPeriod = 300000;

bool trySMS = false;
int tryCount = 0;
int tryCountLimit = 5;
String tryText = " ";
String tryNum = " ";

bool flipU = false;
bool flipD = false;

String conSMS = " ";
String conNum = " ";

String AdminMenu = "Admin Menu\n";
String NMenu = "Menu\n";

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

SoftwareSerial sim800l(14,27);

String AdminPhoneNumber = "+94778049650";
String N1PhoneNumber = AdminPhoneNumber;

float getTemperature() {
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.println(temperature);
  return temperature;
}

void sendSMS(String message, String phoneNumber) {
  // Send SMS using SIM800L
  sim800l.println("AT+CMGS=\"" + phoneNumber + "\"");
  delay(1000);
  sim800l.println(message);
  delay(1000);
  sim800l.write(26); 
  delay(2000);

  // Wait for the acknowledgment response (+CMGS: <reference>)
  unsigned long startTime = millis();
  while (millis() - startTime < 4000) {  // Wait for 5 seconds for acknowledgment
    if (sim800l.find("+CMGS:")) {
      Serial.println("SMS Sent Successfully!");
      trySMS = false;
      tryText = " ";
      tryNum = " "; 
      return;
    }
    delay(100);
  }
  trySMS = true;
  tryText = message;
  tryNum = phoneNumber;
  // If acknowledgment is not received, consider it a failure
  Serial.println("Failed to send SMS. Please check your SIM800L module and try again.");
}

enum TemperatureState {
  NORMAL,
  EXCEEDS_UPPER_THRESHOLD,
  BELOW_LOWER_THRESHOLD
};

TemperatureState checkTemperatureLimits(float temperature) {
  if (temperature > upperTemperatureThreshold) {
    return EXCEEDS_UPPER_THRESHOLD;
  } else if (temperature < lowerTemperatureThreshold) {
    return BELOW_LOWER_THRESHOLD;
  } else {
    return NORMAL;
  }
}

void processReceivedSMS(String receivedMessage) {
  // Check if the received message starts with "+CMT:"
  if (receivedMessage.startsWith("+CMT:")) {
    // Extract sender's phone number
    int firstQuotePos = receivedMessage.indexOf('"');
    int secondQuotePos = receivedMessage.indexOf('"', firstQuotePos + 1);
    conNum = receivedMessage.substring(firstQuotePos + 1, secondQuotePos);

    // Read the next line to get the SMS content
    receivedMessage = sim800l.readStringUntil('\n');
    conSMS = receivedMessage.substring(0, receivedMessage.length() - 1);  // Remove the newline character

    // Process the received message and sender's number (customize this part based on your needs)
    Serial.println("Received SMS from: " + conNum);
    Serial.println("SMS Content: " + conSMS);

    // Example: Reply back to the sender with the sender's number
    //sendSMS("Received your SMS. Sender's number: " + conNum, conNum);
    }
}

void updateSerial(){
  delay(700);
   while (Serial.available()) {
    sim800l.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while(sim800l.available()) {
    Serial.write(sim800l.read());//Forward what Software Serial received to Serial Port
  }
}

void updateSerialL() {
  delay(700);
  // Forward data from Serial to SIM800L
  while (Serial.available()) {
    sim800l.write(Serial.read());
  }
  // Check for and process incoming SMS
  if (sim800l.available()) {
    String receivedMessage = sim800l.readStringUntil('\n');
    Serial.print(receivedMessage);
    processReceivedSMS(receivedMessage);
  }
}

void setSIM800(){
  sim800l.println("AT");
  updateSerial();
  delay(3000);
  // Set SIM800L to text mode for SMS
  sim800l.println("AT+CMGF=1"); // Set SMS mode to text
  updateSerial();
  delay(100);
  sim800l.println("AT+CNMI=2,2,0,0,0");
  updateSerial();
  delay(100);
}

void setup() {

  Serial.begin(9600);
  sim800l.begin(9600);

  AdminMenu += "Get Temperature :- TEMP\n";
  AdminMenu += "Set Receiver :- SETR+94xxxxxxxxx\n";
  
  NMenu += "Get Temperature :- TEMP\n";
  // Initialize temperature sensor
  sensors.begin();
  setSIM800();
  
}

void loop() {
  if (trySMS == 1){
    if (tryCount<tryCountLimit){
      if (tryCount == 3){
        setSIM800();
      }
      sendSMS(tryText, tryNum);
      tryCount += 1;
    }else{
      trySMS = false;
      tryText = " ";
      tryNum = " ";
      tryCount = 0;
      Serial.println("Try Failed");
    }
  }else{

    float temperature = getTemperature();
    TemperatureState state = checkTemperatureLimits(temperature);
    if ((state == EXCEEDS_UPPER_THRESHOLD) && (flipU == false)) {
    
      String message = "Current Temperature: " + String(temperature) + " C\n";
      message += "Upper Threshold: " + String(upperTemperatureThreshold) + " C\n";
      message += "Lower Threshold: " + String(lowerTemperatureThreshold) + " C\n";
      message += "Temperature exceeds upper threshold!";

      sendSMS(message,N1PhoneNumber);
      flipU = true;
    
      } else if ((state == BELOW_LOWER_THRESHOLD) && (flipD == false)) {

        String message = "Current Temperature: " + String(temperature) + " C\n";
        message += "Upper Threshold: " + String(upperTemperatureThreshold) + " C\n";
        message += "Lower Threshold: " + String(lowerTemperatureThreshold) + " C\n";
        message += "Temperature below lower threshold!";

        sendSMS(message,N1PhoneNumber);
        flipD = true;
    }else if(state == NORMAL){

        flipD = false;
        flipU = false;

    }

  }
  
  updateSerialL();

  if (conSMS == "TEMP"){
    float temperature = getTemperature();
    String message = "Current Temperature: " + String(temperature) + " C\n";
    sendSMS(message, conNum);
    conSMS = " ";
    conNum = " ";
  }

  if (conSMS == "MENU"){
    if(conNum == AdminPhoneNumber){
      sendSMS(AdminMenu,AdminPhoneNumber);
    }else{
      sendSMS(NMenu,conNum);
    }
    conSMS = " ";
    conNum = " ";
  }
  if (conNum == AdminPhoneNumber) {
      if (conSMS.startsWith("SETR")) {
        // Extract the new phone number from the received command
        N1PhoneNumber = conSMS.substring(4);
        sendSMS("Receiver set to: " + N1PhoneNumber, AdminPhoneNumber);
        conSMS = " ";
        conNum = " ";
      }
  }
}
