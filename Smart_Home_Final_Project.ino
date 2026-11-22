#include <Wire.h>
#include <I2CKeyPad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <DHT.h>

Servo my_servo;
LiquidCrystal_I2C lcd(0x27, 16, 2);
I2CKeyPad keyPad(0x26);

// Pins
const int securityButton = 2;
const int lightButton = 3;
const int Trig       = 4;
const int Echo     = 5;
const int buzzer = 6;
const int doorLed = 7;
const int indoorLed = 8;
const int servo = 9;
const int dhtSensor = 10;
const int fan = 11;
const int reedSwitch = 12;
const int ldr = A0;
const int pot = A1;

DHT dht(dhtSensor,DHT11);

unsigned long lastPot = 0;
unsigned long thre_temp = 0;
unsigned long lastDht = 0;

// settings
const int ldrDark = 200;     
const int darkConfirm_ms = 3000;

// variables
bool securityOn = false;                
int lightMode = 0;                      

int lastSecurityBtn = LOW;
int lastLightBtn = LOW;

unsigned long secLastPressed = 0;
unsigned long lightLastPressed = 0;

unsigned long darkStartTime = 0;        
unsigned long lastPrintTime = 0;        

char door_pass[4] = {'1', '2', '3', '4'};
char security_pass[4]={'5','6','7','8'};
char user[4];
int count = 0;
int trial = 0;

// Door flag
bool isDoorOpen = false;
unsigned long doorOpenTimer = 0;
bool DHT= true;

bool isDoorLockedOut = false;
unsigned long doorLockoutTimer = 0;
unsigned long doorLockoutTick = 0;
int doorLockoutRemaining = 20;

// Security flag
bool window_security = false;             
bool print = false;                     
char alarmUser[4];
int alarmCount = 0;
int alarmTrial = 0;

bool isAlarmLockedOut = false;
unsigned long alarmLockoutTimer = 0;
unsigned long alarmLockoutTick = 0;
int alarmLockoutRemaining = 20;

float distance;
float duration;
char keyMap[19]="123A456B789C*0#DNF";
//char keyMap[19]="FND#0*C987B654A321";

void setup() {

  thre_temp = analogRead(A1);

  Serial.begin(9600);

  pinMode(securityButton, INPUT);
  pinMode(lightButton, INPUT);
  pinMode(reedSwitch, INPUT_PULLUP);
  pinMode(Trig, OUTPUT);
  pinMode(Echo, INPUT);

  pinMode(buzzer, OUTPUT);
  pinMode(doorLed, OUTPUT);
  pinMode(indoorLed, OUTPUT);
  pinMode(fan, OUTPUT);

  digitalWrite(buzzer, 0);
  digitalWrite(doorLed, 0);
  digitalWrite(indoorLed, 0);
  digitalWrite(fan, 0);

  dht.begin();
  keyPad.begin();  
  keyPad.loadKeyMap(keyMap);
  keyPad.setDebounceThreshold(50);

  my_servo.attach(servo);
  my_servo.write(90);

  Serial.println("System started");
  Serial.println("Security: OFF");
  Serial.println("Light mode: AUTO");

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Sec:OFF");
  lcd.setCursor(8,0);
  lcd.print("Light:A");
  lcd.setCursor(0,1);
  lcd.print("D:close");
  lcd.setCursor(8,1);
  lcd.print("DHT:");
  lcd.setCursor(12,1);
  lcd.print(dht.readTemperature());
}

void loop() {
//not printing DHT readings when security warning sent 
  if(DHT){
  lcd.setCursor(12,1);
  lcd.print(dht.readTemperature());
  }
  Fan();

 printLdrValue();

  checkSecurityButton();

  checkLightButton();

  doorAccess();

  if (securityOn || window_security) {
    runSecurity();
  } else {
    digitalWrite(buzzer, LOW);
    digitalWrite(doorLed, LOW);
  }

  runLighting();


  
  //Serial.println(dht.readTemperature());
  //Serial.println(analogRead(pot));
  //digitalWrite(indoorLed,1);
 // Serial.println(getdistance());
  //Serial.println(analogRead(pot));
  //digitalWrite(fan,1);
  //Serial.println(dht.readTemperature());
}

void printLdrValue() {
  if (millis() - lastPrintTime >= 5000) {
    lastPrintTime = millis();
    Serial.print("LDR value: ");
    Serial.println(analogRead(ldr));
  }
}
//checking state of security button
void checkSecurityButton() {
  int buttonState = digitalRead(securityButton);
  //button debounce and last state saving
  if (millis() - secLastPressed >= 50 && buttonState != lastSecurityBtn) {
    secLastPressed = millis();
    if (buttonState == HIGH) {
      securityOn = !securityOn;
      if (securityOn) {
        Serial.println(">>> Security: ACTIVATED");
        lcd.setCursor(0,0);
        lcd.print("Sec:ON ");
      } else {
        Serial.println(">>> Security: DEACTIVATED");
        lcd.setCursor(0,0);
        lcd.print("Sec:OFF");
      }
    }
  }
  lastSecurityBtn = buttonState;
}
//checking state of light button
void checkLightButton() {
  int buttonState = digitalRead(lightButton);
  //debounce and last state saving
  if (millis() - lightLastPressed >= 50 && buttonState != lastLightBtn) {
    lightLastPressed = millis();
    if (buttonState == HIGH) {
      lightMode = (lightMode + 1) % 3;
      if (lightMode == 0) {
        darkStartTime = 0;
        lcd.setCursor(8,0);
        lcd.print("Light:A");
      } else {
        lcd.setCursor(8,0);
        lcd.print("Light:M");
      }
    }
  }
  lastLightBtn = buttonState;
}

void runLighting() {
  //Always On
  if (lightMode == 1) {
    digitalWrite(indoorLed, HIGH);
  //Always Off
  } else if (lightMode == 2) {
    digitalWrite(indoorLed, LOW);
  //Automatic Lightining  
  } else {
    int ldrValue = analogRead(ldr);
    if (ldrValue < ldrDark) {
      if (darkStartTime == 0) darkStartTime = millis();
      if (millis() - darkStartTime >= darkConfirm_ms) digitalWrite(indoorLed, HIGH);
    } else {
      darkStartTime = 0;
      digitalWrite(indoorLed, LOW);
    }
  }
}

//entering characters in keypad
char scanKeypad() {
  char key = keyPad.getChar();
  //filtering noise 
  if (key != '\0' && key != 'N' && key != 'F' && key != 255 && key >= 32 && key <= 126) {
    while (keyPad.getChar() != 'N' && keyPad.getChar() != '\0') {
         delay(10); // Short debounce hold
    }
    return key;
  }
  return '\0';
}

//function for keypad use with door
void doorAccess() {

//toggle for different use of Keypad
  if (window_security) {
    count = 0;
    return;
  }

  unsigned long currentMillis = millis();


  if (isDoorOpen) {
    if (currentMillis - doorOpenTimer >= 5000) {
      my_servo.write(90);
      lcd.setCursor(0, 1);
      lcd.print("D:close");
      count = 0;
      isDoorOpen = false;
    }
    return;
  }


  if (isDoorLockedOut) {
    if (currentMillis - doorLockoutTick >= 1000) {
      doorLockoutTick = currentMillis;
      if (doorLockoutRemaining >= 1) {
        Serial.print("Door Lockout: ");
        Serial.println(doorLockoutRemaining);
        doorLockoutRemaining--;
      }
    }
    if (currentMillis - doorLockoutTimer >= 20000) {
      Serial.println("Door Lockout ended.");
      trial = 0;
      count = 0;
      isDoorLockedOut = false;
    }
    return;
  }


  char key = scanKeypad();
  if (key != '\0' && count < 4) {
    user[count] = key;
    count++;
    Serial.println(key);
  }

  if (count == 4) {
    bool correct = true;
    for (int i = 0; i < 4; i++) {
      if (user[i] != door_pass[i]) {
        correct = false;
        break;
      }
    }

    if (correct) {
      Serial.println("Correct Door Code");
      lcd.setCursor(0, 1);
      lcd.print("D:open  ");
      my_servo.write(70);
      doorOpenTimer = currentMillis;
      isDoorOpen = true;
      trial = 0;
    } else {
      Serial.println("Wrong Door Code");
      trial++;
      count = 0;
      if (trial >= 4) {
        Serial.println("Door Lockout!");
        doorLockoutRemaining = 20;
        doorLockoutTimer = currentMillis;
        doorLockoutTick = currentMillis;
        isDoorLockedOut = true;
      }
    }
  }
}


void runSecurity() {
  //toggling window Security 
  if (securityOn && digitalRead(reedSwitch) == HIGH && !window_security) {
    window_security = true;
    print = true;
  }

  if (window_security) {
    digitalWrite(buzzer, HIGH);
    window_secure();
  } else {
    digitalWrite(buzzer, LOW);
  }

   
//Security Ultrasonic sensor for detecting movement infront of door
  if (getdistance() <= 13) {
    digitalWrite(doorLed, HIGH);
  } else {
    digitalWrite(doorLed, LOW);
  }
}

//getting distance from ultrasonic 
int getdistance(){
digitalWrite(Trig,0);
delay(5);
digitalWrite(Trig,1);
delay(10);
digitalWrite(Trig,0);
  
duration = pulseIn(Echo,1);

distance = 0.0343*duration/2.0; 
  return distance;
}



void Fan(){
  Serial.print("POT: ");
  Serial.println(analogRead(pot));
  if(millis() - lastPot >= 100){
    lastPot = millis();
    //changing the threshold of turning fan on
    thre_temp = map(analogRead(pot),0,1023,20,40);
  }
  if(dht.readTemperature() >= thre_temp){
    Serial.println("Fan ON");
    analogWrite(fan,map(dht.readTemperature(),20,40,120,255));
  }
  else{
    analogWrite(fan,0);
  }
}


//function for Keypad use with windows alarm 
void window_secure() {
  DHT = false;
  unsigned long currentMillis = millis();

  if (print) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter password  ");
    lcd.setCursor(0,1);
    lcd.print("for security     ");
    print = false;
  }


  if (isAlarmLockedOut) {
    digitalWrite(buzzer, HIGH); 
    if (currentMillis - alarmLockoutTick >= 1000) {
      alarmLockoutTick = currentMillis;
      if (alarmLockoutRemaining >= 1) {
        Serial.print("Alarm Lockout: ");
        Serial.println(alarmLockoutRemaining);
        alarmLockoutRemaining--;
      }
    }

    if (currentMillis - alarmLockoutTimer >= 20000) {
      Serial.println("Alarm Lockout ended.");
      alarmTrial = 0;
      alarmCount = 0;
      isAlarmLockedOut = false;
    }
    return;
  }

  char key = scanKeypad();
  if (key != '\0' && alarmCount < 4) {
    alarmUser[alarmCount] = key;
    alarmCount++;
    Serial.println(key);
  }

  if (alarmCount == 4) {
    bool correct = true;
    for (int i = 0; i < 4; i++) {
      if (alarmUser[i] != security_pass[i]) {
        correct = false;
        break;
      }
    }

    if (correct) {
      Serial.println("Correct Alarm Code");
      window_security = false;
      DHT = true;
      digitalWrite(buzzer, LOW);
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("D:close");
      lcd.setCursor(0,0);
      lcd.print("Sec:ON");
      if (lightMode == 0) {
        lcd.setCursor(8,0);
        lcd.print("Light:A");
      } else {
        lcd.setCursor(8,0);
        lcd.print("Light:M");
      }
      lcd.setCursor(8,1);
      lcd.print("DHT:");
      lcd.setCursor(12,1);
      lcd.print(dht.readTemperature());
      alarmCount = 0;
      alarmTrial = 0;
    } else {
      Serial.println("Wrong Alarm Code");
      alarmTrial++;
      alarmCount = 0;

      if (alarmTrial >= 4) {
        Serial.println("Alarm Lockout!");
        alarmLockoutRemaining = 20;
        alarmLockoutTimer = currentMillis;
        alarmLockoutTick = currentMillis;
        isAlarmLockedOut = true;
      }
    }
  }
}