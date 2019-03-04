#include <Arduino.h>
#include <TimeLib.h>
#include <EEPROM.h>

#include "dl_common.h"
#include "breakout.h"
#include "radio.h"
#include "storage.h"

static volatile bool _interrupted = false;
static uint8_t board_id = INVALID_BOARD_ID;

bool dl_common_boot(void switch_isr(void)) {
  // Initialise breakout board
  breakout_init();
  breakout_set_led(BO_LED_3, true); // Waiting for serial
  breakout_set_led(BO_LED_1, true); // Waiting for radio
  breakout_set_led(BO_LED_2, true); // Waiting for rest of boot

  // Initialise serial connectivity
  Serial.begin(115200);
  delay(100);
  // Warning: Not valid for all possible microcontrollers, ideal behaviour will
  // mean bootup will be delayed until serial monitor open
  while (breakout_get_switch_state() == sw_state_bot && !Serial) {}
  breakout_set_led(BO_LED_3, false);

  Serial.printf("Starting common boot phase...\n");
  
  // RTC configuration occurs during breakout board initialisation
  Serial.printf("RTC sync %s!\n", timeStatus() == timeSet ? "successful" : "failed");
  Serial.printf("Current [Date] Time: ");
  Serial.printf(DATETIME_PRINT_FORMAT "\n", DATETIME_PRINT_ARGS);

  // Get the Board ID from the EEPROM
  Serial.printf("Getting Board ID from EEPROM...\n");
  uint8_t identifier_byte_1 = EEPROM.read(IDX_IDENTIFIER_BYTE_1);
  uint8_t identifier_byte_2 = EEPROM.read(IDX_IDENTIFIER_BYTE_2);
  bool board_id_set = identifier_byte_1 == IDENTIFIER_BYTE_1 &&
                      identifier_byte_2 == IDENTIFIER_BYTE_2;
  if (board_id_set) {
    Serial.printf("Board ID Found!\n");
    uint8_t board_id = EEPROM.read(IDX_BOARD_ID);
    Serial.printf("* ID: 0x%02X\n", board_id);
    Serial.printf("* Valid: %s\n", IS_VALID_BOARD_ID(board_id) ? "True" : "False");
    if (!IS_VALID_BOARD_ID(board_id)) {
      return false;
    }
    Serial.printf("* Type: %s\n", GET_BOARD_STR_ID(board_id));
  } else {
    Serial.printf("Board ID not Found!\n");
    return false;
  }

  // Initialise radio, keep track using global instance
  lora_module_t lora_module = {board_id, RFM95_CS, RFM95_RST, RFM95_INT};
  g_radio_a = new LoRaModule(&lora_module, &hc_base_cfg);
  bool radio_init_sucess = g_radio_a->radio_init();
  Serial.printf("Radio initialisation %s!\n", radio_init_sucess ? "successful" : "failed");
  if (!radio_init_sucess) {
    return false;
  }
  breakout_set_led(BO_LED_1, false);

  // Initialise the SD card, failure is not necessarily a boot failure,
  // let higher levels check for initialisation to determine severity of failure.
  bool storage_init_success = storage_init();
  Serial.printf("SD card initialisation %s!\n", storage_init_success ? "successful" : "failed");

  Serial.printf("Configuring switch, ensure it is in its middle position...\n");
  // Wait for switch to return to middle position
  while (breakout_get_switch_state() != sw_state_mid) {}
  // Attach switch interrupts 
  attachInterrupt(digitalPinToInterrupt(BO_SWITCH_PIN1), switch_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BO_SWITCH_PIN2), switch_isr, CHANGE);
  Serial.printf("Switch configured!\n");
  
  breakout_set_led(BO_LED_2, false);
  Serial.printf("Finished common boot phase!\n");
  return true;
}

void dl_common_finish_boot(bool boot_success) {
  breakout_set_led(GET_BOARD_LED(board_id), true);
  if (!boot_success) {
    Serial.printf("Boot as %s failed!\n", GET_BOARD_STR_ID(board_id)); 
    Serial.printf("Check errors and reset device!\n"); 
    
    bool failure_led_on = false;
    while (true) {
      failure_led_on = !failure_led_on;
      breakout_set_led(BO_LED_2, failure_led_on);
      delay(1000);
    }
  } else {
    Serial.printf("Booted as %s successfully!\n", GET_BOARD_STR_ID(board_id)); 
  }           
}

uint8_t dl_common_get_board_id(void) {
  return board_id;
}

void dl_common_set_interrupts(void) {
  dl_common_set_interrupts(true);
}

void dl_common_set_interrupts(bool value) {
  g_radio_a->set_interrupt(value);
  _interrupted = value;
}

bool dl_common_check_interrupts(bool clear) {
  bool temp = _interrupted; 
  if (clear) {
    dl_common_set_interrupts(false);
  }
  return temp;
}
