#include <Wire.h>
#include "MAX30105.h"

MAX30105 particleSensor;

void setup()
{
  Serial.begin(115200);
  Serial.println("MAX30102 Test");

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD))
  {
    Serial.println("MAX30102 not found. Check wiring!");
    while (1);
  }

  // Setup sensor
  particleSensor.setup(); 
  particleSensor.setPulseAmplitudeRed(0x1F); // Red LED
  particleSensor.setPulseAmplitudeIR(0x1F);  // IR LED
}

void loop()
{
  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  Serial.print("IR: ");
  Serial.print(irValue);
  Serial.print("  RED: ");
  Serial.println(redValue);

  delay(200);
}
