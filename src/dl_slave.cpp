#ifdef DL_SLAVE

#include <Arduino.h>

#include "dl_common.h"
#include "breakout.h"
#include "radio.h"
#include "storage.h"

static void ISR_switch_state_change(void);
static void recv_and_execute_cmd(void);

void setup() {
  dl_common_boot(ISR_switch_state_change);
  
  // Prepare the SD card for master logging
  storage_slave_defaults();

  Serial.printf("Booted as %s!\n", BOARD_TYPE);
  breakout_set_led(BO_LED_1, true);
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
  delay(100);
}

/**
 * Set any relevent flags to indicate that the current functions should be
 * exited to allow for a change in device behaviour. 
 */
static void ISR_switch_state_change(void) {
  g_radio_a->set_interrupt(true);
}

static void recv_and_execute_cmd(void) {
  // Clear any set interrupt
  g_radio_a->set_interrupt(false);
  // Reset to agreed base 
  g_radio_a->reset_to_base_cfg();

  // Wait till a command is received
  uint8_t master_id = 0;
  radio_cmd_t recv_cmd = g_radio_a->recv_command(&master_id);
  
  switch (recv_cmd) {
    case cmd_testdef: 
    {
      // Handshake to pass over testdef
      lora_testdef_t testdef;
      testdef.master_id = master_id;
      bool recv_testdef = g_radio_a->recv_testdef(&testdef);
      if (!recv_testdef || g_radio_a->check_interrupt()) {
        return;
      }
      // Send packets based on testdef
      g_radio_a->send_testdef_packets(&testdef);
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

#endif