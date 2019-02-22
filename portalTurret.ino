#include <WaveHC.h>//some code taken from playbyindex example in waveHC library
#include <WaveUtil.h>
//#include <Servo.h>//incompatible with wave shield

#define doorServoPin 9
#define eyePin 8
#define xAccPin 6
#define yAccPin 7

//controls over the actions
#define openAngle 150
#define closedAngle 35
#define accelerometerThreshold 100

// Number of files of each type.
#define ACTIVE_FILE_COUNT 13
#define DISABLED_FILE_COUNT 7
#define FIRE_FILE_COUNT 1
#define PICKUP_FILE_COUNT 11
#define RETIRE_FILE_COUNT 4
#define SEARCH_FILE_COUNT 7
#define TIP_FILE_COUNT 7

//global variables
//Servo doorServo;
const uint16_t numSignalPoints = 50;//don't go over 200
uint16_t eyeSignal[numSignalPoints];
long timeStart = 0;
long lastMeasurement = 0;
long baselineTimer = 0;
bool doorOpen = false;
bool tipped = false;
double eyeSignalAverage = 0; 
double eyeSignalStdev = 0;
int xBaseline;
int yBaseline;
uint16_t currentEyeLevel = 0;

// indices of turret voice files in the root directory
uint16_t activefileIndex[ACTIVE_FILE_COUNT];
uint16_t disabledfileIndex[DISABLED_FILE_COUNT];
uint16_t firefileIndex[FIRE_FILE_COUNT];
uint16_t pickupfileIndex[PICKUP_FILE_COUNT];
uint16_t retirefileIndex[RETIRE_FILE_COUNT];
uint16_t searchfileIndex[SEARCH_FILE_COUNT];
uint16_t tipfileIndex[TIP_FILE_COUNT];

SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the volumes root directory
FatReader file;   // This object represent the WAV file 
WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time

/*
 * Define macro to put error messages in flash memory
 */
#define error(msg) error_P(PSTR(msg))

//setup the arduino pin functions
void setup() {
  Serial.begin(9600);
  pinMode(eyePin,OUTPUT);
  pinMode(xAccPin,INPUT);
  pinMode(yAccPin,INPUT);
  digitalWrite(eyePin,HIGH);
  manualMoveDoors(doorServoPin, openAngle, closedAngle, 2);
  /*
  doorServo.attach(9);//vestige of servo library
  //doorServo.write(openAngle);//set at open position
  delay(1000);
  moveDoors(doorServo, openAngle, closedAngle, 2);
  doorServo.detach();
  */

  //then initialize the serial monitor and 
  //index the files for voice
  if (!card.init()) error("card.init");

  // enable optimized read - some cards may timeout
  card.partialBlockRead(true);

  if (!vol.init(card)) error("vol.init");

  if (!root.openRoot(vol)) error("openRoot");

  PgmPrintln("Indexing files");
  indexFiles();
  playByIndex(activefileIndex[4]);
  Serial.println(F("begin"));
  timeStart = millis();
  baselineTimer = timeStart;
  randomSeed(analogRead(1));
  //update the accelerometer data
  xBaseline = pulseIn(xAccPin, HIGH);
  yBaseline = pulseIn(yAccPin, HIGH);
}

//main loop
void loop() {
  //update the accelerometer data
  int pulseX = pulseIn(xAccPin, HIGH);
  int pulseY = pulseIn(yAccPin, HIGH);
  
  //update the eye sensor data array if we've past a time increment
  if(millis() - lastMeasurement > 20)
  {
    for(byte k = numSignalPoints - 1; k > 0; k--)
    {
      //shift the data in the array
      eyeSignal[k] = eyeSignal[k - 1];
      //Serial.println(eyeSignal[k]);
    }
    //Serial.println("end");
    currentEyeLevel = analogRead(0);
    eyeSignal[0] = currentEyeLevel;
   
    lastMeasurement = millis();
    
    //update some statistics about the eye sensor data
    eyeSignalAverage = 0; 
    eyeSignalStdev = 0;
    //width = 0;
    for(byte i = 0; i < numSignalPoints; i++)
    {
      //find the average
      eyeSignalAverage += eyeSignal[i]/(numSignalPoints*1.00);
    }
    
    
    for(byte j = 0; j < numSignalPoints; j++)
    {
      //find the standard deviation
      double deviation = eyeSignal[j] - eyeSignalAverage;
      deviation = deviation*deviation;
      eyeSignalStdev += deviation/((numSignalPoints*1.00) - 1.00);
    }
    eyeSignalStdev = sqrt(eyeSignalStdev);
  }
  
    //Serial.println(currentEyeLevel);
    //Serial.println(eyeSignalAverage);
    //Serial.println(eyeSignalStdev);
    //Serial.println(slope);
    
  double triggerLevel = eyeSignalStdev;
  //Serial.println(triggerLevel);

  //make some short calculations with the accelerometer data
  int xTip = pulseX - xBaseline;
  int yTip = pulseY - yBaseline;
  xTip = abs(xTip);
  yTip = abs(yTip);
   Serial.println(xTip);
  
  //**********************************************************//
  //then, act on the sensor information
  if(triggerLevel > 1 && doorOpen == false && millis() - timeStart >= 2000 && !tipped)//2 sec debounce so electrical noise does not trigger a re-open
  {
    //open doors
    Serial.println(F("open"));
    /*doorServo.attach(9);//vestige of servo library
    switchDoorState(doorServo, doorOpen);
    doorServo.detach();*/
    timeStart = millis();
    doorOpen = !doorOpen;
    manualMoveDoors(doorServoPin, closedAngle, openAngle, 2);
    uint8_t randNum = random(0,ACTIVE_FILE_COUNT);
    playByIndex(activefileIndex[randNum]);//play 'activating' sound
  }
  else if(triggerLevel > 1 && doorOpen == true && millis() - timeStart >= 200)
  {
    //keep doors open
    //shoot gun sound
    playByIndex(firefileIndex[0]);//play 'firing' sound
    timeStart = millis();
  }
  else if(triggerLevel < 1 && doorOpen == true && millis() - baselineTimer >= 5000)
  {
    //search
    baselineTimer = millis();
    uint8_t randNum = random(0,SEARCH_FILE_COUNT);
    playByIndex(searchfileIndex[randNum]);//play 'searching' sound
  }

  if(doorOpen == true && millis() - timeStart >= 10000)
  {
    //close the doors
    Serial.println(F("close"));
    uint8_t randNum = random(0,RETIRE_FILE_COUNT);
    playByIndex(retirefileIndex[randNum]);//play 'shutting down' sound
    
    /*doorServo.attach(9);//vestige of servo library
    switchDoorState(doorServo, doorOpen);
    doorServo.detach();*/
    timeStart = millis();
    doorOpen = !doorOpen;
    manualMoveDoors(doorServoPin, openAngle, closedAngle, 2);
  }

  if((xTip > accelerometerThreshold || yTip > accelerometerThreshold) && !tipped)
  {
    //the turret is tipped
    if(doorOpen == true)
    {
      //we should close the doors
      doorOpen = !doorOpen;
      manualMoveDoors(doorServoPin, openAngle, closedAngle, 1);
    }
    uint8_t randNum = random(0,TIP_FILE_COUNT);
    playByIndex(tipfileIndex[randNum]);//play 'tipping' sound

    randNum = random(0,DISABLED_FILE_COUNT);
    playByIndex(disabledfileIndex[randNum]);//play 'disabling' sound

    tipped = !tipped;
  }
  else if(xTip < accelerometerThreshold && yTip < accelerometerThreshold && tipped)
  {
    //the turret was stood back up
    uint8_t randNum = random(0,PICKUP_FILE_COUNT);
    playByIndex(pickupfileIndex[randNum]);//play 'picking up' sound
    tipped = !tipped;
  }
}
/*
void moveDoors(Servo myServo, byte startAngle, byte endAngle, double transitionTime)//vestige of servo library
{
  //moves from startAngle to endAngle over transitionTime seconds
  byte stepSize = (endAngle - startAngle)/(transitionTime*10);
  myServo.write(startAngle);
  byte angle = startAngle;
  long startTime = millis();
  while (millis()-startTime < transitionTime*1000)
  {
    angle += stepSize;
    myServo.write(angle);
    delay(100);
  }
  myServo.write(endAngle);
}

void switchDoorState(Servo myServo, bool &doorState)//vestige of servo library
{
  if(doorState)
  {
    //the doors are open, so we want to close them
    //close the doors a bit too far, then move the servo back to the correct end place
    if(closedAngle > 40)
    {
      moveDoors(doorServo, openAngle, closedAngle-40, 2);//close the doors
    }
    else
    {
      moveDoors(doorServo, openAngle, closedAngle, 2);//close the doors
    }
    delay(100);
    myServo.write(closedAngle);
  }
  else
  {
    //the doors are closed, so we want to open them
    if(openAngle < 160)
    {
      moveDoors(doorServo, closedAngle, openAngle+20, 2);//open the doors
    }
    else
    {
      moveDoors(doorServo, closedAngle, openAngle, 2);//open the doors
    }
    myServo.write(openAngle);
  }
  doorState = !doorState;
}
*/

void manualMoveDoors(byte pin, uint8_t startAngle, uint8_t endAngle, double transitionTime)
{
  //manually switches a servo on 'pin' from startAngle to endAngle over transitionTime seconds
  pinMode(pin,OUTPUT);
  uint16_t servoMaxpulse = 2400;//microseconds, longest allowable pulse
  uint16_t servoMinpulse = 544;//microseconds, shortest allowable pulse
  uint8_t longPulse = 20 - servoMaxpulse/1000;//this delay is in ms, not microseconds
  uint16_t startPulse = map(startAngle,0,180,servoMinpulse,servoMaxpulse);
  uint16_t endPulse = map(endAngle,0,180,servoMinpulse,servoMaxpulse);
  
  uint32_t highTime;
  uint32_t timerStart = micros();
  uint32_t elapsedTime = 0;
  uint32_t transitionTimeMicros = transitionTime*1000000;
  /*
  Serial.print(transitionTimeMicros);
  Serial.print(",");
  Serial.print(startPulse);
  Serial.print(",");
  Serial.println(endPulse);
  */
  while(elapsedTime < transitionTimeMicros)
  {
    //highTime = map(elapsedTime,0,transitionTimeMicros,startPulse,endPulse);
    int difference = (endPulse-startPulse);
    float slope = (float) difference / (float) transitionTime;
    float fracTime = (float) elapsedTime / (float) 1000000;
    highTime = slope * fracTime + startPulse;

    /*
    Serial.print(fracTime);
    Serial.print(",");
    Serial.print(elapsedTime);
    Serial.print(",");
    Serial.println(highTime);
    */
    digitalWrite(pin, HIGH);
    delayMicroseconds(highTime);
    digitalWrite(pin,LOW);
    delayMicroseconds(servoMaxpulse - highTime);
    delay(longPulse);
    elapsedTime = micros() - timerStart;
  }

  //then run the servo for an extra 200 ms to let the doors pull or push tighter
  elapsedTime = 0;
  while(elapsedTime < 200000)
  {
    digitalWrite(pin, HIGH);
    delayMicroseconds(highTime);
    digitalWrite(pin,LOW);
    delayMicroseconds(servoMaxpulse - highTime);
    delay(longPulse);
    elapsedTime = micros() - timerStart;
  }
  
}

/*
 * print error message and halt
 */
void error_P(const char *str) {
  PgmPrint("Error: ");
  SerialPrint_P(str);
  sdErrorCheck();
  while(1);
}
/*
 * print error message and halt if SD I/O error, great for debugging!
 */
void sdErrorCheck(void) {
  if (!card.errorCode()) return;
  PgmPrint("\r\nSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  PgmPrint(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}


/*
 * Find files and save file index.  A file's index is is the
 * index of it's directory entry in it's directory file. 
 */
void indexFiles(void) {
  //character array for filename indexing
  char fileLetter[] =  {'1', '2', '3', '4', '5', '6', 
      '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'}; 
  char name[15];
  
  // copy flash string to RAM
  strcpy_P(name, PSTR("activex.wav"));
  
  for (uint8_t i = 0; i < ACTIVE_FILE_COUNT; i++) {
    
    // Make file name
    name[6] = fileLetter[i];
    //Serial.println(name);
    // Open file by name
    if (!file.open(root, name)) error("open by name: active");
    
    // Save file's index (byte offset of directory entry divided by entry size)
    // Current position is just after entry so subtract one.
    activefileIndex[i] = root.readPosition()/32 - 1;   
  }
  
  // copy flash string to RAM
  strcpy_P(name, PSTR("disablex.wav"));
  
  for (uint8_t i = 0; i < DISABLED_FILE_COUNT; i++) {
    
    // Make file name
    name[7] = fileLetter[i];
    //Serial.println(name);
    // Open file by name
    if (!file.open(root, name)) error("open by name: disabled");
    
    // Save file's index (byte offset of directory entry divided by entry size)
    // Current position is just after entry so subtract one.
    disabledfileIndex[i] = root.readPosition()/32 - 1;   
  }

  // copy flash string to RAM
  strcpy_P(name, PSTR("firex.wav"));
  
  for (uint8_t i = 0; i < FIRE_FILE_COUNT; i++) {
    
    // Make file name
    name[4] = fileLetter[i];
    
    // Open file by name
    if (!file.open(root, name)) error("open by name: fire");
    
    // Save file's index (byte offset of directory entry divided by entry size)
    // Current position is just after entry so subtract one.
    firefileIndex[i] = root.readPosition()/32 - 1;   
  }

  // copy flash string to RAM
  strcpy_P(name, PSTR("pickupx.wav"));
  
  for (uint8_t i = 0; i < PICKUP_FILE_COUNT; i++) {
    
    // Make file name
    name[6] = fileLetter[i];
    
    // Open file by name
    if (!file.open(root, name)) error("open by name: pickup");
    
    // Save file's index (byte offset of directory entry divided by entry size)
    // Current position is just after entry so subtract one.
    pickupfileIndex[i] = root.readPosition()/32 - 1;   
  }

  // copy flash string to RAM
  strcpy_P(name, PSTR("retirex.wav"));
  
  for (uint8_t i = 0; i < RETIRE_FILE_COUNT; i++) {
    
    // Make file name
    name[6] = fileLetter[i];
    
    // Open file by name
    if (!file.open(root, name)) error("open by name: retire");
    
    // Save file's index (byte offset of directory entry divided by entry size)
    // Current position is just after entry so subtract one.
    retirefileIndex[i] = root.readPosition()/32 - 1;   
  }

  // copy flash string to RAM
  strcpy_P(name, PSTR("searchx.wav"));
  
  for (uint8_t i = 0; i < SEARCH_FILE_COUNT; i++) {
    
    // Make file name
    name[6] = fileLetter[i];
    
    // Open file by name
    if (!file.open(root, name)) error("open by name: search");
    
    // Save file's index (byte offset of directory entry divided by entry size)
    // Current position is just after entry so subtract one.
    searchfileIndex[i] = root.readPosition()/32 - 1;   
  }

  // copy flash string to RAM
  strcpy_P(name, PSTR("tipx.wav"));
  
  for (uint8_t i = 0; i < TIP_FILE_COUNT; i++) {
    
    // Make file name
    name[3] = fileLetter[i];
    
    // Open file by name
    if (!file.open(root, name)) error("open by name: tip");
    
    // Save file's index (byte offset of directory entry divided by entry size)
    // Current position is just after entry so subtract one.
    tipfileIndex[i] = root.readPosition()/32 - 1;   
  }
  
  //PgmPrintln("Done");
}
/*
 * Play file by index and print latency in ms
 */
void playByIndex(uint16_t index) {
    // open by index
    if (!file.open(root, index)) {
      error("open by index");
    }
    
    // create and play Wave
    if (!wave.create(file)) error("wave.create");
    wave.play();
    while(wave.isplaying){ }
    
    // check for play errors
    sdErrorCheck();
    //PgmPrintln("Done");
}
