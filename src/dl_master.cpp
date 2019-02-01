#include <Arduino.h>
#include "breakout.h"

static void handle_loop_mode_top(void);
static void handle_loop_mode_mid(void);
static void handle_loop_mode_bot(void);

#ifdef DL_MASTER
void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.printf("Booted");
  breakout_init();
	// init_radio();
	// init_sd_card();
}

void loop() {
  switch (breakout_get_switch_state()) {
    case sw_state_top:
      handle_loop_mode_top();
      break;
    case sw_state_mid:
      handle_loop_mode_mid();
      break;
    case sw_state_bot:
      handle_loop_mode_bot();
      break;
    default:
      break;
  }
  delay(100);
}
#endif

static void handle_loop_mode_top(void) {
  // Run Test Definitions
  breakout_set_led(BO_LED_1, false);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, true);
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