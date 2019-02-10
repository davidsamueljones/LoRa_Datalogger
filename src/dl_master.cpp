#ifdef DL_MASTER

#include <Arduino.h>
#include <TimeLib.h>

#include "dl_master.h"
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
  .crc = true,
};

lora_testdef_t g_testdef_a;

void print_date_time() {
  Serial.printf("[%04d-%02d-%02d] %02d:%02d:%02d\n", 
      year(), month(), day(), hour(), minute(), second());
}

void setup() {
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

  Serial.printf("Booting as Master...\n");
  
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
    return;
  }
  breakout_set_led(BO_LED_2, false);

  // init_sd_card();
  breakout_set_led(BO_LED_3, false);

  Serial.printf("Configuring switch, ensure it is in its middle position...\n");
  // Wait for switch to return to middle position
  while (breakout_get_switch_state() != sw_state_mid) {}
  // Attach switch interrupts 
  attachInterrupt(digitalPinToInterrupt(BO_SWITCH_PIN1), ISR_switch_state_change, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BO_SWITCH_PIN2), ISR_switch_state_change, CHANGE);
  Serial.printf("Switch configured!\n");


  // Define an inbuilt test definition
  strcpy(g_testdef_a.id, "Test_A");
  g_testdef_a.packet_cnt = 10;
  g_testdef_a.packet_len = 60;
  g_testdef_a.cfg = g_testdef_cfg_a;

  Serial.printf("Booted as Master!\n");

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
  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();

  // Handshake to pass over test def
  g_radio_a->send_testdef(&g_testdef_a);
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