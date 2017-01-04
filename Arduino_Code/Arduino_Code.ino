#include <TimerOne.h>

volatile float BatV; //Voltage to Battery
volatile float Vcc; //Voltage to Arduino
volatile float Vin; //Voltage on Input

const int MTimes = 5; // Times to measure
const int pinIV = A0; // Input Voltage pin
const int pinBV = A1; // Battery Voltage pin
const int pinBC = A2; // Battery Current pin

const int pinBat = 1; //pin to Battery MOSFET
const int pinLoad = 2; //pin to Load MOSFET
const int pinBatLed = 3; //pin to Battery charging Led
const int pinVinLed = 4; //pin to Input is present Led
//const int pinVinLed = 5; //pin to Error Led

int Debug = 0; //Debug output level.
//Debug 0 -- print only system critical information
//Debug 1 -- print everysecond information
//Debug 2 -- print all information and enable delay

volatile bool Charge = false; // Battery Charging or not.
volatile bool Load = false; // Load Connected or not.
volatile long ChargeTimer = 0;

//Constants MUST BE EDITED!!!!!!!!!
const float Vbg = 1.1160895646; //Reference voltage. Read http://tim4dev.com/arduino-secret-true-voltmeter/
const float VBatMin = 4.0; //Battery Minimum Voltage
const float VBatMax = 4.6; //Battery Maximum Voltage
const bool UseCurrentSensor = false; //Use or not Current sensor, connected to pinBC (A2 default)
const float R1 = 0000; // 33K Resistor at Battery Voltage resistor dividor
const float R2 = 10000; // 10K Resistor at Battery Voltage resistor dividor

const long ChargeTimerCharge = 2; //How long to charge. Default is 18000
const long ChargeTimerRecharge = 5; //How often to recharge when on standby. Default is 86400
const long EverySecondTime = 1000000; //How often run Everysecond script in microseconds
const int delaytimer = 1000;


float readVcc() { //Read VCC voltage, Comparing to 1.1 Internal.
  byte i;
  float result = 0.0;
  float tmp = 0.0;

  for (i = 0; i < MTimes; i++) {
    // Read 1.1V reference against AVcc
    // set the reference to Vcc and the measurement to the internal 1.1V reference
    #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
    #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
    #else
    // works on an Arduino 168 or 328
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    #endif

    delay(3); // Wait for Vref to settle
    ADCSRA |= _BV(ADSC); // Start conversion
    while (bit_is_set(ADCSRA, ADSC)); // measuring

    uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
    uint8_t high = ADCH; // unlocks both

    tmp = (high << 8) | low;
    tmp = (Vbg * 1023.0) / tmp;
    result = result + tmp;
    delay(5);
  }

  result = result / MTimes;
  if (Debug >= 2) {
    Serial.print("VCC Voltage is: ");
    Serial.println(result);
    delay(delaytimer);
  }
  return result;
}

float ReadBatV() {//Battery voltage test
  int i;
  int tempBatV;
  Vcc = readVcc();
  // Read Analog pin, which connected to resistor divider on battery.
  tempBatV = 0.0;
  for (i = 0; i < MTimes; i++) {
    tempBatV = tempBatV + analogRead(pinBV);
//    delay(10);
  }
  tempBatV = tempBatV / MTimes;
  
  float v  = (tempBatV * Vcc) / 1024.0;
  float v2 = v / (R2 / (R1 + R2));

  if (Debug >= 2) {
    Serial.print("tempBatV: ");
    Serial.println(tempBatV);
    Serial.print("v: ");
    Serial.println(v);
    Serial.print("Battery Voltage is: ");
    Serial.println(v2);
    delay(delaytimer);
  }
  return v2;
}

void ChangeCharge(int NewChargeState) { //Enable or disable battery charging
  if (Debug >= 1) {Serial.print(F("ChangeCharge.NewChargeState is: "));Serial.println(NewChargeState);}
  if (NewChargeState == 1) {
    if (Debug >= 0) Serial.println(F("Battery charge is enabled "));
    Charge = true;
    digitalWrite(pinBat,HIGH);
    digitalWrite(pinBatLed,HIGH);
    delay (1);
  } else {
    if (Debug >= 0) Serial.println(F("Battery charge is disabled "));
    Charge = false;
    digitalWrite(pinBat,LOW);
    digitalWrite(pinBatLed,LOW);
    delay (1);
  }
}

void ChangeVin(int NewChargeState) { //Enable or disable input voltage present LED
  Vin=analogRead(pinIV); //Check input voltage
  if (Debug >= 1) {Serial.print(F("ChangeCharge.NewChargeState is: "));Serial.println(NewChargeState);}
  if (NewChargeState == 1) {
    if (Debug >= 0) Serial.println(F("Battery charge is enabled "));
    digitalWrite(pinBatLed,HIGH);
    delay (1);
  } else {
    if (Debug >= 0) Serial.println(F("Battery charge is disabled "));
    digitalWrite(pinBatLed,LOW);
    delay (1);
  }
}

void ChangeLoad(int NewLoadState) { //Enable or disable Load
  if (Debug >= 1) {Serial.print(F("ChangeLoad.NewLoadState is: "));Serial.println(NewLoadState);}
  if (NewLoadState == 1) {
    if (Debug >= 0) Serial.println(F("Load Connected to Battery"));
    Load = true;
    digitalWrite(pinLoad,HIGH);
    digitalWrite(pinVinLed,HIGH);
    delay (1);
  } else {
    if (Debug >= 0) Serial.println(F("Load Disconnected from Battery"));
    Load = false;
    digitalWrite(pinLoad,LOW);
    digitalWrite(pinVinLed,LOW );
    delay (1);
  }
}



void setup() {
  float tmp;
  // put your setup code here, to run once:
  if (Debug >=0) Serial.begin(9600);
  if (Debug >= 0) Serial.println(F("HALLO! BOOTING! "));
  pinMode(pinIV, INPUT);
  pinMode(pinBV, INPUT);
  pinMode(pinBC, INPUT);

  pinMode(pinBatLed, OUTPUT);
  pinMode(pinVinLed, OUTPUT);

  ChangeLoad(1);
  Load=1;
  
  tmp = ReadBatV();
  BatV = tmp;
  if (Debug >= 0) {Serial.print(F("Battery Voltage is "));Serial.println(BatV);}
  if (BatV < VBatMin){
    if (Debug >= 0) Serial.println(F("To low volage on Boot. Switching to 'Charge Mode'"));
    ChargeTimer=ChargeTimerCharge;
    ChangeCharge (1);
  } else {
    if (Debug >= 0) Serial.println(F("Voltage on boot is normal. Switching to 'Recharge Mode'"));
    ChargeTimer=ChargeTimerRecharge;
    ChangeCharge (0);

  }
  if (Debug >= 2) delay(delaytimer);

  
  Timer1.initialize(EverySecondTime); // Every second do checking script
  Timer1.attachInterrupt(Everysecond);
}

void Everysecond () {
  Vin=analogRead(pinIV);//Checking input voltage
  if (Vin>512) { //If input voltage present
    if (Debug >=1) Serial.println(F("ES:Input Voltage present"));

    if (Charge) { //And if charging check -- is it time to stop?
      if (Debug >=1) Serial.println(F("ES:Currently status: Charging"));
      if (Debug >=1) {Serial.print(F("ES:Timer is: "));Serial.println(ChargeTimer);}
      if (!UseCurrentSensor) { //If no current sensor then use timer
        ChargeTimer--;
        if (ChargeTimer<=0) {
          if (Debug >=0) { Serial.print(F("ES:Charging process is over. Recharging in: "));Serial.println(ChargeTimerRecharge);}
          ChangeCharge(0);
          ChargeTimer=ChargeTimerRecharge;
        }
      } else {
        //FIXME: Write code with current sensor
      }
    } else { //If not charging check if it is time to recharge
      if (Debug >=1) Serial.println(F("ES:Currently status: Not charging"));
      if (Debug >=1) {Serial.print(F("ES:Timer is: "));Serial.println(ChargeTimer);}
      ChargeTimer--;
      if (ChargeTimer<=0){ //Test if time to recharge is come
        if (Debug >=0) { Serial.print(F("ES:Charging process is over. Recharging in: "));Serial.println(ChargeTimerCharge);}
        ChangeCharge(1);
        ChargeTimer=ChargeTimerCharge;
      }
    }
  } else {
    
    if (Debug >=1) Serial.println(F("ES:No Input Voltage present"));
  }
}


void loop() {
  Vin=analogRead(pinIV); //Check input voltage
  if (Debug >=2) {Serial.print(F("INPUT Voltage is: "));Serial.println(Vin);delay(delaytimer);}
  
  if (Vin<512) {//If no voltage present,disconnect charging MOSFET and connect load
    if (Debug >=0) Serial.println(F("Switching to battery"));
    if (Charge) {
      if (Debug >=2) {Serial.print(F("ES:Charge is: "));Serial.println(Charge);delay(delaytimer);}
      ChangeCharge(0);
      ChargeTimer=ChargeTimerCharge;
      if (Debug >=2) delay(delaytimer);
    }

    BatV=ReadBatV(); //Check battery voltage
    if (BatV<=VBatMin) {//If too low disconnect load MOSFET
      if ((!Charge)&&(Load)) {
          if (Debug >=0) Serial.println(F("NO INPUT. BATTERY VOLTAGE TOO LOW. SHUTTING DOWN!"));
          ChangeLoad(0);
          delay(100500);
        }
        else if ((Charge)&&(Load)) 
        {//If charging and Battery voltage too low -- show error
          if (Debug >=2) {Serial.println(F("Battery Voltage Too low when charging enable o_O"));delay(delaytimer);}
        }
    }
  }
  
}
