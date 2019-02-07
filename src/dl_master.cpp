#ifdef DL_MASTER

#include <Arduino.h>
#include "breakout.h"
#include "radio.h"

static void run_test_defs(void);
static void handle_loop_mode_mid(void);
static void handle_loop_mode_bot(void);

lora_cfg_t g_testdef_cfg_a = {
	.freq = 868.0,
	.sf = 7,
	.tx_dbm = 14,
	.bw = 125000,
	.cr4_denom = 5,
	.crc = true,
};

lora_testdef_t g_testdef_a;

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

  Serial.printf("Booting...\n");
  
  // Initialise radio, keep track using global instance
  lora_module_t lora_module = {RFM95_CS, RFM95_RST, RFM95_INT};
  g_radio_a = new LoRaModule(&lora_module, &hc_base_cfg);
  if (!g_radio_a->radio_init()) {
    return;
  }
  breakout_set_led(BO_LED_2, false);

  // Define an inbuilt test definition
  strcpy(g_testdef_a.id, "Test_A");
  g_testdef_a.packet_cnt = 10;
  g_testdef_a.packet_len = 60;
  g_testdef_a.cfg = g_testdef_cfg_a;

	// init_sd_card();
  Serial.printf("Booted\n");
  breakout_set_led(BO_LED_3, false);
  // Wait for switch to return to middle position
  while (breakout_get_switch_state() != sw_state_mid) {}
}

void loop() {
  // Poll switch to choose behaviour, keep internal function behaviour
  // short so it can react to switch changes
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
      break;
    default:
      break;
  }
}


static void run_test_defs(void) {
  Serial.printf("Running test defs...\n");
  // Run Test Definitions
  breakout_set_led(BO_LED_1, false);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, true);

  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();

  // Handshake to pass over test def
  g_radio_a->send_testdef(&g_testdef_a);
  // Wait for packets
  while (true) {

  }
}

static void handle_loop_mode_mid(void) {
  // Off State - Do Nothing
  breakout_set_led(BO_LED_1, false);
  breakout_set_led(BO_LED_2, true);
  breakout_set_led(BO_LED_3, false);
}

/**
 * Listen for test packets on the base configuration and record any received.
 */
static void handle_loop_mode_bot(void) {
  // Simple Listen 
  breakout_set_led(BO_LED_1, true);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, false);
}

#endif