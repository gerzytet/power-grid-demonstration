// Libary to control 8x8 Dot Matrix Display
#include <Matrix.h>
// Libary to control LCD Display
#include <LiquidCrystal.h>
// Libary to control stepper motor
#include <Stepper.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 31, en = 29, d4 = 28, d5 = 27, d6 = 26, d7 = 25;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// isRaining is true if it is currently raining
bool isRaining = true;
// stepsPerRevvolution is how many steps per revolution
const int stepsPerRevolution = 2038;

// Define Matrix Object that holds the current status of the Dog Matrix
Matrix weatherMatrix(A4,A5);

// LedArray1 is a 2D array that contains the six frames of the Dot Maxtrix Animation 
uint8_t LedArray1[6][8]={{0x00, 0x2A, 0x00, 0x3F, 0x41, 0x41, 0x32, 0x0C},
                        {0x2A, 0x2A, 0x00, 0x3F, 0x41, 0x41, 0x32, 0x0C},
                        {0x2A, 0x00, 0x00, 0x3F, 0x41, 0x41, 0x32, 0x0C},
                        {0x00, 0x00, 0x00, 0x3F, 0x41, 0x41, 0x32, 0x0C},
                        {0x00, 0x00, 0x00, 0x3F, 0x41, 0x41, 0x32, 0x0C},
                        {0x00, 0x00, 0x00, 0x3F, 0x41, 0x41, 0x32, 0x0C}};
uint8_t LEDArray[8];
// Define stepper object with it's control pins
Stepper myStepper = Stepper(stepsPerRevolution, 2, 4, 3, 5);
// lastGameTimeUpdate holds the values in milliseconds since the last in game minute
long lastGameTimeUpdate = 0;

void setup(){
  // Open Serial Port for debugging
  Serial.begin(9600);
  // Start matrix
  weatherMatrix.begin(0x70);
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.clear();
  delay(1000);
  // Display start menu to user
  lcd.print("PRESS BUTTON TO");
  lcd.setCursor(0, 1);
  lcd.print("START");
  waitForStart();
  lcd.clear();
  pinMode(6, INPUT);
  pinMode(33, OUTPUT);
  lastGameTimeUpdate = millis();
}

// GameDayTimeMins is the current amount of minutes in game time
int gameDayTimeMins = 0;
// After every 35 milliseconds add another minute to gameDayTimeMins
long minsPerMili = 35;
// Boolean to hold status of the day
bool isAM = true;
// Integer to hold the current hour in game time
int currentHour;
// runGameClock handles the all the logic involved envolving the in game time changing
void runGameClock(){
  currentHour = (int) gameDayTimeMins / 60;
  // Only execute code if enough time has passed
  if(millis() - lastGameTimeUpdate >= minsPerMili) {
    
    gameDayTimeMins += (millis() - lastGameTimeUpdate) / minsPerMili;
    lastGameTimeUpdate = millis();
  }
  if(gameDayTimeMins >= (11*60) && gameDayTimeMins <= (60 * 23)){
    isAM = false;
  } else {
    isAM = true;
  }
  if(gameDayTimeMins >= (24*60)){
    gameDayTimeMins = 0;
  }

  // set the cursor to column 0, line 1
  lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  if((currentHour % 12)+1 <= 9){
    lcd.print("0");
  }
  // print the current hour
  lcd.print((currentHour % 12)+1);
  lcd.print(":00 ");
  if(isAM){
    lcd.print("AM");
  }else{
    lcd.print("PM");
  }
}

// Thresholds to determine weather conditions 
int becomeCloudyThreshold = 350;
int becomeDayThreshold = 550;
void updateIsRaining() {
  int light = analogRead(A15);
  if (isRaining && light > becomeDayThreshold) {
    isRaining = false;
  }
  if (!isRaining && light < becomeCloudyThreshold) {
    isRaining = true;
  }
}

// LED Matrix Logic
void DrawLED(uint8_t img[8]){
  // Clear the Dot matrix data
  weatherMatrix.clear();
  // Loop through each row of the matrix data and set it to the matrix
  for(int i=0; i<8; i++) {
    LEDArray[i]= img[i];
    for(int j=7; j>=0; j--) {
    if((LEDArray[i]&0x01)>0){
      weatherMatrix.drawPixel(j, i, 1);
    }
    LEDArray[i] = LEDArray[i]>>1;
    }
  }
  // Display the matrix
  weatherMatrix.writeDisplay();
}
// powerPlantStrength determins how much power is generated from the power plant based on a potentiometer input
float powerPlantStrength = 0;

void updateMotor() {
  float powerPlantSetting = analogRead(A14);
  // Require user to spin past 50 to enable power generation
  int powerDeadzone = 50;
  if (powerPlantSetting <= powerDeadzone) {
    powerPlantSetting = 0;
  }

  // Calculate motor speed
  float motorSpeed = powerPlantSetting == 0 ? 0 : map(powerPlantSetting, 0, 1024, 4, 20);
  powerPlantStrength = motorSpeed;
  // Set motor speed and step
  myStepper.setSpeed(motorSpeed);
  myStepper.step(motorSpeed);
}

// Peak demand is the heightest demand possible
int PEAK_DEMAND = 4000;
// percent decrease is used to find the lowest demand
double PERCENT_DECREASE = 0.57;

//for the given hour, return current modeled power demand
double getCurrentDemand() {
    //data source:
    //https://www.eia.gov/electricity/gridmonitor/expanded-view/electric_overview/US48/US48/ElectricityOverview-2/edit
    //We model power consumption as a sine wave over the course of a day.
    //the trough is at midnight, and the peak is at noon

    //calculate relative_demand, which is 0 for the trough, 1 for the peak, and a number linearly in-between for the rest
    double angle = currentHour * (M_PI * 2 / 24);
    double relative_demand = (-cos(angle)/2 + 0.5);

    //remap this sine wave between base and peak demand
    double base_demand = PEAK_DEMAND * PERCENT_DECREASE;
    double difference = PEAK_DEMAND - base_demand;
    return (base_demand + relative_demand*difference);
}

float getCurrentGeneration() {
  int powerPlantGeneration = map(powerPlantStrength, 0, 20, 0, 4000);

  double angle = currentHour * (M_PI * 2 / 24);
  double relative_generation = (-cos(angle)/2 + 0.5);
  int solarGeneration = PEAK_DEMAND / 3 * relative_generation;
  // If it is raining, the solar panel is only at 20% efficiency 
  if (isRaining) {
    solarGeneration *= 0.2;
  }
  return solarGeneration + powerPlantGeneration;
}

// UpdateHouses visually displays the current power reletive to the current demand
void updateHouses() {
  if (getCurrentGeneration() > getCurrentDemand()) {
    analogWrite(A0, 1024);
  } else if (abs(getCurrentDemand() - getCurrentGeneration()) <= 500) {
    analogWrite(A0, sin(millis() / 100) * 1024);
  } else {
    analogWrite(A0, 0);
  }
}

// updatePowerDisplay updates the current demand and supply variables on the LCD
void updatePowerDisplay() {
  lcd.setCursor(0, 0);
  lcd.print("POWER: ");
  int generation = getCurrentGeneration() / 10;
  if (generation <= 9) {
    lcd.print(0);
  }
  if (generation <= 99) {
    lcd.print(0);
  }
  lcd.print(generation);
  lcd.print("/");
  lcd.print((int) (getCurrentDemand() / 10));
}

// rainC is the current rain frame on the Dot Matrix animation
int rainC = 0;
long lastRainFrame = 0;

// Function to wait until start button is pressed to continue
void waitForStart() {
  while (!digitalRead(6)) {
    delay(20);
  }
}
bool paused = false;


void updatePausedIndicator() {
  lcd.setCursor(9, 1);
  if (paused) {
    lcd.print("PAUSED");
  } else {
    lcd.print("      ");
  }
}

bool lastButtonStatus = true;

// Main logic loop
void loop(){
  bool button = digitalRead(6) != 0;
  // Pause game logic
  if (!lastButtonStatus && button) {
    paused = !paused;
    if (!paused) {
      lastGameTimeUpdate = millis();
    }
  }
  updatePausedIndicator();
  lastButtonStatus = button;
  updatePowerDisplay();
  updateIsRaining();
  updateMotor();
  if (!paused) {
    runGameClock();
  }
  if(isRaining){
    DrawLED(LedArray1[rainC]);
    // 300 is the delay in millis between frames
    if(millis() - lastRainFrame >= 300){
      lastRainFrame = millis();
      rainC++;
      if(rainC == 6){
        rainC = 0;
      }
    }  
  }else{
      weatherMatrix.clear();
      weatherMatrix.writeDisplay();
  }
  updateHouses();
}
