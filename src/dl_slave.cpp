#ifdef DL_SLAVE

#include <Arduino.h>

#include "dl_common.h"
#include "breakout.h"
#include "radio.h"

static void ISR_switch_state_change(void);
static void run_test_defs(void);
static void handle_loop_mode_mid(void);
static void handle_loop_mode_bot(void);

void setup() {
  // Initialise breakout board
  breakout_init();
  breakout_set_led(BO_LED_1, true); // Waiting for serial
  breakout_set_led(BO_LED_2, true); // Waiting for radio
  breakout_set_led(BO_LED_3, true); // Waiting for rest of boot

  // Initialise serial connectivity
  Serial.begin(9600);
  delay(100);
  // Warning: Not valid for all possible microcontrollers, ideal behaviour will
  // mean bootup will be delayed until serial monitor open
  while (breakout_get_switch_state() == sw_state_bot && !Serial) {}
  breakout_set_led(BO_LED_1, false);

  Serial.printf("Booting as Slave...\n");
  
  // Initialise radio, keep track using global instance
  lora_module_t lora_module = {BOARD_ID, RFM95_CS, RFM95_RST, RFM95_INT};
  g_radio_a = new LoRaModule(&lora_module, &hc_base_cfg);
  bool radio_init_sucess = g_radio_a->radio_init();
  Serial.printf("Radio initialisation %s!\n", radio_init_sucess ? "successful" : "failed");
  Serial.printf("* Radio ID: 0x%02X\n", lora_module.radio_id);
  if (!radio_init_sucess) {
    return;
  }
  breakout_set_led(BO_LED_2, false);

  // init_sd_card();
  Serial.printf("Booted as Slave!\n");
  breakout_set_led(BO_LED_3, false);

  Serial.printf("Configuring switch, ensure it is in its middle position...\n");
  // Wait for switch to return to middle position
  while (breakout_get_switch_state() != sw_state_mid) {}
  // Attach switch interrupts 
  attachInterrupt(digitalPinToInterrupt(BO_SWITCH_PIN1), ISR_switch_state_change, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BO_SWITCH_PIN2), ISR_switch_state_change, CHANGE);
  Serial.printf("Switch configured!\n");
  Serial.printf("Entering control loop...\n");
}

void loop() {
  switch (breakout_get_switch_state()) {
    case sw_state_top:
      run_test_defs();
      break;
    case sw_state_mid:
      handle_loop_mode_mid();
      delay(100);
      break;
    case sw_state_bot:
      handle_loop_mode_bot();
      delay(100);
      break;
    default:
      break;
  }
}

/**
 * Set any relevent flags to indicate that the current functions should be
 * exited to allow for a change in device behaviour. 
 */
static void ISR_switch_state_change(void) {
  g_radio_a->set_interrupt(true);
}

static void run_test_defs(void) {
  lora_testdef_t testdef;
  // Run Test Definitions
  breakout_set_led(BO_LED_1, false);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, true);

  // Clear any set interrupt
  g_radio_a->set_interrupt(false);
  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();
  // Handshake to pass over test def
  g_radio_a->recv_testdef(&testdef);
}

static void handle_loop_mode_mid(void) {
  // Off State - Do Nothing
  breakout_set_led(BO_LED_1, false);
  breakout_set_led(BO_LED_2, true);
  breakout_set_led(BO_LED_3, false);
}


static void handle_loop_mode_bot(void) {
  // Simple Send 
  breakout_set_led(BO_LED_1, true);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, false);
}

#endif