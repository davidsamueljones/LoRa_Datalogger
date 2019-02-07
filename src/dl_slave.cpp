#ifdef DL_SLAVE

#include "breakout.h"
#include "radio.h"

static void run_test_defs(void);
static void handle_loop_mode_mid(void);
static void handle_loop_mode_bot(void);

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.printf("Booting...");
  
  // Initialise breakout board
  breakout_init();
  // Initialise radio, keep track using global instance
  lora_module_t lora_module = {RFM95_CS, RFM95_RST, RFM95_INT};
  g_radio_a = new LoRaModule(&lora_module, &hc_base_cfg);
  g_radio_a->radio_init(); // TODO: Handle failure
  
	// init_sd_card();
  Serial.printf("Booted");
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
  lora_testdef_t testdef;
  // Run Test Definitions
  breakout_set_led(BO_LED_1, false);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, true);

  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();
  // Handshake to pass over test def
  g_radio_a->recv_testdef(&testdef);
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


static void handle_loop_mode_bot(void) {
  // Simple Send 
  breakout_set_led(BO_LED_1, true);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, false);
}

#endif