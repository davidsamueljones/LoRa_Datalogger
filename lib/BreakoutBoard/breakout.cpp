/*
  Source for handling functionality of the datalogger breakout board.
  @author David Jones (dsj1n15)
*/
#include <Arduino.h>
#include <TimeLib.h>

#include "breakout.h"

static time_t getTeensy3Time();

void breakout_init(void) {
  // Initialise LEDs
  pinMode(BO_LED_1, OUTPUT);
  breakout_set_led(BO_LED_1, false);
  pinMode(BO_LED_2, OUTPUT);
  breakout_set_led(BO_LED_2, false);
  pinMode(BO_LED_3, OUTPUT);
  breakout_set_led(BO_LED_3, false);
  // Initialise software switch
  pinMode(BO_SWITCH_PIN1, INPUT_PULLUP); 
  pinMode(BO_SWITCH_PIN2, INPUT_PULLUP);
  // Set the Time library to use Teensy RTC
  setSyncProvider(getTeensy3Time);
}

void breakout_set_led(uint8_t led, bool enable) {
  digitalWrite(led, enable ? LOW : HIGH);
}

sw_state_t breakout_get_switch_state(void) {
  uint8_t pin_1 = !digitalRead(BO_SWITCH_PIN1);
  uint8_t pin_2 = !digitalRead(BO_SWITCH_PIN2);
  if (!pin_1 && !pin_2) {
    return sw_state_mid;
  } else if (pin_1) {
    return sw_state_top;
  } else if (pin_2) {
    return sw_state_bot;
  }
  // This should never be called
  return sw_state_unknown;
}

static time_t getTeensy3Time() {
  return Teensy3Clock.get();
}
