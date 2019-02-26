#ifdef DL_SLAVE

#include <Arduino.h>

#include "dl_common.h"
#include "breakout.h"
#include "radio.h"
#include "storage.h"

static void recv_and_execute_cmd(void);
static bool handle_testdef_cmd(uint8_t master_id);

void setup() {
  bool boot_success= dl_common_boot(dl_common_set_interrupts); 
  // Prepare the SD card for slave logging, failure isn't critical
  storage_slave_defaults();
  // Report whether boot has successed, will block if boot_success is false
  dl_common_finish_boot(boot_success);      
}

void loop() {
  switch (breakout_get_switch_state()) {
    case sw_state_top:
      recv_and_execute_cmd();
      break;
    default:
      delay(100); // do nothing
      break;
  }
}


static void recv_and_execute_cmd(void) {
  // Clear all interrupts
  dl_common_set_interrupts(false);
  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();

  // Wait till a command is received
  uint8_t master_id = 0;
  radio_cmd_t recv_cmd = g_radio_a->recv_command(&master_id);
  
  switch (recv_cmd) {
    case cmd_testdef: 
    {
      handle_testdef_cmd(master_id);
      break;
    }
    case cmd_heartbeat:
    {
      g_radio_a->ack_heartbeat(master_id);
      break;
    }
    default:
      break;
  }
}

static bool handle_testdef_cmd(uint8_t master_id) {
    // Handshake to pass over testdef
    lora_testdef_t testdef;
    testdef.master_id = master_id;
    bool recv_testdef = g_radio_a->recv_testdef(&testdef);
    if (!recv_testdef || dl_common_check_interrupts()) {
      breakout_set_led(BO_LED_3, true);
      delay(500);
      breakout_set_led(BO_LED_3, false);
      return false;
    }
    // Send packets based on testdef
    breakout_set_led(BO_LED_2, true);
    g_radio_a->send_testdef_packets(&testdef);
    breakout_set_led(BO_LED_2, false);
    return true;
}

#endif