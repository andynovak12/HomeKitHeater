# HomeKitHeater

This repo contains the code for putting my heater on HomeKit as a thermostat, by using an ESP8266 microcontroller and a relay. This blog post describes the full setup. - ADD LINK

- Add the three code files in this repo to the Arduino IDE. 
- Update the code with your wifi credentials: https://github.com/andynovak12/HomeKitHeater/blob/683839d5070ac9f2788fc4f784554559df702ced/wifi_info.h#L10-L11
- Upload to the ESP8266 using [these settings](https://github.com/Mixiaoxiao/Arduino-HomeKit-ESP8266#recommended-settings-in-ide).

I heavily relied on this library and its examples: https://github.com/Mixiaoxiao/Arduino-HomeKit-ESP8266. Many thanks Mixiaoxiao for all the work put into it.

This is a mapping of the HomeKit state to the unit functions:

<img width="467" alt="Screen Shot 2023-02-05 at 3 52 44 PM" src="https://user-images.githubusercontent.com/15303865/216845398-731bb007-b312-4e4c-b21c-0dc4be70c3ab.png">
