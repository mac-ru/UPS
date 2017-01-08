#include <TimerOne.h>

volatile float BatV; //Voltage to Battery
volatile float Vcc; //Voltage to Arduino
volatile float Vin; //Voltage on Input

const int MTimes = 5; // Times to measure
const int pinIV = A0; // Input Voltage pin
const int pinBV = A1; // Battery Voltage pin
const int pinBC = A2; // Battery Current pin

const int pinBat = 2; //pin to Battery MOSFET
const int pinLoad = 3; //pin to Load MOSFET
const int pinBatLed = 4; //pin to Battery charging Led
const int pinVinLed = 5; //pin to Input is present Led
//const int pinVinLed = 5; //pin to Error Led

int Debug = -1; //Debug output level.
//Debug 0 -- print only system critical information
//Debug 1 -- print table
//Debug 2 -- Be verbose
//Debug 3 -- print everysecond information
//Debug 4 -- print all information and enable delay

volatile bool InputV = false; //Input is present or not
volatile bool Charge = false; // Battery Charging or not.
volatile bool Cooldown = false; // Battery Cooldown state.
volatile bool Load = false; // Load Connected or not.
volatile long ChargeTimer = 0;

//Constants MUST BE EDITED for your system!!!!!!!!!
const float Vbg = 1.1160895646; //Reference voltage. Read http://tim4dev.com/arduino-secret-true-voltmeter/
const float VBatMin = 11.0; //Battery Minimum Voltage -- System will shutdown at this voltage
const float VBatMax = 13.5; //Battery Maximum Voltage -- battery charge will stop if this voltage will stay more than this at ChargeTimerCooldown cycles
const bool UseCurrentSensor = false; //Use or not Current sensor, connected to pinBC (A2 default)
const float R1 = 29900; // 33K Resistor at Battery Voltage resistor dividor
const float R2 = 10000; // 10K Resistor at Battery Voltage resistor dividor
const float CurrentDiff = 10.0;

const long constChargeTimerCharge = 10; //How long to charge. Default is 60
const long constChargeTimerCooldown = 10; //How long to Cooldown battery. Default is 10
const long constChargeTimerRecharge = 10; //How often to recharge when on standby. Default is 86400 seconds (1 day)
const long EverySecondTime = 5000000; //How often run Everysecond script in microseconds

long ChargeTimerCharge = constChargeTimerCharge/(EverySecondTime/1000000);
long ChargeTimerCooldown = constChargeTimerCooldown/(EverySecondTime/1000000);
long ChargeTimerRecharge = constChargeTimerRecharge/(EverySecondTime/1000000);


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
  //if (Debug >= 4) {Serial.print(F("VCC Voltage is: ")); Serial.println(result); delay(delaytimer);}
  return result;
}

float ReadBatV() {//Battery voltage test
  int i;
  int tempBatV;
//  Vcc = readVcc();
  // Read Analog pin, which connected to resistor divider on battery.
  tempBatV = 0.0;
  for (i = 0; i < MTimes; i++) {
    tempBatV = tempBatV + analogRead(pinBV);
//    delay(10);
  }
  tempBatV = tempBatV / MTimes;
  
  float v  = (tempBatV * Vcc) / 1024.0;
  float v2 = v / (R2 / (R1 + R2));

  //if (Debug >= 3) {Serial.print(F("ReadBatV.out.Battery Voltage is: "));Serial.println(v2);delay(delaytimer);}
  return v2;
}

void ChangeCharge(int NewState) { //Enable or disable battery charging
  //if (Debug >= 3) {Serial.print(F("ChangeCharge.NewState is: "));Serial.println(NewState);}
  switch (NewState) {
    case 0:
      //if (Debug >= 2) Serial.println(F("Battery charge is disabled "));
      Charge = false;
      Cooldown = false;
      digitalWrite(pinBat,LOW);
      digitalWrite(pinBatLed,LOW);
    break;
    case 1:
      //if (Debug >= 2) Serial.println(F("Battery charge is enabled "));
      Charge = true;
      Cooldown = false;
      digitalWrite(pinBat,HIGH);
      digitalWrite(pinBatLed,HIGH);
    break;
    case 2:
      //if (Debug >= 2) Serial.println(F("Battery charge is on Hold "));
      Charge = true;
      Cooldown = true;
      digitalWrite(pinBat,LOW);
      digitalWrite(pinBatLed,HIGH);
    break;
  }
  delay (1);
}

void ChangeVin() { //Enable or disable input voltage present
  int VinTrigger = 900;
  //if (Debug >= 3) {Serial.println(F("Executing ChangeVin "));delay(delaytimer);}
  delay(5);
  Vin=analogRead(pinIV); //Check input voltage

  if (((!InputV)&&(Vin>=VinTrigger))||((InputV)&&(Vin<VinTrigger))) //If Voltage status change
  {
    if (Vin >=VinTrigger) {
      InputV=true;
      //if (Debug >= 2) Serial.println(F("Input voltage normal! "));
      digitalWrite(pinVinLed,HIGH);
    } else {
      InputV=false;
      //if (Debug >= 2) Serial.println(F("No input voltage "));
      digitalWrite(pinVinLed,LOW);
    }
    delay (1);
  }
}

void ChangeLoad(int NewState) { //Enable or disable Load
  //if (Debug >= 4) {Serial.print(F("ChangeLoad.NewState is: "));Serial.println(NewState);}
  if (NewState == 1) {
    //if (Debug >= 2) Serial.println(F("Load Connected to Battery"));
    Load = true;
    digitalWrite(pinLoad,HIGH);
  } else {
    //if (Debug >= 2) Serial.println(F("Load Disconnected from Battery"));
    Load = false;
    digitalWrite(pinLoad,LOW);
  }
  delay (1);
}



void setup() {

  float tmp;
  long Time;
  // put your setup code here, to run once:
  //if (Debug >= 0) Serial.begin(9600);
  //if (Debug >= 0) Serial.println(F("HALLO! BOOTING! "));
  pinMode(pinIV, INPUT);
  pinMode(pinBV, INPUT);
  pinMode(pinBC, INPUT);

  pinMode(pinBat, OUTPUT);
  pinMode(pinLoad, OUTPUT);
  pinMode(pinBatLed, OUTPUT);
  pinMode(pinVinLed, OUTPUT);

  //if (Debug >= 4) {Serial.print(F("ChargeTimerCharge is "));Serial.println(ChargeTimerCharge);}
  //if (Debug >= 4) {Serial.print(F("ChargeTimerCooldown is "));Serial.println(ChargeTimerCooldown);}
  //if (Debug >= 4) {Serial.print(F("ChargeTimerRecharge is "));Serial.println(ChargeTimerRecharge);}

  delay(10);
  ChangeCharge(0);
  ChangeLoad(1);

  delay(100);
  Vcc = readVcc();
  ChangeVin();
  BatV = ReadBatV();
  

  //if (Debug >= 2) {Serial.print(F("Battery Voltage is "));Serial.println(BatV);}
  if (InputV)
  {
    if (BatV < VBatMax){
      //if (Debug >= 2) Serial.println(F("To low volage on Boot. Switching to 'Charge Mode'"));
      ChargeTimer=ChargeTimerCharge;
     ChangeCharge (1);
   } else {
     //if (Debug >= 2) Serial.println(F("Voltage on boot is normal. Switching to 'Recharge Mode'"));
     ChargeTimer=ChargeTimerRecharge;
     ChangeCharge (0);
   }
  }
  //if (Debug >= 4) delay(delaytimer);

  
  //if (Debug >= 0) Serial.println(F("Let's work! "));
  Timer1.initialize(EverySecondTime); // Every second do checking script
  Timer1.attachInterrupt(Everysecond);
}
/*
void printtable() { //Function to print table
  //INPUTV:TRUE(1023) MODE:charging; BATTERYVOLTAGE:11.5; OUTPUT;
  Serial.print(F("InputV: "));
  if (InputV) Serial.print(F("True")); else Serial.print(F("   False"));
  Serial.print(F("("));
  Serial.print(Vin);
  Serial.print(F(")  Mode:"));
  if ((Charge)&&  (Cooldown)) Serial.print(F("Cooldown;"));;
  if ((Charge)&& (!Cooldown)) Serial.print(F("Charging;"));;
  if ((!Charge)&&(!Cooldown)) Serial.print(F("Waiting ;"));;
  Serial.print(F(" Battery Voltage: "));
  if (BatV<10) Serial.print(F(" "));
  Serial.print(BatV);
  Serial.print(F("; Output is: "));
  if (Load)  Serial.print(F("Connected   "));
  if (!Load) Serial.print(F("Disconnected"));
  Serial.println("");
  
}
*/
void Everysecond () { //This function check Everyperiod is it time to charge
  int i;
  int z = 5;
  float cur = 0.0;
  if (InputV) { //If input voltage present
    //if (Debug >= 3) Serial.println(F("ES:Input Voltage present"));

    if (Charge) { //And if charging check -- is it time to stop?
      //if (Debug >= 3) Serial.println(F("ES:Currently status: Charging"));
      //if (Debug >= 3) {Serial.print(F("ES:Timer is: "));Serial.println(ChargeTimer);}
      if (!UseCurrentSensor) { //If no current sensor then use timer
        ChargeTimer--;
        if (ChargeTimer<=0) {
          if (!Cooldown)
          {
            //if (Debug >= 3) { Serial.print(F("ES:Charging process is over. Time to test battery in: "));Serial.println(ChargeTimerRecharge);}
            ChangeCharge(2);
            ChargeTimer=ChargeTimerCooldown;
          } else {
            //if (Debug >= 3) { Serial.println(F("ES:Cooldown time exceeded. Time to test battery voltage"));}
            Vcc = readVcc(); //Check if battery voltage. If it more than VBatMax then stop charging. Else charge battery again.
            delay(10);
            BatV=ReadBatV(); 
            if (BatV>=VBatMax) {
              //if (Debug >= 3) { Serial.print(F("ES:Charging process is over. Recharging in: "));Serial.println(ChargeTimerRecharge);}
              ChangeCharge(0);
              ChargeTimer=ChargeTimerRecharge;
            } else {
              //if (Debug >= 3) { Serial.print(F("ES:Charging process continue because BatV<VBatMax. Try again at: "));Serial.println(ChargeTimerCharge);}
              ChangeCharge(1);
              ChargeTimer=ChargeTimerCharge;
            }
          }
        }
      } else {
        cur = 0;
        for (i=1;i<=z;i++) {
          cur=cur+analogRead(pinBC);
          delay(1);
        }
        cur=cur/z;
        //if (Debug >= 3) {Serial.print(F("ES:Current is: "));Serial.println(cur);}
        if (abs(cur-512)<CurrentDiff){
          //if (Debug >= 3) { Serial.print(F("ES:Charging process is over. Recharging in: "));Serial.println(ChargeTimerRecharge);}
          ChangeCharge(0);
          ChargeTimer=ChargeTimerRecharge;
        }
        
      }
    } else { //If not charging check if it is time to recharge
      //if (Debug >= 3) Serial.println(F("ES:Currently status: Not charging"));
      //if (Debug >= 3) {Serial.print(F("ES:Timer is: "));Serial.println(ChargeTimer);}
      ChargeTimer--;
      if (ChargeTimer<=0){ //Test if time to recharge is come
        Vcc = readVcc(); //Check if battery voltage. If it more than VBatMax then do nothing. Else charge battery again.
        BatV=ReadBatV(); 
        if (BatV<=VBatMax) {
          //if (Debug >= 3) { Serial.print(F("ES:Charging process is over. Recharging in: "));Serial.println(ChargeTimerRecharge);}
          ChangeCharge(0);
          ChargeTimer=ChargeTimerRecharge;
        }

        //if (Debug >= 3) { Serial.print(F("ES:Time to recharge "));Serial.println(ChargeTimerCharge);}
        ChangeCharge(1);
        ChargeTimer=ChargeTimerCharge;
      }
    }
  } else {
    
    //if (Debug >= 3) Serial.println(F("ES:No Input Voltage present"));
  }
    //if (Debug >= 1) printtable();
}


void loop() {
  //FIXME: ADD manual charging button
  ChangeVin();
  if (!InputV) {//If no voltage present,disconnect charging MOSFET and connect load
    if (Charge) //if (Debug >= 4) {Serial.print(F("Charge is: "));Serial.println(Charge);}
      ChangeCharge(0);
      ChargeTimer=ChargeTimerCharge;
      //if (Debug >= 4) delay(delaytimer);
    
      if (BatV<=VBatMin) {//If too low voltage disconnect load MOSFET. I know, that BatV stored in memory
        if ((!Charge)&&(Load)) {
          //if (Debug >= 0) Serial.println(F("No input. Battery voltage low. SHUTTING DOWN!"));
          ChangeLoad(0);
          delay(10500);
          //if (Debug >= 0) Serial.println(F("Still Alive? WTF? External power to me? Input voltage is normal? Oh, I will try to sort out this situation"));
          ChangeVin();
          if (InputV) {
             //if (Debug >= 0) Serial.println(F("You put me on big caparcitor? I will set output to online :)"));
             ChangeLoad(0);
          }
          } else if ((Charge)&&(Load)) {//If charging and Battery voltage too low -- show error
            //if (Debug >= 0) {Serial.println(F("Battery Voltage Too low when charging enable o_O"));delay(delaytimer);}
          }
      }
  }
  if  (Debug >= 3) {Serial.print(F("Charge is: "));Serial.println(Charge);delay(delaytimer);}
  
}
