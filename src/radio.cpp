#include <Arduino.h>

#include "radio.h"
#include "breakout.h"

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
LoRaModule* g_radio_c = NULL;

LoRaModule::LoRaModule(lora_module_t *module_cfg, lora_cfg_t *base_cfg) : 
    _rf95(module_cfg->pin_cs, module_cfg->pin_int), 
    _module_cfg(*module_cfg), 
    _base_cfg(*base_cfg)
    {}

// TODO: Can we remove delays?
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
    // Check for driver init success
    bool success = _rf95.init();
    Serial.printf("Radio initialisation %s!\n", success ? "successful" : "failed");

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

void LoRaModule::send_testdef(lora_testdef_t *tx_testdef) {

    // Setup TX Buffer
    uint8_t tx_msg_len = 0;
    uint8_t tx_buf[RH_RF95_MAX_MESSAGE_LEN];
    radio_msg_t* tx_header = (radio_msg_t*) &tx_buf[MSG_HEADER_START];
    
    //strcpy(tx_header->radio_id, _module_cfg.radio_id);
    
    // Setup RX Buffer
    uint8_t rx_msg_len = 0;
    uint8_t rx_buf[RH_RF95_MAX_MESSAGE_LEN];
    radio_msg_t* rx_header = (radio_msg_t*) &rx_buf[MSG_HEADER_START];

    // Send RDY? (No Payload) 
    tx_msg_len = sizeof(radio_msg_t);
    tx_header->type = msg_test_rdy;
    Serial.printf("Sending %d bytes!\n", tx_msg_len);
    bool sent = _rf95.send(tx_buf, tx_msg_len);
    _rf95.waitPacketSent();
    Serial.printf("TX RDY? : [%s]\n", sent ? "Successful" : "Failed"); 

    // Receive ACK! (No Payload Expected)
    bool got_ack = false;
    while (!_rf95.available()) {};
    rx_msg_len = sizeof(rx_buf);
    _rf95.recv(rx_buf, &rx_msg_len);
    Serial.printf("Received %d bytes!\n", rx_msg_len);
    Serial.printf("Expected %d bytes!\n", sizeof(radio_msg_t));  
    if (rx_msg_len >= sizeof(radio_msg_t)) {
        if (rx_header->type == msg_test_ack) {
            got_ack = true;
        }
    }
    Serial.printf("RX ACK! : [%s]\n", got_ack ? "Successful" : "Failed"); 
    
    if (got_ack == false) {
        return;
    }

    // Send Test Definition
    tx_msg_len = sizeof(radio_msg_t) + sizeof(lora_testdef_t);
    tx_header->type = msg_test_testdef;
    memcpy(&tx_buf[MSG_PAYLOAD_START], tx_testdef, sizeof(lora_testdef_t));
    
    dbg_print_cfg(&tx_testdef->cfg);
    Serial.printf("Sending %d bytes!\n", tx_msg_len);
    sent = _rf95.send(tx_buf, tx_msg_len);
    _rf95.waitPacketSent();
    Serial.printf("TX Test Definition : [%s]\n", sent ? "Successful" : "Failed");

    // Receive ACK! (No Payload Expected)
    got_ack = false;
    while (!_rf95.available()) {};
    rx_msg_len = sizeof(rx_buf);
    _rf95.recv(rx_buf, &rx_msg_len);
    Serial.printf("Received %d bytes!\n", rx_msg_len);
    Serial.printf("Expected %d bytes!\n", sizeof(radio_msg_t));  
    if (rx_msg_len >= sizeof(radio_msg_t)) {
        if (rx_header->type == msg_test_ack) {
            got_ack = true;
        }
    }
    Serial.printf("RX ACK! : [%s]\n", got_ack ? "Successful" : "Failed"); 
    
    // Update to current test definition
    set_cfg(&tx_testdef->cfg);

    uint8_t cnt = 0;
    while (cnt < tx_testdef->packet_cnt) {
        breakout_set_led(BO_LED_1, false);
        while (!_rf95.available()) {};
        breakout_set_led(BO_LED_1, true);
        Serial.printf("Received %d bytes! - ", rx_msg_len);
        rx_msg_len = sizeof(rx_buf);
        _rf95.recv(rx_buf, &rx_msg_len);
        if (rx_msg_len >= sizeof(radio_msg_t)) {
            if (rx_header->type == msg_test_packet) {
                cnt++;
                Serial.printf("Test Packet!");
            } else {
                Serial.printf("??? Packet");
            }
        }
    }
    breakout_set_led(BO_LED_1, false);
    breakout_set_led(BO_LED_2, true);

}

void LoRaModule::recv_testdef(lora_testdef_t *rx_testdef) {
    // Setup TX Buffer
    uint8_t tx_msg_len = 0;
    uint8_t tx_buf[RH_RF95_MAX_MESSAGE_LEN];
    radio_msg_t* tx_header = (radio_msg_t*) &tx_buf[MSG_HEADER_START];
    //strcpy(tx_header->radio_id, _module_cfg.radio_id);
    // Setup RX Buffer
    uint8_t rx_msg_len = 0;
    uint8_t rx_buf[RH_RF95_MAX_MESSAGE_LEN];
    radio_msg_t* rx_header = (radio_msg_t*) &rx_buf[MSG_HEADER_START];

    // Receive RDY? (No Payload Expected)
    bool got_rdy = false;
    Serial.printf("Waiting for RDY?...\n");
    while (!_rf95.available()) {};
    rx_msg_len = sizeof(rx_buf);
    _rf95.recv(rx_buf, &rx_msg_len);
    Serial.printf("Received %d bytes!\n", rx_msg_len);
    Serial.printf("Expected %d bytes!\n", LEN_MSG_EMPTY);  
    if (rx_msg_len >= LEN_MSG_EMPTY) {
        if (rx_header->type == msg_test_rdy) {
            got_rdy = true;
        } else {
            Serial.printf("Not a ready!\n"); 
        }
    }
    if (!got_rdy) {
        return;
    }

    // Send ACK! (No Payload)
    tx_msg_len = LEN_MSG_EMPTY;
    tx_header->type = msg_test_ack;
    _rf95.send(tx_buf, tx_msg_len);
    _rf95.waitPacketSent();

    // Receive Test Definition
    Serial.printf("Waiting for Test Definition...\n");
    while (!_rf95.available()) {};
    rx_msg_len = sizeof(rx_buf);
    _rf95.recv(rx_buf, &rx_msg_len);
    Serial.printf("Received %d bytes!\n", rx_msg_len);
    Serial.printf("Expected %d bytes!\n", LEN_MSG_EMPTY + sizeof(lora_testdef_t));  
    if (rx_header->type == msg_test_testdef) {
        if (rx_msg_len - sizeof(lora_testdef_t)) {
            memcpy(rx_testdef, &rx_buf[MSG_PAYLOAD_START], sizeof(lora_testdef_t));
            dbg_print_cfg(&rx_testdef->cfg);
        } else {
            Serial.printf("Not long enough for test def");
        }
    }

    // Send ACK! (No Payload)
    tx_msg_len = LEN_MSG_EMPTY;
    tx_header->type = msg_test_ack;
    _rf95.send(tx_buf, tx_msg_len);
    _rf95.waitPacketSent();

    // Send packets
    set_cfg(&rx_testdef->cfg);
    
    delay(100);
    uint8_t cnt = 0;
    while (cnt < rx_testdef->packet_cnt) {
        // Send ACK! (No Payload)
        tx_msg_len = LEN_MSG_EMPTY;
        tx_header->type = msg_test_packet;
        _rf95.send(tx_buf, tx_msg_len);
        _rf95.waitPacketSent();
        cnt++;
        breakout_set_led(BO_LED_1, true);
        delay(200);
        breakout_set_led(BO_LED_1, false);
    }

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

