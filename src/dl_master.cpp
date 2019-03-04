#include <Arduino.h>
#include <TimeLib.h>

#include "dl_common.h"
#include "dl_master.h"
#include "breakout.h"
#include "radio.h"
#include "storage.h"

#define MAX_TESTDEFS (100)

bool dl_master_setup(void) {
  Serial.printf("Running %s setup...\n", MASTER_BOARD_STR_ID);
  // Prepare the SD card for master logging, failure isn't critical
  storage_master_defaults();
  Serial.printf("Finished %s setup!\n", MASTER_BOARD_STR_ID);
  return true;                       
}

bool dl_master_loop(void) {
  switch (breakout_get_switch_state()) {
    case sw_state_top:
      dl_master_run_testdefs();
      break;
    case sw_state_mid:
      breakout_set_led(BO_LED_1, false);
      breakout_set_led(BO_LED_2, false);
      delay(100); // do nothing
      break;
    case sw_state_bot:
      dl_master_send_heartbeats();
      break;
    default:
      delay(100); // do nothing
      break;
  }
  return true;
}

void dl_master_run_testdefs(void) {
  // Clear all interrupts
  dl_common_set_interrupts(false);
  
  if (!is_storage_initialised()) {
    Serial.printf("Storage is not initialised, cannot execute testdefs!\n");
    while (!dl_common_check_interrupts()) {
      delay(100);
    }
    return;
  }

  // Create results folder with current datetime
  SD.chdir(true);
  char test_results_path[64];
  sprintf(test_results_path, "%s%04d-%02d-%02d-%02dh%02dm%02ds/", RESULTS_DIR,
          year(), month(), day(), hour(), minute(), second());
  SD.mkdir(test_results_path);

  // Load in all the testdefs 
  // TODO: Don't really want to load all in at once but needed at the moment for ordering
  //       and would rather not load multiple times.
  Serial.printf("Loading all testdefs in testdef folder...\n");
  lora_testdef_t testdefs[MAX_TESTDEFS];
  bool completed[MAX_TESTDEFS] = {false};
  uint8_t testdef_cnt = storage_load_testdefs(testdefs, MAX_TESTDEFS);

  // Enter results directory so logs end up there
  SD.chdir(test_results_path, true);
  File log_file = storage_init_test_log();

  // Start running all testdefs in reverse order of expected range
  // If no packets are received at a certain level do not carry out the further testdefs
  lora_testdef_t *last_testdef = NULL;
  uint16_t packets_at_level = 0;
  Serial.printf("Executing testdefs...\n");
  while (!dl_common_check_interrupts()) {
    bool all_completed = true;
    uint8_t max_exp_range = 0;
    uint8_t selected = 0;
    for (uint8_t i=0; i < testdef_cnt; i++) {
      if (!completed[i]) {
        all_completed = false;
        if (testdefs[i].exp_range > max_exp_range) {
          selected = i;
          max_exp_range = testdefs[i].exp_range;
        }
      }
    }

    if (all_completed) {
      SERIAL_AND_LOG(log_file, "\nAll testdefs excuted!\n")
      break;
    }
    if (last_testdef != NULL && max_exp_range < last_testdef->exp_range) {
      // Exit early as there is no point in carrying on
      if (packets_at_level == 0) {
        SERIAL_AND_LOG(log_file, "\nGiving up, got no packets from testdef with highest expected range!\n")
        break;
      }      
      // At a new level, reset the number of packets found
      packets_at_level = 0;
    }

    // Clear any status LEDs
    breakout_set_led(BO_LED_1, false);
    breakout_set_led(BO_LED_2, false);

    // Run test definition
    lora_testdef_t *testdef = &testdefs[selected];
    SERIAL_AND_LOG(log_file, "\nExecuting testdef: '%s'\n", testdef->id);
    SERIAL_AND_LOG(log_file, "Start Time: " DATETIME_PRINT_FORMAT "\n", DATETIME_PRINT_ARGS);
    log_file.flush();
    g_radio_a->dbg_print_testdef(testdef);
    Serial.printf("\n");
    uint16_t recv_packets = 0;
    bool valid_results = dl_master_run_testdef(testdef, &recv_packets, &log_file);  
    packets_at_level += recv_packets;
    completed[selected] = valid_results;
    last_testdef = testdef;
    SERIAL_AND_LOG(log_file, "Testdef results: %s\n", valid_results ? "Valid" : "Invalid");
    SERIAL_AND_LOG(log_file, "End Time: " DATETIME_PRINT_FORMAT "\n", DATETIME_PRINT_ARGS);
    log_file.flush();
  }

  // All LEDs set to indicate finished
  breakout_set_led(BO_LED_1, true);
  breakout_set_led(BO_LED_2, true);
  log_file.close();

  // Be careful not to just infinitely run tests
  if (breakout_get_switch_state() != sw_state_mid) {
    Serial.printf("\nReturn switch to middle to run tests again...\n");
  }
  while (breakout_get_switch_state() != sw_state_mid) {}
}

bool dl_master_run_testdef(lora_testdef_t *testdef, uint16_t* recv_packets, File* log_file) {
  // Clear all interrupts
  dl_common_set_interrupts(false);
  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();
  // Handshake to pass over test def
  bool delivered_testdef = false;
  while (!delivered_testdef && !dl_common_check_interrupts()) {
    breakout_set_led(BO_LED_1, false);
    breakout_set_led(BO_LED_2, false);
    delivered_testdef = g_radio_a->send_testdef(testdef);
    breakout_set_led(delivered_testdef ? BO_LED_2 : BO_LED_1, true);
    if (!delivered_testdef) {
      delay(500); 
    }
  }
  bool valid_results = false;
  if (delivered_testdef) {
    valid_results = g_radio_a->recv_testdef_packets(testdef, recv_packets, log_file);
    breakout_set_led(BO_LED_1, true);
  }
  return valid_results;
}

void dl_master_send_heartbeats(void) {
  // Clear all interrupts
  dl_common_set_interrupts(false);
  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();
  // Start sending some acknowledged packets indefinitely
  while (!dl_common_check_interrupts()) {
    bool heartbeat_success = g_radio_a->send_heartbeat();
    Serial.printf("Got Heartbeat ACK: %s\n", heartbeat_success ? "True" : "False");
    breakout_set_led(heartbeat_success ? BO_LED_2 : BO_LED_1, true);
    delay(500);
    breakout_set_led(heartbeat_success ? BO_LED_2 : BO_LED_1, false);
  }
  // Just a safety check to ensure switch doesn't skip mid
  while (breakout_get_switch_state() != sw_state_mid) {}
}