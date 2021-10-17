#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <SPI.h>
#include <Wire.h>
#include <JC_Button.h>

//Pin Definition
#define triggerPin 8
#define solenoidGo 5
#define flywheelPin 10
#define revPin 7
#define magOutPin 4
#define jamDoorPin 9
#define voltageRead A0

// Configuration Options
int revDelay = 300;                                         //Default flywheel rev delay
byte burstSize = 2;                                         //Default qty of darts to fire in burst mode
byte rateOfFire = 50;                                       //Default rate of fire percentage
byte rateOfFireDelayMin = 30;                               //Max rate of fire delay between shots
byte rateOfFireDelayMax = 150;                              //Min rate of fire delay between shots
byte solenoidRoF;
byte flywheelPower = 50;                                    //Default flywheel power
byte fireMode = 0;                                          //Starts blaster in safety mode. 0=Safety, 1=Single Shot, 2=Burst Fire, 3=Full Auto
byte dartsFired = 0;                                        //Counts how many darts are fired for each trigger pull
int totalDartsFired = 0;                                    //Keeps track of total darts fired between blaster resets
int dartsToFire = 0;                                        //Counts how many darts are left to fire for each trigger pull
int ammoLeft;                                               //Counts remaining ammo in magazine
int menuitem = 1;                                           //Used for configuration menu
int page = 1;                                               //Used for configuration menu
int magArray[9] = {6, 10, 12, 15, 18, 22, 25, 35, 50};      //Magazine size array for selection of mag size when mag is out
byte arrayIndex = 0;
byte magSize = magArray[5];                                 //Default magazine size
unsigned long currentMillis;
unsigned long previousMillis = 0;
unsigned long transitionMillis;
unsigned long previousTransitionMillis = 0;

//voltage measurement
float vout = 0.0;
float batteryVoltage = 0.0;
float R1 = 100000.0;
float R2 = 10000.0;
unsigned char sampleCount = 0;
int sumBatteryValue = 0;
byte avValue = 0;
byte samples = 10;
int batteryMin = 9.6;

//Other Settings
bool normalMode = true;
bool magOut = false;
bool configMode = false;
bool aDartIsFired = false;
bool fullAuto = false;
bool lowBattery = false;

//For buttons/ switches
#define PULLUP true                                                            //Internal pullup, so we dont need to wire resistor
#define INVERT true                                                            //Invert required for proper readings with pullup
#define DEBOUNCE_MS 10                                                         //Check btn time every 10ms

Button triggerSwitch (triggerPin, PULLUP, INVERT, DEBOUNCE_MS);                //Trigger button, using the library
Button revSwitch (revPin, PULLUP, INVERT, DEBOUNCE_MS);                        //Firemode selector switch, using the library
Button magOutSwitch (magOutPin, PULLUP, INVERT, DEBOUNCE_MS);                  //Magout switch, using the library
Button jamDoorSwitch (jamDoorPin, PULLUP, INVERT, DEBOUNCE_MS);                //Jamdoor switch, using the library

// Display Setup
#define SCREEN_WIDTH 128                                                       // OLED display width, in pixels
#define SCREEN_HEIGHT 64                                                       // OLED display height, in pixels
#define OLED_RESET    -1                                                       // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {

  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  display.clearDisplay();

  //  Display Message
  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(28, 20);
  display.print(F("NERF"));
  display.setCursor(22, 50);
  display.print(F("BLASTER"));
  display.display();
  delay(1500);
  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.print(F("BOOTING UP"));
  display.setTextSize(2);
  display.setCursor(55, 50);
  display.print(F("3"));
  display.display();
  delay(500);
  display.setCursor(55, 50);
  display.setTextColor(BLACK);
  display.print(F("3"));
  display.setCursor(55, 50);
  display.setTextColor(WHITE);
  display.print(F("2"));
  display.display();
  delay(500);
  display.setCursor(55, 50);
  display.setTextColor(BLACK);
  display.print(F("2"));
  display.setCursor(55, 50);
  display.setTextColor(WHITE);
  display.print(F("1"));
  display.display();
  delay(500);

  //PinModes
  pinMode(triggerPin, INPUT_PULLUP);
  pinMode(revPin, INPUT_PULLUP);
  pinMode(magOutPin, INPUT_PULLUP);
  pinMode(jamDoorPin, INPUT_PULLUP);
  pinMode(voltageRead, INPUT);
  pinMode(solenoidGo, OUTPUT);
  pinMode(flywheelPin, OUTPUT);

  //Holding the rev and trigger during booting will put the blaster at full power without needing to go into the config menu
  revSwitch.read();
  triggerSwitch.read();
  if (revSwitch.isPressed() && triggerSwitch.isPressed()) {
    rateOfFire = 100;
    flywheelPower = 100;
  }
  ammoLeft = magSize;
}

void loop() {
  jamDoorSwitch.read();                   //Listening for jam door being opened
  magOutSwitch.read();                    //Listening for magazine removal
  triggerSwitch.read();                   //Listening to trigger being pressed
  revSwitch.read();                       //Listening to rev trigger being pressed

  voltageMonitor();

  if (!lowBattery && !jamDoorSwitch.isPressed()) {
    configMode = true;
    magOut = false;
    normalMode = false;
    configDisplay();
    menuHandling();
    resetDartsFired();
  }
  else if (!lowBattery && !magOutSwitch.isPressed()) {
    ammoLeft = magSize;
    configMode = false;
    magOut = true;
    normalMode = false;
    magOutDisplay();
    resetDartsFired();
    //    magSelection();
    if (triggerSwitch.wasPressed()) {
      arrayIndex++;
      if (arrayIndex >= 9) {
        arrayIndex = 0;
      }
      magSize = magArray[arrayIndex];
      ammoLeft = magSize;
    }
  }
  else if (lowBattery) {
    LowBatteryDisplay();
    configMode = false;
    magOut = false;
    normalMode = false;
  }
  else {
    configMode = false;
    magOut = false;
    normalMode = true;
    normalDisplay();
    toggleFireModes();
    //    fireDart();
    //    cycleControl();
    checkForDartsFired();
    selectFire();
    menuitem = 1;
    page = 1;
  }
}

//Function: Changes between firing modes upon each press of the rev trigger
void toggleFireModes() {
  if (normalMode) {
    if (revSwitch.wasPressed()) {                                  //Check if the rev trigger was pressed to change fire modes
      fireMode++;                                                  //Increment fireMode
    }
    if (fireMode == 0) {
    }
    if (fireMode == 1) {
      dartsToFire = 1;
    }
    if (fireMode == 2) {
      dartsToFire = burstSize;
    }
    if (fireMode == 3) {
      dartsToFire = 0;
    }
    if (fireMode > 3) {
      fireMode = 0;                                                 //Reset fireMode to 0 if >3
    }
  }
}

//Function: Handles selectfire control
void selectFire () {
  if (normalMode) {
    currentMillis = millis();
    if (triggerSwitch.isPressed() && fireMode == 0) {                                                 //Check if in safety mode if trigger is pressed
      digitalWrite(solenoidGo, LOW);
      digitalWrite(flywheelPin, LOW);
    }
    else if (triggerSwitch.wasPressed() && (fireMode == 1 || fireMode == 2) && ammoLeft > 0) {        //If in burst fire or single shot mode
      aDartIsFired = true;                                                                            //Allow for darts to be fired, handled elsewhere
    }
    else if (triggerSwitch.isPressed() && fireMode == 3 && ammoLeft > 0) {                            //If full auto turn on motors
      fullAuto = true;
    }
    else if (!triggerSwitch.isPressed()) {                                                            //if trigger isn't pressed
      if (fireMode == 3) {                                                                            //if firemode is fullauto or safety, turn off motor
        resetDartsFired();
      }
      else if (!aDartIsFired && (fireMode == 1 || fireMode == 2 )) {
        resetDartsFired();
      }
    }
  }
}

//Function: Checks that a dart has been fired
void checkForDartsFired () {
  if (aDartIsFired) {
    dartsToFire = dartsToFire - dartsFired;                                          //Determine max amounts of darts to be fired
    if (dartsToFire > 0 && ammoLeft > 0) {
      analogWrite(flywheelPin, flywheelPower * 2.55);                                //PWM to flyhweel motor flywheel power pecentage x 2.55 to give PWM range from 0 to 255
      if (currentMillis - previousMillis >= revDelay) {                              //Delay between flywheels starting and solenoid starting
        fireDart();                                                                  //Fire dart
      }
    }
    else if ((dartsFired >= dartsToFire) || ammoLeft == 0) {                         //If can't fire anymore darts
      resetDartsFired();                                                             //Reset darts fired stuff so it can happen again
    }
  }
  if (fullAuto) {
    dartsFired = 0;
    analogWrite(flywheelPin, flywheelPower * 2.55);
    if (currentMillis - previousMillis >= revDelay) {
      fireDart();
    }
    else if (!triggerSwitch.isPressed() && dartsFired >= 1) {
      resetDartsFired();
    }
  }
}

//Function: to handle dart firing
void fireDart() {
  solenoidRoF = rateOfFireDelayMin + (rateOfFireDelayMax - rateOfFireDelayMin) * (rateOfFire - 100) / (30 - 100); //Solenoid timing
  digitalWrite(solenoidGo, HIGH);
  delay(40);
  digitalWrite(solenoidGo, LOW);
  delay(solenoidRoF);
  dartsFired++;
  ammoLeft--;
  totalDartsFired++;
  if (ammoLeft <= 0) {
    ammoLeft = 0;
  }
}

//Function: Turns off motors and resets dart counters once darts have been fired
void resetDartsFired () {
  digitalWrite(solenoidGo, LOW);                                    //Turn off solenoid
  digitalWrite(flywheelPin, LOW);                                 //Turn off flywheel motors
  dartsFired = 0;                                                 //Reset darts fired to 0
  aDartIsFired = false;                                           //No longer checking if darts are being fired
  fullAuto = false;                                               //Turn off full auto
  previousMillis = currentMillis;
}

//Function; Checks battery voltage.
void voltageMonitor() {
  static byte counter = 0;
  counter++;
  if (counter < 250) {
    return;
    counter = 0;
  }
  if (sampleCount < samples) {
    sumBatteryValue += analogRead(voltageRead);
    sampleCount++;
  }
  else {
    avValue = sumBatteryValue / samples;
    vout = (avValue * 5.0) / 1024.0;
    batteryVoltage = (vout / (R2 / (R1 + R2))) * 1.05;
    sampleCount = 0;
    avValue = 0;
    sumBatteryValue = 0;
    vout = 0;
    if (batteryVoltage < batteryMin) {
      lowBattery = true;
    }
    else {
      lowBattery = false;
    }
  }
}

//Function: Changes blaster settings if in configuration or magout modes
void menuHandling() {
  if (configMode) {
    if (revSwitch.wasPressed()) {
      page++;
      if (page > 2) {
        page = 1;
      }
    }
    if (triggerSwitch.wasPressed() && page == 1) {
      menuitem++;
      if (menuitem > 4) {
        menuitem = 1;
      }
    }
    if (triggerSwitch.wasPressed() && menuitem == 1 && page == 2 ) {
      flywheelPower = flywheelPower + 10;
      if (flywheelPower > 100) {
        flywheelPower = 30;
      }
    }
    else if (triggerSwitch.wasPressed() && menuitem == 2 && page == 2 ) {
      rateOfFire = rateOfFire + 10;
      if (rateOfFire > 100) {
        rateOfFire = 30;
      }
    }
    else if (triggerSwitch.wasPressed() && menuitem == 3 && page == 2 ) {
      burstSize++;
      if (burstSize > 5) {
        burstSize = 2;
      }
    }
    else if (triggerSwitch.wasPressed() && menuitem == 4 && page == 2 ) {
      revDelay = revDelay + 50;
      if (revDelay > 750) {
        revDelay = 50;
      }
    }
  }
}

//Function: Draws normal display when blaster is in normal mode
void normalDisplay() {
  display.clearDisplay();
  display.setFont();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 2);
  display.print(batteryVoltage, 1);
  display.println(F("V"));
  display.setTextSize(1);
  display.setCursor(0, 22);
  display.print(F("POWER:"));
  display.print(flywheelPower);
  display.print(F("%"));
  display.setCursor(0, 32);
  display.print(F("RoF:"));
  display.print(rateOfFire);
  display.print(F("%"));
  display.setCursor(0, 42);
  display.print(F("MODE:"));
  if (fireMode == 0) {
    display.print(F("SAFETY"));
  }
  else if (fireMode == 1) {
    display.print(F("SINGLE"));
  }
  else if (fireMode == 2) {
    display.print(F("BURST"));
  }
  else if (fireMode == 3) {
    display.print(F("FULL AUTO"));
  }
  display.setCursor(0, 52);
  display.print(F("BURST SIZE:"));
  display.print(burstSize);
  display.drawCircle(102, 25, 25, WHITE);
  display.drawCircle(102, 25, 24, WHITE);
  if (ammoLeft >= 20) {
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(2);
    display.setCursor(81, 35);
    display.print(ammoLeft);
    display.setTextSize(1);
  }
  else if (ammoLeft >= 10 && ammoLeft < 20) {
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(2);
    display.setCursor(79, 35);
    display.print(ammoLeft);
    display.setTextSize(1);
  }
  else if (ammoLeft < 10) {
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(2);
    display.setCursor(92, 35);
    display.print(ammoLeft);
    display.setTextSize(1);
  }
  if (ammoLeft > 3) {
    display.setFont();
    display.setTextSize(1);
    display.setCursor(91, 52);
    display.println(F("AMMO"));
  }
  else if (ammoLeft <= 3 && ammoLeft > 0) {
    display.setFont();
    display.setTextSize(1);
    display.setCursor(78, 52);
    display.println(F("AMMO LOW"));
  }
  else if (ammoLeft == 0) {
    display.setFont();
    display.setTextSize(1);
    display.setCursor(83, 52);
    display.println(F("*EMPTY*"));
  }
  display.display();
}

//Function: Draws configuration display when jamdoor is opened
void configDisplay() {
  if (configMode) {
    display.clearDisplay();
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 12 );
    display.print(F("CONFIG MODE"));
    display.drawFastHLine(0, 15, 128, WHITE);
    display.drawFastHLine(0, 16, 128, WHITE);
    display.setFont();
    if (page == 1 && menuitem == 1)
    {
      display.setCursor(5, 20);
      display.print(F(">"));
      display.setCursor(15, 20);
      display.print(F("POWER:"));
      display.setCursor(95, 20);
      display.print(flywheelPower);
      display.print(F("%"));
      display.setCursor(15, 30);
      display.print(F("RoF:"));
      display.setCursor(95, 30);
      display.print(rateOfFire);
      display.print(F("%"));
      display.setCursor(15, 40);
      display.print(F("BURST SIZE:"));
      display.setCursor(95, 40);
      display.print(burstSize);
      display.setCursor(15, 50);
      display.print(F("SHOT DELAY:"));
      display.setCursor(95, 50);
      display.print(revDelay);
    }
    else if (page == 1 && menuitem == 2)
    {
      display.setCursor(15, 20);
      display.print(F("POWER:"));
      display.setCursor(95, 20);
      display.print(flywheelPower);
      display.print(F("%"));
      display.setCursor(5, 30);
      display.print(F(">"));
      display.setCursor(15, 30);
      display.print(F("RoF:"));
      display.setCursor(95, 30);
      display.print(rateOfFire);
      display.print(F("%"));
      display.setCursor(15, 40);
      display.print(F("BURST SIZE:"));
      display.setCursor(95, 40);
      display.print(burstSize);
      display.setCursor(15, 50);
      display.print(F("SHOT DELAY:"));
      display.setCursor(95, 50);
      display.print(revDelay);
    }
    else if (page == 1 && menuitem == 3)
    {
      display.setCursor(15, 20);
      display.print(F("POWER:"));
      display.setCursor(95, 20);
      display.print(flywheelPower);
      display.print(F("%"));
      display.setCursor(15, 30);
      display.print(F("RoF:"));
      display.setCursor(95, 30);
      display.print(rateOfFire);
      display.print(F("%"));
      display.setCursor(5, 40);
      display.print(F(">"));
      display.setCursor(15, 40);
      display.print(F("BURST SIZE:"));
      display.setCursor(95, 40);
      display.print(burstSize);
      display.setCursor(15, 50);
      display.print(F("SHOT DELAY:"));
      display.setCursor(95, 50);
      display.print(revDelay);
    }
    else if (page == 1 && menuitem == 4)
    {
      display.setCursor(15, 20);
      display.print(F("POWER:"));
      display.setCursor(95, 20);
      display.print(flywheelPower);
      display.print(F("%"));
      display.setCursor(15, 30);
      display.print(F("RoF:"));
      display.setCursor(95, 30);
      display.print(rateOfFire);
      display.print(F("%"));
      display.setCursor(15, 40);
      display.print(F("BURST SIZE:"));
      display.setCursor(95, 40);
      display.print(burstSize);
      display.setCursor(5, 50);
      display.print(F(">"));
      display.setCursor(15, 50);
      display.print(F("SHOT DELAY:"));
      display.setCursor(95, 50);
      display.print(revDelay);
    }
    if (page == 2 && menuitem == 1)
    {
      display.setFont(&FreeSans9pt7b);
      display.setTextSize(1);
      display.setCursor(35, 35);
      display.print(F("Power:"));
      if (flywheelPower < 100) {
        display.setCursor(45, 55);
        display.print(flywheelPower); //OK
        display.print(F("%"));
      }
      if (flywheelPower == 100) {
        display.setCursor(35, 55);
        display.print(flywheelPower); //OK
        display.print(F("%"));
      }
    }
    else if (page == 2 && menuitem == 2)
    {
      display.setFont(&FreeSans9pt7b);
      display.setTextSize(1);
      display.setCursor(45, 35);
      display.print(F("RoF:"));
      if (rateOfFire < 100) {
        display.setCursor(45, 55);
        display.print(rateOfFire); //OK
        display.print(F("%"));
      }
      if (rateOfFire == 100) {
        display.setCursor(35, 55);
        display.print(rateOfFire); //OK
        display.print(F("%"));
      }
    }
    else if (page == 2 && menuitem == 3)
    {
      display.setFont(&FreeSans9pt7b);
      display.setTextSize(1);
      display.setCursor(23, 35);
      display.print(F("Burst Size:"));
      display.setCursor(60, 55);
      display.print(burstSize); //OK
    }
    else if (page == 2 && menuitem == 4)
    {
      display.setFont(&FreeSans9pt7b);
      display.setTextSize(1);
      display.setCursor(23, 35);
      //      display.print("Burst Size:");
      display.print(F("Shot Delay:"));
      display.setCursor(50, 55);
      display.print(revDelay); //OK
    }
    display.display();
  }
}

//Function: Draws magout display when magazine removed
void magOutDisplay() {
  if (magOut) {
    display.clearDisplay();
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(20, 12 );
    display.print(F("MAG OUT"));
    display.drawFastHLine(0, 15, 128, WHITE);
    display.setFont();
    display.setTextColor(WHITE);
    display.setCursor(10, 20);
    display.print(F("TOTAL DARTS"));
    display.setCursor(10, 30);
    display.print(F("      FIRED"));
    display.setCursor(10, 42);
    display.print(F("    SET MAG"));
    display.setCursor(10, 52);
    display.print(F("       SIZE"));
    display.setTextSize(2);
    display.setCursor(75, 21 );
    display.print(F(":"));
    display.setCursor(85, 21 );
    display.print(totalDartsFired);
    display.setCursor(75, 43 );
    display.print(F(":"));
    display.setCursor(85, 43 );
    display.print(magSize);
    display.drawRoundRect(1, 18, 126, 21, 5, WHITE);
    display.drawRoundRect(1, 40, 126, 21, 5, WHITE);
    display.display();
  }
}

//Function: Draws low battery display when battery is too low
void LowBatteryDisplay() {
  display.clearDisplay();
  display.drawRoundRect(12, 7, 100, 50, 7, WHITE);
  display.drawRoundRect(13, 8, 98, 48, 5, WHITE);
  display.fillRect(112, 17, 10, 30, WHITE);
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(42, 28);
  display.println(F("LOW"));
  display.setCursor(18, 48);
  display.println(F("BATTERY!"));
  display.display();
  delay(500);
  display.fillRoundRect(16, 11, 92, 42, 3, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(42, 28);
  display.println(F("LOW"));
  display.setCursor(18, 48);
  display.println(F("BATTERY!"));
  display.display();
  delay(500);
}
