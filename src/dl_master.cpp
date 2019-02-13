#ifdef DL_MASTER

#include <Arduino.h>
#include <TimeLib.h>

#include "dl_common.h"
#include "breakout.h"
#include "radio.h"
#include "storage.h"

static void ISR_switch_state_change(void);
static void run_testdefs(void);
static uint16_t run_testdef(lora_testdef_t *testdef);
static void send_heartbeats(void);

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

  // Prepare the SD card for master logging
  storage_master_defaults();

  // Define an inbuilt test definition
  strcpy(g_testdef_a.id, "Test_A");
  g_testdef_a.exp_range = 255;
  g_testdef_a.packet_cnt = 10;
  g_testdef_a.packet_len = 255;
  g_testdef_a.cfg = g_testdef_cfg_a;

  Serial.printf("Booted as %s!\n", BOARD_TYPE);
  breakout_set_led(BO_LED_3, true);
}

void loop() {
  switch (breakout_get_switch_state()) {
    case sw_state_top:
      run_testdefs();
      break;
    case sw_state_bot:
      send_heartbeats();
      break;
    default:
      delay(100); // do nothing
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

static void run_testdefs(void) {
  // Clear any set interrupt
  g_radio_a->set_interrupt(false);

  // Go to the root directory
  SD.chdir(true);
  
    // Create results folder with current datetime
  char test_results_path[30];
  sprintf(test_results_path, "%s%04d-%02d-%02d-%02dh%02dm%02ds/", RESULTS_DIR,
          year(), month(), day(), hour(), minute(), second());
  SD.mkdir(test_results_path);

  // Load in all the testdefs (TODO: Don't really want to load all in at once)
  Serial.printf("Loading all testdefs in testdef folder...\n");
  lora_testdef_t testdefs[20];
  bool completed[20] = {false};
  File dir = SD.open(TESTDEF_DIR, O_RDONLY);
  File dirFile;
  uint8_t n = 0;
  uint8_t nMax = 20;
  while (n < nMax && dirFile.openNext(&dir, O_RDONLY)) {
    char buf[30];
    dirFile.getName(buf, 30);
    Serial.printf("* Found: %s", buf);
    if (dirFile.isSubDir() || dirFile.isHidden() || buf[0] == '_') {
      Serial.printf(" [Ignored]\n");
    } else {
      Serial.printf("\n");
      storage_load_testdef(&dirFile, &testdefs[n]);
      n++;
    }
    dirFile.close();
  }
  Serial.printf("%d testdefs loaded!\n", n);

  // Enter results directory so logs end up there
  SD.chdir(test_results_path, true);
  
  // Start running all testdefs in reverse order of expected range
  // If no packets are received at a certain level do not carry out the further testdefs
  uint8_t last_exp_range = 0;
  uint16_t last_recv_packets = UINT16_MAX;
  Serial.printf("Executing testdefs...\n");
  while (!g_radio_a->check_interrupt(false)) {
    bool all_completed = true;
    uint8_t max_exp_range = 0;
    uint8_t selected = 0;
    for (uint8_t i=0; i < n; i++) {
      if (!completed[i]) {
        all_completed = false;
        if (testdefs[i].exp_range > max_exp_range) {
          selected = i;
          max_exp_range = testdefs[i].exp_range;
        }
      }
    }
    if (last_recv_packets == 0 && max_exp_range < last_exp_range) {
      Serial.printf("Giving up, got no packets from testdef with highest expected range!\n");
      break;
    }
    if (all_completed) {
      Serial.printf("All testdefs executed!\n");
      break;
    }
    // Run test definition
    lora_testdef_t *testdef = &testdefs[selected];
    g_radio_a->dbg_print_testdef(testdef);
    last_recv_packets = run_testdef(testdef);  
    completed[selected] = true;
    last_exp_range = testdef->exp_range;
  }

  // Be careful not to just infinitely run tests
  if (breakout_get_switch_state() != sw_state_mid) {
  Serial.printf("Return switch to middle to run tests again...\n");
  }
  while (breakout_get_switch_state() != sw_state_mid) {}
}

static uint16_t run_testdef(lora_testdef_t *testdef) {
  uint16_t recv_packets = 0;

  // Clear any set interrupt
  g_radio_a->set_interrupt(false);
  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();
  // Handshake to pass over test def
  bool delivered_testdef = false;
  while (!delivered_testdef && !g_radio_a->check_interrupt(false)) {
    delivered_testdef = g_radio_a->send_testdef(testdef);
  }
  if (delivered_testdef) {
    g_radio_a->recv_testdef_packets(testdef, &recv_packets);
  }
  return recv_packets;
}

static void send_heartbeats(void) {
  // Clear any set interrupt
  g_radio_a->set_interrupt(false);
  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();
  // Start sending some acknowledged packets indefinitely
  while (!g_radio_a->check_interrupt(false)) {
    bool heartbeat_success = g_radio_a->send_heartbeat();
    Serial.printf("Got Heartbeat ACK: %d\n", heartbeat_success);
    breakout_set_led(heartbeat_success ? BO_LED_2 : BO_LED_1, true);
    delay(500);
    breakout_set_led(heartbeat_success ? BO_LED_2 : BO_LED_1, false);
  }

  while (breakout_get_switch_state() != sw_state_mid) {}
}

#endif