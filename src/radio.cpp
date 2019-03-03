#include <Arduino.h>
#include <math.h>

#include "radio.h"
#include "breakout.h"
#include "storage.h"

#define SINGLE_RX_CHECK_TIMEOUT (500)
#define RDY_RX_TIMEOUT (3000)
#define HEARTBEAT_TIMEOUT (3000)
#define TESTDEF_RX_TIMEOUT (5000)
#define ACK_TIMEOUT (1000)

// Delay from after configuration before slave starts sending packets
#define SLAVE_PACKET_SEND_DELAY (1000)

// Airtime will not reflect all time for a single packet, will require extra processing.
// TODO: Currently captured as a simple multiplier, is something more sophisticated better?
#define AIRTIME_MULTIPLIER (1.3)
#define RX_PROCESS_TIME_MS (100)

lora_cfg_t hc_base_cfg = {
  .freq = 869.525f,
  .sf = 12,
  .tx_dbm = 14,
  .bw = 125000,
  .cr4_denom = 8,
  .preamble_syms = 8,
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
  _rf95_dg.setTimeout(ACK_TIMEOUT); 
  // Initialise any buffer values
  _tx_buf.from = _rf95_dg.thisAddress();
  return success;
}

void LoRaModule::reset_to_base_cfg(void) {
  set_cfg(&_base_cfg);
}

void LoRaModule::set_cfg(lora_cfg_t *new_cfg) {
  Serial.printf("Setting new configuration...\n");
  _rf95.setModeIdle();
  _rf95.setFrequency(new_cfg->freq);
  _rf95.setSpreadingFactor(new_cfg->sf);
  _rf95.setTxPower(new_cfg->tx_dbm);
  _rf95.setSignalBandwidth(new_cfg->bw);
  _rf95.setCodingRate4(new_cfg->cr4_denom);
  _rf95.setPreambleLength(new_cfg->preamble_syms);
  _rf95.setPayloadCRC(new_cfg->crc);
  _cur_cfg = *new_cfg;
  Serial.printf("Configuration set!\n");
}

bool LoRaModule::acknowledged_tx(radio_msg_buffer_t *tx_buf, uint8_t attempts) {
  bool sent = false;
  uint8_t attempt = 0;
  Serial.printf("Sending acknowledged %d bytes...\n", tx_buf->len);
  // Disable retrying so we can escape in case of interrupt
  _rf95_dg.setRetries(1);
  // Handle multiple attempt behaviour, lots of retry behaviour not mirrored 
  // from RadioHead but good enough
  while (!sent && (attempts == 0 || attempt < attempts)) {
    attempt++;
    Serial.printf("* Attempt: %d\n", attempt);
    if (check_interrupt()) {
      Serial.printf("Interrupted waiting for acknowledged TX!\n", tx_buf->len);
      break;
    }
    sent = _rf95_dg.sendtoWait(tx_buf->data, tx_buf->len, tx_buf->to);
  }
  Serial.printf("TX %s!\n", sent ? "successful" : "failed");
  return sent;
}

bool LoRaModule::unacknowledged_tx(radio_msg_buffer_t *tx_buf) {
  Serial.printf("Sending unacknowledged %d bytes...\n", tx_buf->len);
  bool queued = _rf95_dg.sendto(tx_buf->data, tx_buf->len, tx_buf->to);
  if (!queued) {
    Serial.printf("TX queued unsucessfully!\n");
    return false;
  }
  // Wait until it has actually been sent
  bool sent = _rf95_dg.waitPacketSent();
  Serial.printf("TX %s!\n", sent ? "successful" : "failed");
  return sent;
}

bool LoRaModule::acknowledged_rx(radio_msg_buffer_t *rx_buf, uint16_t timeout) {
  uint8_t exp_rx_len = _rx_buf.len;
  bool received = false;
  uint16_t time = 0;
  Serial.printf("Waiting for acknowledged RX...\n");
  while (!received && (timeout == 0 || time < timeout)) {
      rx_buf->len = exp_rx_len;
      // Copy full message if length not pre-configured correctly
      if (rx_buf->len == 0 || rx_buf->len > RH_RF95_MAX_MESSAGE_LEN) {
        rx_buf->len = RH_RF95_MAX_MESSAGE_LEN;
      }
      // Wait for a message, receive it, and acknowledge it
      received = _rf95_dg.recvfromAckTimeout(rx_buf->data, &rx_buf->len, 
                                      SINGLE_RX_CHECK_TIMEOUT, &rx_buf->from, &rx_buf->to);
      time += SINGLE_RX_CHECK_TIMEOUT;
      if (check_interrupt()) {
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

bool LoRaModule::unacknowledged_rx(radio_msg_buffer_t *rx_buf, uint16_t timeout) {
  uint8_t exp_rx_len = rx_buf->len;
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
      rx_buf->len = exp_rx_len;
      if (rx_buf->len == 0 || rx_buf->len > RH_RF95_MAX_MESSAGE_LEN) {
        rx_buf->len = RH_RF95_MAX_MESSAGE_LEN;
      }
      received = _rf95_dg.recvfrom(rx_buf->data, &rx_buf->len, 
                                      &rx_buf->from, &rx_buf->to);
      // Count as failed receive if not meant for us
      received &=  ((rx_buf->to == RH_BROADCAST_ADDRESS) ||
                    (rx_buf->to == _rf95_dg.thisAddress()));
    }
    if (check_interrupt()) {
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

radio_cmd_t LoRaModule::recv_command(uint8_t *master_id) {
  Serial.printf("Waiting for a command from master...\n");
  bool got_cmd = false;
  while (!got_cmd) {
    _rx_buf.len = LEN_MSG_EMPTY;
    got_cmd = unacknowledged_rx(&_rx_buf);
    if (check_interrupt()) {
      break;
    }
    // Can't do much if we don't know who to reply to
    if (!got_cmd || _rx_buf.from == RH_BROADCAST_ADDRESS) {
      continue;
    }
    *master_id = _rx_buf.from;

    // Handle conversion to command
    switch (_rx_buf.p_hdr->type) {
      case msg_test_qry:
        Serial.printf("Received message is a handle testdef command!\n");
        return cmd_testdef;
      case msg_heartbeat:
        Serial.printf("Received message is a heartbeat command!\n");
        return cmd_heartbeat;
      default:
        Serial.printf("Received message is not a command!\n");
        break;
    }
  }
  return cmd_invalid;
}

bool LoRaModule::send_testdef(lora_testdef_t *tx_testdef) {
    // Send QRY? command and wait for someone to say RDY!
    bool got_rdy = false;
    while(!got_rdy) {
      Serial.printf("Sending QRY? request to slaves...\n");
      _tx_buf.to = RH_BROADCAST_ADDRESS;
      _tx_buf.len = sizeof(radio_msg_t);
      _tx_buf.p_hdr->type = msg_test_qry;
      bool sent = unacknowledged_tx(&_tx_buf);
      if (!sent || check_interrupt())
        return false;
      Serial.printf("Waiting for RDY! from a slave...\n");
      _rx_buf.len = LEN_MSG_EMPTY;
      got_rdy = acknowledged_rx(&_rx_buf, RDY_RX_TIMEOUT);
      if (!got_rdy || check_interrupt())
        return false;
      // Verify received message is a RDY!
      got_rdy = (_rx_buf.p_hdr->type == msg_test_rdy);
      got_rdy &= _rx_buf.from != RH_BROADCAST_ADDRESS;
      got_rdy &= _rx_buf.to == _rf95_dg.thisAddress();
      Serial.printf("Received message is%s a RDY!\n", got_rdy ? "" : " not");
    }
    // Track the members of the exchange
    tx_testdef->master_id = _rf95_dg.thisAddress();
    tx_testdef->slave_id = _rx_buf.from;

    // Send Test Definition
    Serial.printf("Sending test definition to slave...\n");
    _tx_buf.to = tx_testdef->slave_id;
    _tx_buf.len = LEN_MSG_TESTDEF;
    _tx_buf.p_hdr->type = msg_test_testdef;
    memcpy(&_tx_buf.data[MSG_PAYLOAD_START], tx_testdef, sizeof(lora_testdef_t));
    dbg_print_testdef((lora_testdef_t*) &_tx_buf.data[MSG_PAYLOAD_START]);
    bool acked_testdef = acknowledged_tx(&_tx_buf, 3);
    if (!acked_testdef || check_interrupt())
      return false;  
    Serial.printf("Testdef delivered successfully!\n");

    return true;
}

bool LoRaModule::recv_testdef(lora_testdef_t *rx_testdef) {
  // Respond to master with a RDY!
  _tx_buf.to = rx_testdef->master_id;
  _tx_buf.len = LEN_MSG_EMPTY;
  _tx_buf.p_hdr->type = msg_test_rdy;
  Serial.printf("Responding with RDY! to master...\n");
  bool acked_rdy = acknowledged_tx(&_tx_buf, 3);
  if (!acked_rdy || check_interrupt())
    return false;
  Serial.printf("Got acknowledgment to RDY!\n");

  // Receive Test Definition
  bool got_testdef = false;
  Serial.printf("Waiting for test definition from master...\n");
  while (!got_testdef) {
    // Give up if we haven't received any message within a timeout of the last
    _rx_buf.len = LEN_MSG_TESTDEF;
    got_testdef = acknowledged_rx(&_rx_buf, TESTDEF_RX_TIMEOUT);
    if (!got_testdef || check_interrupt())
      return false;
    // Verify received message is a test definition
    got_testdef &= _rx_buf.p_hdr->type == msg_test_testdef;
    got_testdef &= _rx_buf.from == rx_testdef->master_id;
    got_testdef &= _rx_buf.to == _rf95_dg.thisAddress();
  }
  // Copy out testdef to receive buffer
  memcpy(rx_testdef, &_rx_buf.data[MSG_PAYLOAD_START], sizeof(lora_testdef_t));
  Serial.printf("Testdef received successfully!\n");
  dbg_print_testdef((lora_testdef_t*) &_rx_buf.data[MSG_PAYLOAD_START]);
  Serial.printf("\n");
  return true;
}

bool LoRaModule::send_testdef_packets(lora_testdef_t *testdef) {
  // Verify anything that could start trashing memory
  if (testdef->packet_len < MIN_TESTDEF_PACKET_LEN || testdef->packet_len > MAX_TESTDEF_PACKET_LEN) {
    Serial.printf("Packet length to send must be between %d and %d but was %d!\n",
        MIN_TESTDEF_PACKET_LEN, MAX_TESTDEF_PACKET_LEN, testdef->packet_len);
    return false;
  }
  // Use the mutually agreed configuration
  set_cfg(&testdef->cfg);

  // Configure message
  _tx_buf.to = testdef->master_id;
  _tx_buf.p_hdr->type = msg_test_packet;
  _tx_buf.len = testdef->packet_len - RH_RF95_HEADER_LEN;
  uint8_t payload_len = _tx_buf.len - LEN_MSG_EMPTY;
    
  // Repeating pattern every 4 bytes for irrelevent data
  uint8_t pattern[] = {0xF0, 0x0F, 0xAA, 0x55};
  for (uint8_t i=0; i < payload_len; i++) {
    _tx_buf.data[MSG_PAYLOAD_START + i] = pattern[payload_len % 
                                              sizeof(pattern) / sizeof(pattern[0])]; 
  }
  Serial.printf("Sending %d packets of length %d...\n", 
    testdef->packet_cnt, testdef->packet_len);
  // Give the master some time to prepare
  delay(SLAVE_PACKET_SEND_DELAY);

  // Fire and forget the number of packets requested
  uint16_t packet = 0;
  uint32_t start_time = millis();
  while (packet < testdef->packet_cnt) {
    if (check_interrupt()) {
      Serial.printf("Interrupted when sending packets!\n", packet);
      return false;
    }
    Serial.printf("Sending Packet %d...\n", packet);
    _tx_buf.p_hdr->id = packet;
    // If any fail to send we'll just ignore it, this shouldn't happen
    unacknowledged_tx(&_tx_buf);
    packet++;
    delay(RX_PROCESS_TIME_MS);
  }
  // Calculate the sending duration, not worrying about wraps, should be good
  // for 49 days...
  uint32_t duration = millis() - start_time;
  Serial.printf("Took %dms to send all packets!\n", duration);
  return true;
}

uint16_t LoRaModule::rx_bad_since_last_check(void) {
  static uint16_t rx_bad_last = _rf95.rxBad();

  // Check if we have any bad receives
  uint16_t rx_bad = _rf95.rxBad();
  // Find how many have occurred since last check
  uint16_t rx_bad_since = rx_bad;
  if (rx_bad < rx_bad_last) {
    rx_bad_since += UINT16_MAX;
  }
  rx_bad_since -= rx_bad_last;
  rx_bad_last = rx_bad;

  return rx_bad_since;
}

bool LoRaModule::recv_testdef_packets(lora_testdef_t *testdef, uint16_t *recv_packets, File* log_file) {
  bool results_valid = true;
  File results_file = storage_init_result_file(testdef->id);
  // Use the mutually agreed configuration
  set_cfg(&testdef->cfg);
  uint16_t valid_packets = 0;
  // Track the number of failed receives
  uint32_t rx_bad_total = 0;
  rx_bad_since_last_check(); // reset the functions internal count

  // Determine a timeout that should allow all packets to be captured
  uint32_t packet_airtime = calculate_packet_airtime(&testdef->cfg, testdef->packet_len);
  uint32_t packet_timeout = packet_airtime * AIRTIME_MULTIPLIER + RX_PROCESS_TIME_MS;
  uint32_t timeout = packet_timeout * (testdef->packet_cnt+1) + SLAVE_PACKET_SEND_DELAY;
  SERIAL_AND_LOG((*log_file), "Waiting for packets for %dms...\n", timeout);

  uint32_t start_time = millis();
  int32_t time_left = 0;
  while ((time_left = timeout - (millis() - start_time)) > 0) {
    if (check_interrupt()) {
      SERIAL_AND_LOG((*log_file), "Interrupted when receiving packets!\n");
      results_valid = false;
      break;
    }
    _rx_buf.len = testdef->packet_len - RH_RF95_HEADER_LEN;
    bool got_packet = unacknowledged_rx(&_rx_buf, SINGLE_RX_CHECK_TIMEOUT);

    if (got_packet) {
      // Verify received message is a test packet, contents should be
      // pre-verified by crc check so only valid packets should reach this stage
      got_packet &= _rx_buf.p_hdr->type == msg_test_packet;
      got_packet &= _rx_buf.from == testdef->slave_id;
      got_packet &= _rx_buf.to == testdef->master_id;
      got_packet &= (_rx_buf.len + RH_RF95_HEADER_LEN) == testdef->packet_len;
      
      if (got_packet) {
        valid_packets++;
        int16_t rssi = _rf95.lastRssi();
        int16_t snr = _rf95.lastSNR();
        rx_bad_total += rx_bad_since_last_check();
        Serial.printf("Packet Received | [ID: %d] [RSSI: %ddBm] [SNR: %ddB] " \
                        "[Packets: %d/%d] [Bad Recvs: %ld] [Time Left: %ld]\n", 
                        _rx_buf.p_hdr->id, rssi, snr, valid_packets, testdef->packet_cnt, 
                        rx_bad_total, time_left);
        // Record to results file
        storage_write_result(&results_file, _rx_buf.p_hdr->id, rssi, snr, rx_bad_total, time_left);
        // Got the last packet, may as well stop
        if (_rx_buf.p_hdr->id == (testdef->packet_cnt - 1)) {
          break;
        }
        // TODO: Could adjust timeout based on received packets
      }
    }
  }
  rx_bad_total += rx_bad_since_last_check();
  if (time_left < 0) {
    SERIAL_AND_LOG((*log_file), "Timed out when receiving packets!\n");
  }
  SERIAL_AND_LOG((*log_file), "Finished receiving [%d/%d] packets!\n", valid_packets, testdef->packet_cnt);
  SERIAL_AND_LOG((*log_file), "In this time %d failed receive(s) occurred!\n", rx_bad_total);

  if (recv_packets != NULL) {
    *recv_packets = valid_packets;
  }
  results_file.close();
  return results_valid;
}

bool LoRaModule::send_heartbeat(void) {
  _tx_buf.to = RH_BROADCAST_ADDRESS;
  _tx_buf.len = LEN_MSG_EMPTY;
  _tx_buf.p_hdr->type = msg_heartbeat;
  bool sent = unacknowledged_tx(&_tx_buf);
  if (!sent || check_interrupt()) {
    return false;
  }
  bool recv_ack = false;
  uint32_t end_time = millis() + HEARTBEAT_TIMEOUT;
  while (!recv_ack && millis() < end_time) {
    recv_ack = unacknowledged_rx(&_rx_buf, HEARTBEAT_TIMEOUT);
    recv_ack &= _rx_buf.to == _rf95_dg.thisAddress();
    recv_ack &= _rx_buf.p_hdr->type == msg_ack;
  }
  return recv_ack;
}

void LoRaModule::ack_heartbeat(uint8_t master_id) {
  _tx_buf.to = master_id;
  _tx_buf.len = LEN_MSG_EMPTY;
  _tx_buf.p_hdr->type = msg_ack;
  unacknowledged_tx(&_tx_buf);
}

uint32_t LoRaModule::calculate_packet_airtime(lora_cfg_t *cfg, uint16_t packet_len) {
    // All equations used from (modified slightly for our formats):
    // https://www.semtech.com/uploads/documents/LoraDesignGuide_STD.pdf

    float symbol_time = 1000.0 * pow(2, cfg->sf) / cfg->bw;	// ms
    float preamble_time = (cfg->preamble_syms + 4.25) * symbol_time;
    bool ldr = is_low_datarate_required(cfg);
    // N.B Explicit header is always enabled by RadioHead so -20 always present
    uint16_t psc_top = 8 * packet_len - 4 * cfg->sf + 28 + 16 - 20;
    uint16_t psc_bot = 4 * (cfg->sf - 2 * ldr);
    uint16_t psc_lhs = (int) ceil(psc_top / psc_bot);
    uint16_t payload_symbol_count = 8 + (max(psc_lhs * (cfg->cr4_denom), 0));
    float payload_time = payload_symbol_count * symbol_time;
    float total_time = preamble_time + payload_time;
    return total_time;
}

bool LoRaModule::is_low_datarate_required(lora_cfg_t *cfg) {
    float symbol_time = 1000.0 * pow(2, cfg->sf) / cfg->bw;	// ms
    // Value of 16.0 for symbol time required for enabling low data rate is copied from
    // RadioHead library. No source provided but keep the same for consistency.
    return symbol_time > 16.0;    
}

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
  Serial.printf("* Frequency (MHz): %.03f\n", cfg->freq);
  Serial.printf("* Spreading Factor: %d\n", cfg->sf);
  Serial.printf("* Power (dBm): %d\n", cfg->tx_dbm);
  Serial.printf("* Bandwidth (Hz): %d\n", cfg->bw);
  Serial.printf("* Coding rate: 4 / %d \n", cfg->cr4_denom);
  Serial.printf("* Preamble Symbols: %d \n", cfg->preamble_syms);
  Serial.printf("* CRC Enable: %d\n", cfg->crc);
}

void LoRaModule::dbg_print_testdef(lora_testdef_t *testdef) {
  Serial.printf("Test Definition: %s\n", testdef->id);
  Serial.printf("* Expected Range: %d\n", testdef->exp_range);
  Serial.printf("* Packet Length: %d\n", testdef->packet_len);
  Serial.printf("* Packet Count: %d\n", testdef->packet_cnt);
  dbg_print_cfg(&testdef->cfg, false);
}
