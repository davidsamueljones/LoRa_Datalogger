#ifdef DL_MASTER

#include <Arduino.h>
#include <TimeLib.h>

#include "dl_common.h"
#include "breakout.h"
#include "radio.h"


static void ISR_switch_state_change(void);
static void run_test_defs(void);
static void handle_loop_mode_mid(void);
static void handle_loop_mode_bot(void);

lora_cfg_t g_testdef_cfg_a = {
  .freq = 868.0,
  .sf = 7,
  .tx_dbm = 14,
  .bw = 125000,
  .cr4_denom = 5,
  .preamble_syms = 8,
  .crc = true,
};

lora_testdef_t g_testdef_a;

void setup() {
  dl_common_boot(ISR_switch_state_change);

  // Define an inbuilt test definition
  strcpy(g_testdef_a.id, "Test_A");
  g_testdef_a.packet_cnt = 10;
  g_testdef_a.packet_len = 255;
  g_testdef_a.cfg = g_testdef_cfg_a;

  Serial.printf("Booted as %s!\n", BOARD_TYPE);
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
  Serial.printf("Running test defs...\n");
  // Run Test Definitions
  breakout_set_led(BO_LED_1, false);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, true);

  // Clear any set interrupt
  g_radio_a->set_interrupt(false);

  // Handshake to pass over test def
  bool recv_testdef = g_radio_a->send_testdef(&g_testdef_a);
  if (recv_testdef) {
    g_radio_a->recv_testdef_packets(&g_testdef_a);
  }

  while (breakout_get_switch_state() != sw_state_mid) {}
}

static void handle_loop_mode_mid(void) {
  // Off State - Do Nothing
  breakout_set_led(BO_LED_1, false);
  breakout_set_led(BO_LED_2, true);
  breakout_set_led(BO_LED_3, false);
  delay(100);
}

static void handle_loop_mode_bot(void) {
  breakout_set_led(BO_LED_1, true);
  breakout_set_led(BO_LED_2, false);
  breakout_set_led(BO_LED_3, false);
  delay(100);
}

#endif