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
  dl_common_boot(ISR_switch_state_change);
  Serial.printf("Booted as %s!\n", BOARD_TYPE);
}

void loop() {
  switch (breakout_get_switch_state()) {
    case sw_state_top:
      run_test_defs();
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

/**
 * Set any relevent flags to indicate that the current functions should be
 * exited to allow for a change in device behaviour. 
 */
static void ISR_switch_state_change(void) {
  g_radio_a->set_interrupt(true);
}

static void run_test_defs(void) {
  // Run Test Definitions
  breakout_set_led(BO_LED_1, false);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, true);

  // Clear any set interrupt
  g_radio_a->set_interrupt(false);

  // Handshake to pass over testdef
  lora_testdef_t testdef;
  bool recv_testdef = g_radio_a->recv_testdef(&testdef);
  if (!recv_testdef || g_radio_a->check_interrupt())
    return;
  // Send packets based on testdef
  g_radio_a->send_testdef_packets(&testdef);
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