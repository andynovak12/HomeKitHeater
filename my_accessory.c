#include <homekit/homekit.h>
#include <homekit/characteristics.h>

// Called to identify this accessory. See HAP section 6.7.6 Identify Routine
// Generally this is called when paired successfully or click the "Identify Accessory" button in Home APP.
void my_accessory_identify(homekit_value_t _value) {
	printf("accessory identify\n");
}


homekit_characteristic_t cha_current_heating_cooling_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATING_COOLING_STATE, 0);
homekit_characteristic_t cha_target_heating_cooling_state = HOMEKIT_CHARACTERISTIC_(TARGET_HEATING_COOLING_STATE, 0);
homekit_characteristic_t cha_current_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 13);
homekit_characteristic_t cha_target_temperature = HOMEKIT_CHARACTERISTIC_(TARGET_TEMPERATURE, 21);
homekit_characteristic_t cha_temperature_display_units = HOMEKIT_CHARACTERISTIC_(TEMPERATURE_DISPLAY_UNITS, 1);

homekit_accessory_t *accessories[] = {
  HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Thermostat Relay 1"),
      HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Andy Novak"),
      HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123599"),
      HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266"),
      HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.1"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(THERMOSTAT, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
      &cha_current_heating_cooling_state,
      &cha_target_heating_cooling_state,
      &cha_current_temperature,
      &cha_target_temperature,
      &cha_temperature_display_units,
      NULL
    }),
    NULL
  }),
  NULL
};

homekit_server_config_t config = {
		.accessories = accessories,
		.password = "111-11-111" // When pairing to HomeKit, this is the password to input in the Home app.
};
