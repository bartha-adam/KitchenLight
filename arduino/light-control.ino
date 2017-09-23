#include <Wire.h>
#include <BH1750.h>

// Types:
enum LightControlState
{
  LIGHT_OFF = 0,
  LIGHT_ON = 1,
  LIGHT_TURNING_ON = 2,
  LIGHT_TURNING_OFF = 3
};

// Global constants:
const unsigned int LIGHT_FADE_TIME_MS = 1000;                 // How fast the light is turning on or off
const unsigned long LIGHT_TURN_OFF_DELAY_MS = 60000;          // Light will turn of after <val> ms in case no movement was detected
const unsigned long PIR_SENSOR_STABILIZE_DURATION_MS = 60000; // PIR sensor needs around 1 minute to calibrate. https://learn.adafruit.com/pir-passive-infrared-proximity-motion-sensor/testing-a-pir
const int LIGHT_THRESHOLD_POT_PIN = A1;                       // Input for potentiometer - light turn on threshold in lx. If measured light is lower then this threshold and movement detected, will turn on the light
const int TEST_LED_PIN = LED_BUILTIN;                         // Output for the built in led
const int PIR_SENSOR_PIN = 2;                                 // D2 - Input from the PIR sensor
const int LIGHTING_CONTROL_PWM_PIN = 9;                       // D9 - Light PWM output pin

// Globals:
int lightThreshold = -1;  // light threshold limit in lx
BH1750 lightMeter;
LightControlState lightState = LIGHT_OFF;
unsigned long lastStateChange_ms = 0;
unsigned long lastMovement_ms = 0;
int movement;
uint16_t lightLevelReading;


// The setup function runs once when you press reset or power the board
void setup()
{
  pinMode(TEST_LED_PIN, OUTPUT);
  pinMode(LIGHTING_CONTROL_PWM_PIN, OUTPUT);
  pinMode(LIGHT_THRESHOLD_POT_PIN, INPUT);
  pinMode(PIR_SENSOR_PIN, INPUT);
  lightMeter.begin();
  Serial.begin(115200);
}

int updateMovingAverage(int& movingAverage, int newValue, int window = 5)
{
  if(movingAverage < 0) // First value
  {
    movingAverage = newValue;
  }
  else
  {
    movingAverage = (movingAverage * (window-1) + newValue) / window;
  }
  return movingAverage;
}

int normalizeADCValue(int rawValue)
{
  // ADC on 10 bits => range from 0 to 1023
  // Returns value between 0 and 100
  return (rawValue * 10) / 102;
}

void doLightControl(unsigned long now_ms)
{
  Serial.print ("| DoLightControl ");
  if(movement == HIGH)
  {
    lastMovement_ms = now_ms;
    if(lightThreshold > lightLevelReading)
    {
      if(lightState == LIGHT_OFF)
      {
        Serial.print("| Turning lights ON");
        lightState = LIGHT_TURNING_ON;
        lastStateChange_ms = now_ms;
      }
      else if(lightState == LIGHT_TURNING_OFF)
      {
        Serial.print("| Turning lights back on");
        lightState = LIGHT_TURNING_ON;
      }
    }
  }
  
  if(lightState == LIGHT_ON &&
     now_ms > LIGHT_TURN_OFF_DELAY_MS &&
     now_ms - lastMovement_ms > LIGHT_TURN_OFF_DELAY_MS)
  {
    Serial.print("| Turning lights off");
    lightState = LIGHT_TURNING_OFF;
    lastStateChange_ms = now_ms;
  }

  
  if (lightState == LIGHT_TURNING_ON)
  {
    if(now_ms - lastStateChange_ms >= LIGHT_FADE_TIME_MS)
    {
      lightState = LIGHT_ON;
      lastStateChange_ms = now_ms;
    }
    else
    {
      int lightIntensity = (255 * (now_ms - lastStateChange_ms)) / LIGHT_FADE_TIME_MS; // 255 = ON, 0 = OFF
      analogWrite(LIGHTING_CONTROL_PWM_PIN, lightIntensity);
    }
  }
  else if (lightState == LIGHT_TURNING_OFF)
  {
    if(now_ms - lastStateChange_ms >= LIGHT_FADE_TIME_MS)
    {
      lightState = LIGHT_OFF;
      lastStateChange_ms = now_ms;
    }
    else
    {
      int lightIntensity = 255 - (255 * (now_ms - lastStateChange_ms)) / LIGHT_FADE_TIME_MS; // 255 = ON, 0 = OFF
      analogWrite(LIGHTING_CONTROL_PWM_PIN, lightIntensity);
    }
  }
  
  if (lightState == LIGHT_OFF)
  {
    analogWrite(LIGHTING_CONTROL_PWM_PIN, 0);
  }
  else if (lightState == LIGHT_ON)
  {
    analogWrite(LIGHTING_CONTROL_PWM_PIN, 255);
  }
}

// the loop function runs over and over again forever
void loop() {
  int lightThresholdReading = normalizeADCValue(analogRead(LIGHT_THRESHOLD_POT_PIN));
  updateMovingAverage(lightThreshold, lightThresholdReading);
  movement = digitalRead(PIR_SENSOR_PIN);
  lightLevelReading = lightMeter.readLightLevel();

  Serial.print(" LghtT=");
  Serial.print(lightThreshold);
  Serial.print(" lx | RawLghtT=");
  Serial.print(lightThresholdReading);
  Serial.print(" | LghtR=");
  Serial.print(lightLevelReading);
  Serial.print(" lx");
  
  if(movement == HIGH)
  {
    Serial.print(" | MV");
    digitalWrite(TEST_LED_PIN, HIGH);
  }
  else
  {
    digitalWrite(TEST_LED_PIN, LOW);
  }
  
  unsigned long now_ms = millis();
  
  Serial.print(" | TimeFromLastChange=");
  Serial.print(now_ms - lastStateChange_ms);
  Serial.print(" | LightState=");
  Serial.print(lightState);
  // PIR sensor might report false positives while stabilizing => do control lights using PIR only after warmup
  if(now_ms < PIR_SENSOR_STABILIZE_DURATION_MS) 
  {
    // During PIR warmup, blink the lights at 1Hz
    if(now_ms / 1000 % 2 == 0)
    {
      analogWrite(LIGHTING_CONTROL_PWM_PIN, 0);
      digitalWrite(TEST_LED_PIN, 0);
      
    }
    else
    {
       analogWrite(LIGHTING_CONTROL_PWM_PIN, 255);   
       digitalWrite(TEST_LED_PIN, 1);
    }
  }
  else
  {
    doLightControl(now_ms);
  } 
  
  Serial.println("");
  delay(10);
}
