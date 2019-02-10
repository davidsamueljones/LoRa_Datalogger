#include <Arduino.h>
#include <TimeLib.h>

#include "dl_common.h"
#include "breakout.h"
#include "radio.h"

static void print_date_time(void);

bool dl_common_boot(void switch_isr(void)) {
  // Initialise breakout board
  breakout_init();
  breakout_set_led(BO_LED_1, true); // Waiting for serial
  breakout_set_led(BO_LED_2, true); // Waiting for radio
  breakout_set_led(BO_LED_3, true); // Waiting for rest of boot

  // Initialise serial connectivity
  Serial.begin(115200);
  delay(100);
  // Warning: Not valid for all possible microcontrollers, ideal behaviour will
  // mean bootup will be delayed until serial monitor open
  while (breakout_get_switch_state() == sw_state_bot && !Serial) {}
  breakout_set_led(BO_LED_1, false);

  Serial.printf("Starting common boot phase...\n");
  
  // RTC configuration occurs during breakout board initialisation
  Serial.printf("RTC sync %s!\n", timeStatus() == timeSet ? "successful" : "failed");
  Serial.printf("Current [Date] Time: ");
  print_date_time();

  // Initialise radio, keep track using global instance
  lora_module_t lora_module = {BOARD_ID, RFM95_CS, RFM95_RST, RFM95_INT};
  g_radio_a = new LoRaModule(&lora_module, &hc_base_cfg);
  bool radio_init_sucess = g_radio_a->radio_init();
  Serial.printf("Radio initialisation %s!\n", radio_init_sucess ? "successful" : "failed");
  Serial.printf("* Radio ID: 0x%02X\n", lora_module.radio_id);
  if (!radio_init_sucess) {
    return false;
  }
  breakout_set_led(BO_LED_2, false);

  Serial.printf("Configuring switch, ensure it is in its middle position...\n");
  // Wait for switch to return to middle position
  while (breakout_get_switch_state() != sw_state_mid) {}
  // Attach switch interrupts 
  attachInterrupt(digitalPinToInterrupt(BO_SWITCH_PIN1), switch_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BO_SWITCH_PIN2), switch_isr, CHANGE);
  Serial.printf("Switch configured!\n");
  
  breakout_set_led(BO_LED_3, false);
  Serial.printf("Finished common boot phase!\n");
  return true;
}

static void print_date_time(void) {
  Serial.printf("[%04d-%02d-%02d] %02d:%02d:%02d\n", 
      year(), month(), day(), hour(), minute(), second());
}