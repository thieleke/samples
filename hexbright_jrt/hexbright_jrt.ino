/* 
 
 Factory firmware for HexBright FLEX 
 v2.4  Dec 6, 2012
 Modifications from Jeff Thieleke:
    * Incorportated accelerometer, 10 minute timeout mode, and the 5 second power off shortcut code from https://github.com/bhimoff/HexBrightFLEX
    * Modified the duty cycle on the flashing mode to be more bicycle friendly
    * Removed the low brightness mode
    * Added a last-on memory.
    * Code reformatting and reorganization
 */

#include <math.h>
#include <Wire.h>

// Settings
#define OVERTEMP                340
// Constants
#define ACC_ADDRESS             0x4C
#define ACC_REG_XOUT            0
#define ACC_REG_YOUT            1
#define ACC_REG_ZOUT            2
#define ACC_REG_TILT            3
#define ACC_REG_INTS            6
#define ACC_REG_MODE            7
// Pin assignments
#define DPIN_RLED_SW            2
#define DPIN_GLED               5
#define DPIN_PGOOD              7
#define DPIN_PWR                8
#define DPIN_DRV_MODE           9
#define DPIN_DRV_EN             10
#define DPIN_ACC_INT            3
#define APIN_TEMP               0
#define APIN_CHARGE             3
// Interrupts
#define INT_SW                  0
#define INT_ACC                 1
// Modes
#define MODE_OFF                0
#define MODE_LOW                1
#define MODE_MED                2
#define MODE_HIGH               3
#define MODE_BLINKING           4
#define MODE_BLINKING_PREVIEW   5

// State
byte mode = 0;
byte lastOnMode = MODE_MED;
unsigned long btnTime = 0;
boolean btnDown = false;
int lastChargeState = 0;
unsigned long lastModeTime = 0;
unsigned long oneSecondLoopTime = 0;
unsigned long lastAccTime = 0;

void setup()
{
    // We just powered on!  That means either we got plugged 
    // into USB, or the user is pressing the power button.
    pinMode(DPIN_PWR,      INPUT);
    digitalWrite(DPIN_PWR, LOW);

    // Initialize GPIO
    pinMode(DPIN_RLED_SW,  INPUT);
    pinMode(DPIN_GLED,     OUTPUT);
    pinMode(DPIN_DRV_MODE, OUTPUT);
    pinMode(DPIN_DRV_EN,   OUTPUT);
    pinMode(DPIN_ACC_INT,  INPUT);
    pinMode(DPIN_PGOOD,    INPUT);
    digitalWrite(DPIN_DRV_MODE, LOW);
    digitalWrite(DPIN_DRV_EN,   LOW);
    digitalWrite(DPIN_ACC_INT,  HIGH);

    // Initialize serial busses
    Serial.begin(9600);
    Wire.begin();

    // Configure accelerometer
    byte config[] = {
        ACC_REG_INTS,  // First register (see next line)
        0xE4,  // Interrupts: shakes, taps
        0x00,  // Mode: not enabled yet
        0x00,  // Sample rate: 120 Hz
        0x0F,  // Tap threshold
        0x10   // Tap debounce samples
    };
    Wire.beginTransmission(ACC_ADDRESS);
    Wire.write(config, sizeof(config));
    Wire.endTransmission();

    // Enable accelerometer
    byte enable[] = {ACC_REG_MODE, 0x01};  // Mode: active!
    Wire.beginTransmission(ACC_ADDRESS);
    Wire.write(enable, sizeof(enable));
    Wire.endTransmission();
    
    btnTime = millis();
    btnDown = digitalRead(DPIN_RLED_SW);
    mode = MODE_OFF;

    Serial.println("Powered up!");
}

void loop()
{
    unsigned long time = millis();
    
    chargeState();
    oneSecondLoop();
        
    // Check if the accelerometer wants to interrupt
    checkAccel();

    // Do whatever this mode does
    switch (mode)
    {
      case MODE_BLINKING:
      case MODE_BLINKING_PREVIEW:
          digitalWrite(DPIN_DRV_EN, (time%600)<450);
          break;
    }

    // Check for mode changes
    byte newMode = mode;
    byte newBtnDown = digitalRead(DPIN_RLED_SW);
 
    switch (mode)
    {
      case MODE_OFF:
        if (btnDown && !newBtnDown && (time-btnTime)>20)
        {
            if(time - lastModeTime > 2500)
            {
                Serial.print("lastOnMode = ");
                Serial.println(lastOnMode);
                newMode = lastOnMode;
                lastModeTime = time;
            }
            else
            {
              newMode = MODE_MED;
            }
        }
        if (btnDown && newBtnDown && (time-btnTime)>500)
        {
          newMode = MODE_BLINKING_PREVIEW;
        }
        break;
      case MODE_LOW:
        if (btnDown && !newBtnDown && (time-btnTime)>50)
        {
            if (time - lastModeTime > 5000) 
            {
              newMode = MODE_OFF;
            } 
            else
            {
              newMode = MODE_MED;
            }
        }
        break;
      case MODE_MED:
        if (btnDown && !newBtnDown && (time-btnTime)>50)
        {
            if (time - lastModeTime > 5000)
            {
              newMode = MODE_OFF;
            }
            else
            {
              newMode = MODE_HIGH;
            }
        }
        break;
      case MODE_HIGH:
        if (btnDown && !newBtnDown && (time-btnTime)>50)
          newMode = MODE_OFF;
        break;
      case MODE_BLINKING_PREVIEW:
        // This mode exists just to ignore this button release.
        if (btnDown && !newBtnDown)
          newMode = MODE_BLINKING;
        break;
      case MODE_BLINKING:
        if (btnDown && !newBtnDown && (time-btnTime)>50)
          newMode = MODE_OFF;
        break;
    }

    // Do the mode transitions
    if (newMode != mode)
    {
        lastModeTime = time;
        
        switch (newMode)
        {
          case MODE_OFF:
            Serial.println("Mode = off");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, LOW);
            digitalWrite(DPIN_DRV_MODE, LOW);
            digitalWrite(DPIN_DRV_EN, LOW);
            break;
          case MODE_LOW:
            Serial.println("Mode = low");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, HIGH);
            digitalWrite(DPIN_DRV_MODE, LOW);
            analogWrite(DPIN_DRV_EN, 64);
            lastOnMode = newMode;
            break;
          case MODE_MED:
            Serial.println("Mode = medium");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, HIGH);
            digitalWrite(DPIN_DRV_MODE, LOW);
            analogWrite(DPIN_DRV_EN, 255);
            lastOnMode = newMode;
            break;
          case MODE_HIGH:
            Serial.println("Mode = high");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, HIGH);
            digitalWrite(DPIN_DRV_MODE, HIGH);
            analogWrite(DPIN_DRV_EN, 255);
            lastOnMode = newMode;
            break;
          case MODE_BLINKING:
          case MODE_BLINKING_PREVIEW:
            Serial.println("Mode = blinking");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, HIGH);
            digitalWrite(DPIN_DRV_MODE, HIGH);
            break;
        }

        mode = newMode;
    }

    // Remember button state so we can detect transitions
    if (newBtnDown != btnDown)
    {
        btnTime = time;
        btnDown = newBtnDown;
        delay(50);
    }
}

void readAccel(char *acc)
{
    while (1)
    {
        Wire.beginTransmission(ACC_ADDRESS);
        Wire.write(ACC_REG_XOUT);
        Wire.endTransmission(false);       // End, but do not stop!
        Wire.requestFrom(ACC_ADDRESS, 3);  // This one stops.

        for (int i = 0; i < 3; i++)
        {
            if (!Wire.available())
              continue;
            acc[i] = Wire.read();
            if (acc[i] & 0x40)  // Indicates failed read; redo!
              continue;
            if (acc[i] & 0x20)  // Sign-extend
              acc[i] |= 0xC0;
        }
        break;
    }
}

float readAccelAngleXZ()
{
    char acc[3];
    readAccel(acc);
    return atan2(acc[0], acc[2]);
}

void chargeState()
{
    unsigned long time = millis();
  
    // Check the state of the charge controller
    int chargeState = analogRead(APIN_CHARGE);
    
    if (chargeState < 128)  // Low - charging
    {
        digitalWrite(DPIN_GLED, (time&0x0100)?LOW:HIGH);
    }
    else if (chargeState > 768) // High - charged
    {
        digitalWrite(DPIN_GLED, HIGH);
    }
    else // Hi-Z - shutdown
    {
        digitalWrite(DPIN_GLED, LOW);    
    }
}

void oneSecondLoop()
{
    unsigned long time = millis();
    
    // Check the temperature sensor
    if (time - oneSecondLoopTime > 1000)
    {
        oneSecondLoopTime = time;
        int temperature = analogRead(APIN_TEMP);
        Serial.print("Temp: ");
        Serial.println(temperature);
        if (temperature > OVERTEMP && mode != MODE_OFF)
        {
            Serial.println("Overheating!");

            for (int i = 0; i < 6; i++)
            {
                digitalWrite(DPIN_DRV_MODE, LOW);
                delay(100);
                digitalWrite(DPIN_DRV_MODE, HIGH);
                delay(100);
            }
            digitalWrite(DPIN_DRV_MODE, LOW);

            mode = MODE_LOW;
        }
        
        // Periodically pull down the button's pin, since
        // in certain hardware revisions it can float.
        pinMode(DPIN_RLED_SW, OUTPUT);
        pinMode(DPIN_RLED_SW, INPUT);
    }
}

void checkAccel()
{
    unsigned long time = millis();
    byte tapped = 0, shaked = 0;
    if (!digitalRead(DPIN_ACC_INT))
    {
        Wire.beginTransmission(ACC_ADDRESS);
        Wire.write(ACC_REG_TILT);
        Wire.endTransmission(false);       // End, but do not stop!
        Wire.requestFrom(ACC_ADDRESS, 1);  // This one stops.
        byte tilt = Wire.read();

        if (millis() - lastAccTime > 500)
        {
            lastAccTime = time;

            tapped = !!(tilt & 0x20);
            shaked = !!(tilt & 0x80);

            if (tapped)
            {
              Serial.print("Tap!  ");
              Serial.println(tilt);
            }
        
            if (shaked)
            {
              Serial.print("Shake!  ");
              Serial.println(tilt);
            }
        }
    }
}
