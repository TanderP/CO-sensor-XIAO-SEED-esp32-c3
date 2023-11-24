#include <Arduino.h>
#include <MQUnifiedsensor.h>

#define Board ("ESP-32")
#define Pin (5)

#define Type ("MQ-7")
#define Voltage_Resolution (3.3)
#define ADC_Bit_Resolution (12)
#define RatioMQ2CleanAir (9.83)

MQUnifiedsensor MQ7(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);

#define LED 4
#define LEDGND 3

bool trigger = true;
int timer_set = 180;

// Function to convert PPM to AQI using linear interpolation
int convertPPMtoAQI(float ppm) {
  const int numberOfRanges = 6;
  float ppmValues[numberOfRanges + 1] = {0, 4.4, 9.4, 12.4, 15.4, 30.4, 500};
  int aqiRanges[numberOfRanges] = {0, 50, 100, 150, 200, 300};

  for (int i = 0; i < numberOfRanges; i++) {
    if (ppm <= ppmValues[i + 1]) {
      return aqiRanges[i] + ((aqiRanges[i + 1] - aqiRanges[i]) / (ppmValues[i + 1] - ppmValues[i])) * (ppm - ppmValues[i]);
    }
  }
  return 500;  // Default, if ppm is beyond the defined upper limit
}

// Function to convert PPM to mg/m³
float convertPPMtoMgPerM3(float ppm, float molecularWeight) {
  const float molarVolume = 24.45; // L/mol, constant for ideal gas at STP
  return (ppm * molecularWeight) / molarVolume;
}

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(LEDGND, OUTPUT);
  digitalWrite(LEDGND, LOW);

  Serial.begin(115200);

  for (int i = 0; i < 3; i++) {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(1000);
  }

  Serial.println("ESP BEGIN");
  delay(1000);

  MQ7.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ7.setA(99.042);
  MQ7.setB(-1.518);

  MQ7.init();

  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ7.update();
    calcR0 += MQ7.calibrate(RatioMQ2CleanAir);
    Serial.print(".");
  }
  MQ7.setR0(calcR0 / 10);
  Serial.println("  done!.");

  if (isinf(calcR0)) {
    Serial.println("Warning: Connection issue, R0 is infinite (Open circuit detected) please check your wiring and supply");
    while (1);
  }
  if (calcR0 == 0) {
    Serial.println("Warning: Connection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply");
    while (1);
  }
}

void loop() {
  //heating up before use
  if (trigger == true) {
    for (int i = 0; i < timer_set; i++) {
      if (i == timer_set - 1) {
        analogWrite(LED, 80);
        Serial.print("Ready");
        trigger = false;
      }
      delay(1000);
    }
  }

  MQ7.update();
  float ppmValue = MQ7.readSensor();
  int aqiValue = convertPPMtoAQI(ppmValue);
  float mgPerM3Value = convertPPMtoMgPerM3(ppmValue, 28.01); // Assuming molecular weight of CO is 28.01 g/mol

  Serial.print("PPM Value: ");
  Serial.println(ppmValue);
  Serial.print("AQI Value: ");
  Serial.println(aqiValue);
  Serial.print("CO Concentration (mg/m³): ");
  Serial.println(mgPerM3Value);

  delay(500);
}
