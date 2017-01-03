#include <TimerOne.h>

volatile float BatV; //Voltage to Battery
volatile float Vcc; //Voltage to Arduino
volatile float Vin; //Voltage on Input

const long RechargeEvery = 86400;//
const int MTimes = 5; // Times to measure
const int pinIV = A0; // Input Voltage pin
const int pinBV = A1; // Battery Voltage pin
const int pinBC = A2; // Battery Current pin

const int pinBat = 1; //pin to Battery MOSFET
const int pinLoad = 2; //pin to Load MOSFET
const int pinBatLed = 3; //pin to Battery charging Led
const int pinLoadLed = 4; //pin to Load Connected Led
//const int pinLoadLed = 5; //pin to Error Led

int Debug = 2; //Debug output level.
//Debug 0 -- print only system critical information
//Debug 1 -- print everysecond information
//Debug 2 -- print all information and enable delay

volatile long RechargeTimer = 86400;
volatile bool Charge = false; // Battery Charging or not.
volatile bool Load = false; // Load Connected or not.
volatile int ChargeTimer = 18000;

//Constants MUST BE EDITED!!!!!!!!!
const float Vbg = 1.1160895646; //Reference voltage. Read http://tim4dev.com/arduino-secret-true-voltmeter/
const float VBatMin = 11.0; //Battery Minimum Voltage
const float VBatMax = 13.6; //Battery Maximum Voltage
const bool UseCurrentSensor = false; //Use or not Current sensor, connected to pinBC (A2 default)
const float R1 = 33000; // 33K Resistor at Battery Voltage resistor dividor
const float R2 = 10000; // 10K Resistor at Battery Voltage resistor dividor

const int ChargeTimerMax = 18000; //Charge Every
const int RechargeTimerMax = 86400; 




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
    delay(1000);
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
    delay(10);
  }
  tempBatV = tempBatV / MTimes;
  float v  = (tempBatV * Vcc) / 1024.0;
  float v2 = v / (R2 / (R1 + R2));

  if (Debug >= 2) {
    Serial.print("Battery Voltage is: ");
    Serial.println(v2);
    delay(1000);
  }
  return v2;
}

void ChangeCharge(int NewState) { //Enable or disable battery charging
  if (NewState = 1) {
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

void ChangeLoad(int NewState) { //Enable or disable Load
  if (NewState = 1) {
    if (Debug >= 0) Serial.println(F("Load Connected to Battery"));
    Charge = true;
    digitalWrite(pinLoad,HIGH);
    digitalWrite(pinLoadLed,HIGH);
    delay (1);
  } else {
    if (Debug >= 0) Serial.println(F("Load Disconnected from Battery"));
    Charge = false;
    digitalWrite(pinLoad,LOW);
    digitalWrite(pinLoadLed,LOW );
    delay (1);
  }
}



void setup() {
  float tmp;
  // put your setup code here, to run once:
  Serial.begin(9600);
  if (Debug >= 0) Serial.println(F("HALLO! BOOTING! "));
  pinMode(pinIV, INPUT);
  pinMode(pinBV, INPUT);
  pinMode(pinBC, INPUT);
  
  tmp = ReadBatV();
  BatV = tmp;
  if (Debug >= 0) {Serial.print(F("Battery Voltage is "));Serial.println(BatV);}
  if (BatV < VBatMin){
    if (Debug >= 0) Serial.println(F("To low volage on Boot. Switching to 'Charge Mode'"));
    ChangeCharge (1);
  }
  if (Debug >= 2) delay(1000);

  
  Timer1.initialize(); // Every second do checking script
  Timer1.attachInterrupt(Everysecond);
}

void Everysecond () {
  Vin=analogRead(pinIV);//Checking input voltage
  if (Vin>512) { //If input voltage present
    if (Debug >=1) Serial.println(F("ES:Input Voltage present"));
    if (Charge) { //And charging
      if (Debug >=1) Serial.println(F("ES:Currently status: Charging"));
      if (UseCurrentSensor) { //If no current sensor then use timer
        ChargeTimer--;
        if (ChargeTimer>=0) {
          if (Debug >=0) Serial.println(F("ES:Charging process is over."));
          ChangeCharge(0);
        }
      } else {
        //FIXME: Write code with current sensor
      }
    } else { //
      RechargeTimer--;
      if (RechargeTimer<=0){ //Need to recharge
        RechargeTimer=RechargeTimerMax;
        if (Debug >=0) Serial.println(F("ES:Charging process is over."));
        ChangeCharge(0);
      }
    }
  } else if (Debug >=1) {
    if (Debug >=1) Serial.println(F("ES:No Input Voltage present"));
  }
}


void loop() {
  Vin=analogRead(pinIV); //Check input voltage
  if (Vin<512) {//If no voltage present,disconnect charging MOSFET and connect load
    if (Charge) {
      if (Debug >=0) Serial.println(F("No Input Voltage present, Switching to battery "));
      ChangeLoad(1);
      ChangeCharge(0);
      if (Debug >=2) delay(100);
    }
  }
  BatV=ReadBatV(); //Check battery voltage
  if (BatV<=VBatMin) {//If too low disconnect load MOSFET
    if ((!Charge)&&(Load)) {
        if (Debug >=0) Serial.println(F("NO INPUT. BATTERY VOLTAGE TOO LOW. SHUTTING DOWN!"));
        ChangeLoad(0);
        delay(10500);
      }
      else if ((Charge)&&(Load)) 
      {//If charging and Battery voltage too low -- show error
        if (Debug >=2) {Serial.println(F("Battery Voltage Too low when charging enable o_O"));delay(1000);}
      }
  }
  
}
