/*
 * Note from library author Mixiaoxiao (Wang Bin):
 *
 * You are recommended to read the Apple's HAP doc before using this library.
 * https://developer.apple.com/support/homekit-accessory-protocol/
 *
 * This HomeKit library is mostly written in C,
 * you can define your accessory/service/characteristic in a .c file,
 * since the library provides convenient Macro (C only, CPP can not compile) to do this.
 * But it is possible to do this in .cpp or .ino (just not so conveniently), do it yourself if you like.
 * Check out homekit/characteristics.h and use the Macro provided to define your accessory.
 *
 * Generally, the Arduino libraries (e.g. sensors, ws2812) are written in cpp,
 * you can include and use them in a .ino or a .cpp file (but can NOT in .c).
 * A .ino is a .cpp indeed.
 *
 * You can define some variables in a .c file, e.g. int my_value = 1;,
 * and you can access this variable in a .ino or a .cpp by writing extern "C" int my_value;.
 *
 * So, if you want use this HomeKit library and other Arduino Libraries together,
 * 1. define your HomeKit accessory/service/characteristic in a .c file
 * 2. in your .ino, include some Arduino Libraries and you can use them normally
 *                  write extern "C" homekit_characteristic_t xxxx; to access the characteristic defined in your .c file
 *                  write your logic code (eg. read sensors) and
 *                  report your data by writing your_characteristic.value.xxxx_value = some_data; homekit_characteristic_notify(..., ...)
 * done.
 */

#include <Arduino.h>
#include <arduino_homekit_server.h>
#include "wifi_info.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// GPIO where the DS18B20 temperature sensor is connected to
const int temperatureSensorGPIOPin = 4; // D2 pin
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(temperatureSensorGPIOPin);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

void setup() {
  Serial.begin(115200);
  wifi_connect(); // in wifi_info.h
  sensors.begin(); // start the temperature sensor
  setupServo();
  my_homekit_setup();
}

void loop() {
  my_homekit_loop();
  delay(10);
}

//==============================
// Homekit setup and loop
//==============================

// access your homekit characteristics defined in my_accessory.c
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t cha_current_temperature;
extern "C" homekit_characteristic_t cha_target_temperature;
extern "C" homekit_characteristic_t cha_target_heating_cooling_state;
extern "C" homekit_characteristic_t cha_current_heating_cooling_state;

// Values that Apple defines
const int heating_cooling_state_off_value = 0;
const int heating_cooling_state_heating_value = 1;
const int heating_cooling_state_cooling_value = 2;
const int heating_cooling_state_auto_value = 3;

static uint32_t next_heap_millis = 0;
static uint32_t next_temperature_reading_millis = 0;
static uint32_t next_temperature_diff_check_millis = 0;
static uint8_t current_heater_cooler_state = heating_cooling_state_off_value;

const int temperature_reading_interval_in_seconds = 60;
const int temperature_maintaining_interval_in_seconds = 120;


// Runs when the user makes changes to the heating/cooling state in the Apple Home iOS App
void cha_target_heating_cooling_state_setter(const homekit_value_t value) {
  current_heater_cooler_state = value.int_value;
  cha_current_heating_cooling_state.value.int_value = current_heater_cooler_state; // sync value with homekit (1 of 2 required lines)

  if(current_heater_cooler_state == heating_cooling_state_off_value){
    // Turn the unit OFF
    Serial.println("Target heating/cooling state changed to: OFF - Turning unit OFF regardless of temperature");
    turnOffUnitIfNeeded();
  } else if(current_heater_cooler_state == heating_cooling_state_heating_value) {
    // Maintain heater within target temperature range
    Serial.println("Target heating/cooling state changed to: HEATING");
    next_temperature_diff_check_millis = 0;
  } else if(current_heater_cooler_state == heating_cooling_state_cooling_value) {
    // Maintain AC within target temperature range
    Serial.println("Target heating/cooling state changed to: COOLING");
    next_temperature_diff_check_millis = 0;
  } else if(current_heater_cooler_state == heating_cooling_state_auto_value) {
    // Turn the unit ON
    Serial.println("Target heating/cooling state changed to: AUTO - Turning unit ON regardless of temperature");
    turnOnUnitIfNeeded();
  }
  
  homekit_characteristic_notify(&cha_current_heating_cooling_state, cha_current_heating_cooling_state.value);  // sync value with homekit (2 of 2 required lines)
}

void my_homekit_setup() {
  cha_target_heating_cooling_state.setter = cha_target_heating_cooling_state_setter;
  arduino_homekit_setup(&config);
}

void my_homekit_loop() {
  arduino_homekit_loop();
  const uint32_t t = millis();
  if (t > next_temperature_reading_millis) {
    // report current temperature every X seconds, where X is defined in temperature_reading_interval_in_seconds
    next_temperature_reading_millis = t + temperature_reading_interval_in_seconds * 1000;
    updateCurrentTemperature();
  }
  if (t > next_temperature_diff_check_millis 
     && (current_heater_cooler_state == heating_cooling_state_heating_value 
          || current_heater_cooler_state == heating_cooling_state_cooling_value
        )
     ) {
        // check to maintain temperature every X seconds, where X is defined in temperature_maintaining_interval_in_seconds
        next_temperature_diff_check_millis = t + temperature_maintaining_interval_in_seconds * 1000;
    
        Serial.println("Checking temperature difference");
        if (current_heater_cooler_state == heating_cooling_state_heating_value) {
          maintain_heater_state();
        }
        
        if (current_heater_cooler_state == heating_cooling_state_cooling_value) {
          maintain_cooler_state();
        }
  }
  if (t > next_heap_millis) {
    // show heap info every 5 seconds
    next_heap_millis = t + 5 * 1000;
    LOG_D("Free heap: %d, HomeKit clients: %d",
        ESP.getFreeHeap(), arduino_homekit_connected_clients_count()
    );
  }
}


//==============================
// Temperature section
//==============================

const float target_temperature_range = 0.35;

void maintain_heater_state() {
    Serial.println("Maintaining heater state");
    if (cha_current_temperature.value.float_value < cha_target_temperature.value.float_value - target_temperature_range) {
      LOG_D("Turning on heat because current temp is below target temp minus one. Current temp is: %.1f, Target temp is: %.1f",
        fahrenheitFromCelsius(cha_current_temperature.value.float_value), 
        fahrenheitFromCelsius(cha_target_temperature.value.float_value)
      );
      turnOnUnitIfNeeded();
    }
    else if (cha_current_temperature.value.float_value > cha_target_temperature.value.float_value + target_temperature_range) {
      LOG_D("Turning off heat because current temp is above target temp plus one. Current temp is: %.1f, Target temp is: %.1f",
        fahrenheitFromCelsius(cha_current_temperature.value.float_value), 
        fahrenheitFromCelsius(cha_target_temperature.value.float_value)
      );
      turnOffUnitIfNeeded();
    } else {
      LOG_D("Not changing heater unit because current temp is within range of target temp. Current temp is: %.1f, Target temp is: %.1f",
        fahrenheitFromCelsius(cha_current_temperature.value.float_value), 
        fahrenheitFromCelsius(cha_target_temperature.value.float_value)
      );
    }
}

void maintain_cooler_state() {
    Serial.println("Maintaining cooler state");
    if (cha_current_temperature.value.float_value > cha_target_temperature.value.float_value + target_temperature_range) {
      LOG_D("Turning on cooling because current temp is above target temp plus one. Current temp is: %.1f, Target temp is: %.1f",
        fahrenheitFromCelsius(cha_current_temperature.value.float_value), 
        fahrenheitFromCelsius(cha_target_temperature.value.float_value)
      );
      turnOnUnitIfNeeded();
    }
    else if (cha_current_temperature.value.float_value < cha_target_temperature.value.float_value - target_temperature_range) {
      LOG_D("Turning off cooling because current temp is below target temp minus one. Current temp is: %.1f, Target temp is: %.1f",
        fahrenheitFromCelsius(cha_current_temperature.value.float_value), 
        fahrenheitFromCelsius(cha_target_temperature.value.float_value)
      );
      turnOffUnitIfNeeded();
    } else {
      LOG_D("Not changing cooler unit because current temp is within range of target temp. Current temp is: %.1f, Target temp is: %.1f",
        fahrenheitFromCelsius(cha_current_temperature.value.float_value), 
        fahrenheitFromCelsius(cha_target_temperature.value.float_value)
      );
    }
}

void updateCurrentTemperature() {
  float temperature_value = readCurrentTemperature();
  LOG_D("Current temperature: %.1f celsius, %.1f fahrenheit", temperature_value, fahrenheitFromCelsius(temperature_value));

  // Ignore temps outside real range
  if ((temperature_value < 38) && (temperature_value > 10)) {
    cha_current_temperature.value.float_value = temperature_value;
    homekit_characteristic_notify(&cha_current_temperature, cha_current_temperature.value); // sync value with homekit 
  } else {
    LOG_D("Ignoring temperature change because it is outside of 10-38 degree Celsius range. Keeping temp at current value of %.1f celsius", cha_current_temperature.value.float_value);
  };
}

float readCurrentTemperature() {
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  return temperatureC;
}

float fahrenheitFromCelsius(float celsius) {
  return celsius * 9.0 / 5.0 + 32.0;
}


//==============================
// Relay section
// - "Unit" refers to the heater/cooler
//==============================

//On board LED Connected to GPIO2
#define LED 2  
#define CONTROL_PIN 14 // D5 pin on board. Connects to Relay's IN1 pin.
static bool current_unit_is_on = false;

void setupServo() {
  pinMode(LED, OUTPUT);     // Initialize the LED_BUILTIN pin as an output to have visual indicator of intended Relay state
  pinMode(CONTROL_PIN, OUTPUT);
  turnOffUnit();            // Start with the Unit off 
}


// Functions suffixed with "IfNeeded" are used to minimize the amount of writes to the pins.
void turnOnUnitIfNeeded() {
  if (!current_unit_is_on) {
    turnOnUnit();
  } else {
    Serial.println("turnOnUnitIfNeeded() doing nothing because unit is already on");
  }
}

void turnOffUnitIfNeeded() {
  if (current_unit_is_on) {
    turnOffUnit();
  } else {
    Serial.println("turnOffUnitIfNeeded() doing nothing because unit is already off");
  }
}

void turnOnUnit() {
  digitalWrite(LED,LOW); // Turn on LED (LED is connected in reverse)
  digitalWrite(CONTROL_PIN,LOW); // Turn on Relay (Relay board has jumper set to low-level trigger)

  Serial.println("Turn ON - Relay input set to LOW");
  current_unit_is_on = true;
}

void turnOffUnit() {
  digitalWrite(LED,HIGH); // Turn off LED (LED is connected in reverse)
  digitalWrite(CONTROL_PIN,HIGH); // Turn off Relay (Relay board has jumper set to low-level trigger)
  
  Serial.println("Turn OFF - Relay input set to HIGH");
  current_unit_is_on = false;
}
