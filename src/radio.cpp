#include <Arduino.h>

#include "radio.h"
#include "breakout.h"

#define SINGLE_RX_CHECK_TIMEOUT (500)

lora_cfg_t hc_base_cfg = {
  .freq = 868.0f,
  .sf = 13,
  .tx_dbm = 14,
  .bw = 125000,
  .cr4_denom = 5,
  .crc = true,
};

LoRaModule* g_radio_a = NULL;
LoRaModule* g_radio_b = NULL;

LoRaModule::LoRaModule(lora_module_t *module_cfg, lora_cfg_t *base_cfg) : 
  _rf95(module_cfg->pin_cs, module_cfg->pin_int), 
  _rf95_dg(_rf95, module_cfg->radio_id),
  _module_cfg(*module_cfg), 
  _base_cfg(*base_cfg),
  _interrupt(false)
  {}


bool LoRaModule::radio_init(void) {
  // Configure reset pin
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  // Manually trigger reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  // Initialise a reliable datagram driver, this will initialise the raw driver
  Serial.printf("Initialising radio driver...\n"); 
  bool success = _rf95_dg.init();
  // We need an unreasonably long timeout to detect ACKs, not sure why (TODO)
  _rf95_dg.setTimeout(1000); 
  // Initialise any buffer values
  _tx_buf.from = _rf95_dg.thisAddress();
  return success;
}

void LoRaModule::reset_to_base_cfg(void) {
  set_cfg(&_base_cfg);
}

void LoRaModule::set_cfg(lora_cfg_t *new_cfg) {
  Serial.printf("Setting new configuration...\n");
  _rf95.setFrequency(new_cfg->freq);
  _rf95.setSpreadingFactor(new_cfg->sf);
  _rf95.setTxPower(new_cfg->tx_dbm);
  _rf95.setSignalBandwidth(new_cfg->bw);
  _rf95.setCodingRate4(new_cfg->cr4_denom);
  _rf95.setPayloadCRC(new_cfg->crc);
  _cur_cfg = *new_cfg;
  Serial.printf("Configuration set!\n");
}

bool LoRaModule::acknowledged_tx(uint8_t attempts) {
  bool sent = false;
  uint8_t attempt = 0;
  Serial.printf("Sending acknowledged %d bytes...\n", _tx_buf.len);
  // Disable retrying so we can escape in case of interrupt
  _rf95_dg.setRetries(1);
  // Handle multiple attempt behaviour, lots of retry behaviour not mirrored 
  // from RadioHead but good enough
  do {
    attempt++;
    Serial.printf("* Attempt: %d\n", attempt);
    if (check_interrupt(false)) {
      Serial.printf("Interrupted waiting for acknowledged TX!\n", _tx_buf.len);
      break;
    }
    sent = _rf95_dg.sendtoWait(_tx_buf.data, _tx_buf.len, _tx_buf.to);
  } while (!sent && (attempts == 0 || attempt < attempts));
  
  Serial.printf("TX %s!\n", sent ? "successful" : "failed");
  return sent;
}

bool LoRaModule::unacknowledged_tx(void) {
  Serial.printf("Sending unacknowledged %d bytes...\n", _tx_buf.len);
  bool queued = _rf95_dg.sendto(_tx_buf.data, _tx_buf.len, _tx_buf.to);
  if (!queued) {
    Serial.printf("TX queued unsucessfully!");
    return false;
  }
  // Wait until it has actually been sent
  bool sent = _rf95_dg.waitPacketSent();
  Serial.printf("TX %s!\n", sent ? "successful" : "failed");
  return sent;
}

bool LoRaModule::acknowledged_rx(uint16_t timeout) {
  uint8_t exp_rx_len = _rx_buf.len;
  bool received = false;
  uint16_t time = 0;
  Serial.printf("Waiting for acknowledged RX...\n");
  while (!received && (timeout == 0 || time < timeout)) {
      _rx_buf.len = exp_rx_len;
      // Copy full message if length not pre-configured correctly
      if (_rx_buf.len == 0 || _rx_buf.len > RH_RF95_MAX_MESSAGE_LEN) {
        _rx_buf.len = RH_RF95_MAX_MESSAGE_LEN;
      }
      // Wait for a message, receive it, and acknowledge it
      received = _rf95_dg.recvfromAckTimeout(_rx_buf.data, &_rx_buf.len, 
                                      SINGLE_RX_CHECK_TIMEOUT, &_rx_buf.from, &_rx_buf.to);
      time += SINGLE_RX_CHECK_TIMEOUT;
      if (check_interrupt(false)) {
        Serial.printf("Interrupted waiting for RX!\n");
        break;
      }
  }
  if (timeout != 0 && time >= timeout) {
    Serial.printf("Timed out waiting for RX!\n");
  }
  Serial.printf("Acknowledged RX %s!\n", received ? "successful" : "failed");
  return received;
}

bool LoRaModule::unacknowledged_rx(uint16_t timeout) {
  uint8_t exp_rx_len = _rx_buf.len;
  bool received = false;
  uint16_t time = 0;
  Serial.printf("Waiting for unacknowledged RX...\n");
  while (!received && (timeout == 0 || time < timeout)) {
    // Wait for a message
    bool available = _rf95.waitAvailableTimeout(SINGLE_RX_CHECK_TIMEOUT);
    time += SINGLE_RX_CHECK_TIMEOUT;
    // Process message if one has arrived
    if (available) {
      // Copy full message if length not pre-configured correctly
      _rx_buf.len = exp_rx_len;
      if (_rx_buf.len == 0 || _rx_buf.len > RH_RF95_MAX_MESSAGE_LEN) {
        _rx_buf.len = RH_RF95_MAX_MESSAGE_LEN;
      }
      received = _rf95_dg.recvfrom(_rx_buf.data, &_rx_buf.len, 
                                      &_rx_buf.from, &_rx_buf.to);
      // Count as failed receive if not meant for us
      received &=  ((_rx_buf.to == RH_BROADCAST_ADDRESS) ||
                    (_rx_buf.to == _rf95_dg.thisAddress()));
    }
    if (check_interrupt(false)) {
      Serial.printf("Interrupted waiting for RX!\n");
      break;
    }
  }
  if (timeout != 0 && time >= timeout) {
    Serial.printf("Timed out waiting for RX!\n");
  }
  Serial.printf("Unacknowledged RX %s!\n", received ? "successful" : "failed");
  return received;
}


void LoRaModule::send_testdef(lora_testdef_t *tx_testdef) {
    // Send QRY? and wait for someone to say RDY!
    bool got_rdy = false;
    while(!got_rdy) {
      Serial.printf("Sending QRY? request to slaves...\n");
      _tx_buf.to = RH_BROADCAST_ADDRESS;
      _tx_buf.len = sizeof(radio_msg_t);
      _tx_buf.p_hdr->type = msg_test_qry;
      bool sent = unacknowledged_tx();
      if (check_interrupt(false))
        return;
      if (!sent) {
        delay(500);
        continue;
      }
      Serial.printf("Waiting for RDY! from a slave...\n");
      _rx_buf.len = LEN_MSG_EMPTY;
      got_rdy = acknowledged_rx(3000);
      if (!got_rdy || check_interrupt(false))
        return;
      // Verify received message is a RDY!
      got_rdy = (_rx_buf.p_hdr->type == msg_test_rdy);
      got_rdy &= _rx_buf.from != RH_BROADCAST_ADDRESS;
      got_rdy &= _rx_buf.to == _rf95_dg.thisAddress();
      Serial.printf("Received message is%s a RDY!\n", got_rdy ? "" : " not");
    }
    // Keep a copy of who we are talking to in case rx buf gets trashed
    uint8_t slave_id = _rx_buf.from;

    // TODO: Sort LEDs to be helpful
    breakout_set_led(BO_LED_1, true);
    breakout_set_led(BO_LED_2, true);
    breakout_set_led(BO_LED_3, true);

    // Send Test Definition
    Serial.printf("Sending test definition to slave...\n");
    _tx_buf.to = slave_id;
    _tx_buf.len = LEN_MSG_TESTDEF;
    _tx_buf.p_hdr->type = msg_test_testdef;
    memcpy(&_tx_buf.data[MSG_PAYLOAD_START], tx_testdef, sizeof(lora_testdef_t));
    dbg_print_testdef((lora_testdef_t*) &_tx_buf.data[MSG_PAYLOAD_START]);
    bool acked_testdef = acknowledged_tx(3);
    if (!acked_testdef || check_interrupt(false))
      return;
    Serial.printf("Got acknowledgment to testdef!\n");
}

void LoRaModule::recv_testdef(lora_testdef_t *rx_testdef) {
  uint8_t master_id = 0x80;

  // Wait for QRY?
  Serial.printf("Waiting for QRY? from a master...\n");
  bool got_qry = false;
  while (!got_qry) {
    _rx_buf.len = LEN_MSG_EMPTY;
    got_qry = unacknowledged_rx();
    if (!got_qry || check_interrupt(false))
      return;

    // Verify received message is a query
    got_qry &= (_rx_buf.p_hdr->type == msg_test_qry);
    got_qry &= _rx_buf.from != RH_BROADCAST_ADDRESS;
    got_qry &= _rx_buf.to == RH_BROADCAST_ADDRESS;
    Serial.printf("Received message is%s a QRY?\n", got_qry ? "" : " not");
  }
  // Keep a copy of who we are talking to in case rx buf gets trashed
  master_id = _rx_buf.from;

  // Respond to queryer with a RDY!
  _tx_buf.to = master_id;
  _tx_buf.len = LEN_MSG_EMPTY;
  _tx_buf.p_hdr->type = msg_test_rdy;
  Serial.printf("Responding with RDY! to master...\n");
  bool acked_rdy = acknowledged_tx(3);
  if (!acked_rdy || check_interrupt(false))
    return;
  Serial.printf("Got acknowledgment to RDY!\n");

  // Receive Test Definition
  bool got_testdef = false;
  while (!got_testdef) {
    // Give up if we haven't received any message every 3000ms
    _rx_buf.len = LEN_MSG_TESTDEF;
    got_testdef = acknowledged_rx(3000);
    if (!got_testdef || check_interrupt(false))
      return;
    // Verify received message is a test definition
    got_testdef &= (_rx_buf.p_hdr->type == msg_test_testdef);
    got_testdef &= _rx_buf.from == master_id;
    got_testdef &= _rx_buf.to == _rf95_dg.thisAddress();
  }
  // Copy out testdef to receive buffer
  memcpy(rx_testdef, &_rx_buf.data[MSG_PAYLOAD_START], sizeof(lora_testdef_t));
  dbg_print_testdef((lora_testdef_t*) &_rx_buf.data[MSG_PAYLOAD_START]);
}

// // Update to current test definition
// set_cfg(&tx_testdef->cfg);

// uint8_t cnt = 0;
// while (cnt < tx_testdef->packet_cnt) {
//     breakout_set_led(BO_LED_1, false);
//     while (!_rf95.available()) {};
//     breakout_set_led(BO_LED_1, true);
//     Serial.printf("Received %d bytes! - ", rx_msg_len);
//     rx_msg_len = sizeof(rx_buf);
//     _rf95.recv(rx_buf, &rx_msg_len);
//     if (rx_msg_len >= sizeof(radio_msg_t)) {
//         if (rx_header->type == msg_test_packet) {
//             cnt++;
//             Serial.printf("Test Packet!\n");
//         } else {
//             Serial.printf("??? Packet\n");
//         }
//     }
// }
// Serial.printf("Finished receiving packets!");
// breakout_set_led(BO_LED_1, false);
// breakout_set_led(BO_LED_2, true);

//     // Send packets
//     set_cfg(&rx_testdef->cfg);
    
//     delay(100);
//     uint8_t cnt = 0;
//     while (cnt < rx_testdef->packet_cnt) {
//         Serial.printf("Sending packet!\n");
//         // Send ACK! (No Payload)
//         tx_msg_len = LEN_MSG_EMPTY;
//         tx_header->type = msg_test_packet;
//         _rf95.send(tx_buf, tx_msg_len);
//         _rf95.waitPacketSent();
//         cnt++;
//         breakout_set_led(BO_LED_1, true);
//         delay(200);
//         breakout_set_led(BO_LED_1, false);
//     }
//     Serial.printf("Finished sending packets!\n");
// }

void LoRaModule::set_interrupt(bool value) {
  _interrupt = value;
}

bool LoRaModule::check_interrupt(bool clear) {
  bool val = _interrupt;
  if (clear) {
    _interrupt = false;
  }
  return val;
}

void LoRaModule::dbg_print_cur_cfg(void) {
  Serial.printf("Current Radio Configuration:\n");
  dbg_print_cfg(&_cur_cfg, false);
}

void LoRaModule::dbg_print_cfg(lora_cfg_t *cfg, bool title) {
  if (title) {
    Serial.printf("Radio Configuration:\n");
  }
  Serial.printf("* Frequency (MHz): %.0f\n", cfg->freq);
  Serial.printf("* Spreading Factor: %d\n", cfg->sf);
  Serial.printf("* Power (dBm): %d\n", cfg->tx_dbm);
  Serial.printf("* Bandwidth (Hz): %d\n", cfg->bw);
  Serial.printf("* Coding rate: 4 / %d \n", cfg->cr4_denom);
  Serial.printf("* CRC Enable: %d\n", cfg->crc);
}

void LoRaModule::dbg_print_testdef(lora_testdef_t *testdef) {
  Serial.printf("Test Definition: %s\n", testdef->id);
  Serial.printf("* Packet Length: %d\n", testdef->packet_len);
  Serial.printf("* Packet Count: %d\n", testdef->packet_cnt);
  dbg_print_cfg(&testdef->cfg, false);
}

