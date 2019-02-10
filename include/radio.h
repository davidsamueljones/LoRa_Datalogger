/*
  TODO
*/

#ifndef RADIO_H
#define RADIO_H

#include <Arduino.h>
#include <RH_RF95.h>
#include <RHDatagram.h>
#include <RHReliableDatagram.h>

#define TESTDEF_ID_LEN (10)

/*
  Helper structure for holding information required to setup
  a unique RF95 LoRa radio.
*/
typedef struct lora_module_t {
    uint8_t radio_id;
    uint8_t pin_cs;
    uint8_t pin_rst;
    uint8_t pin_int;
} lora_module_t;

/*
  Full LoRa radio configuration. It is likely that many of the
  fields will be the same across various configurations.
*/
typedef struct lora_cfg_t {
  float freq; // MHz
  uint8_t sf;
  int8_t tx_dbm;
  long bw; // Hz
  uint8_t cr4_denom; // (4 / Val)
  bool crc;
} lora_cfg_t;

/*
  Full definition for a packet test.
*/
typedef struct lora_testdef_t {
  char id[TESTDEF_ID_LEN];
  uint16_t packet_cnt;
  uint8_t packet_len;
  lora_cfg_t cfg;
  uint8_t master_id;
  uint8_t slave_id;
} lora_testdef_t;

/*
  Types of message that can be sent by the mutual radio interface.
  This is designed to be part of the RadioHead packet payload and
  therefore ack message types should be considered seperate from
  the Radiohead datagram acknowledgment system.
*/
typedef enum radio_msg_type_t {
  msg_invalid = 0,  // Unconfigured message  
  msg_ack,          // General success acknowledgment
  msg_nack,         // General failure acknowledgment
  msg_test_qry,     // Query if ready to handle a test
  msg_test_rdy,     // Alert that ready to handle a test
  msg_test_testdef, // Packet contains test definition
  msg_test_packet,  // Test packet
} radio_msg_type_t;

/*
  Structure of our higher level packets, payload should directly
  follow this header.
  N.B: Use RadioHead datagram packet for addressed packets. 
*/
typedef struct radio_msg_t {
  radio_msg_type_t type;
} radio_msg_t;

// Hardcoded positions to place data in a message
#define MSG_HEADER_START (0)
#define MSG_PAYLOAD_START (sizeof(radio_msg_t))

#define LEN_MSG_EMPTY (sizeof(radio_msg_t))
#define LEN_MSG_WITH_PAYLOAD(PAYLOAD_LENGTH) (LEN_MSG_EMPTY + PAYLOAD_LENGTH)

#define LEN_MSG_TESTDEF LEN_MSG_WITH_PAYLOAD(sizeof(lora_testdef_t))

#define MIN_TESTDEF_PACKET_LEN (RH_RF95_HEADER_LEN + LEN_MSG_EMPTY)
#define MAX_TESTDEF_PACKET_LEN (RH_MAX_MESSAGE_LEN)

/*
  Helper structure for handling a message queue. Holds a buffer long
  enough for the longest possible message, with a pointer mapping to
  the header section (our header, not RadioHead's).
*/
typedef struct radio_msg_buffer_t {
  uint8_t from;
  uint8_t to;
  uint8_t len;
  uint8_t data[RH_RF95_MAX_MESSAGE_LEN]; 
  radio_msg_t* p_hdr = (radio_msg_t*) &data[MSG_HEADER_START];
} radio_msg_buffer_t;

/*
  TODO
*/
class LoRaModule {
  public:
    LoRaModule(lora_module_t *module_cfg, lora_cfg_t *base_cfg);
    bool radio_init(void);
    void reset_to_base_cfg(void);
    void set_cfg(lora_cfg_t *new_cfg);

    bool acknowledged_tx(uint8_t attempts);
    bool unacknowledged_tx(void);
    
    bool acknowledged_rx(uint16_t timeout=0);
    bool unacknowledged_rx(uint16_t timeout=0);

    bool send_testdef(lora_testdef_t *tx_testdef);
    bool recv_testdef(lora_testdef_t *recv_testdef);

    bool send_testdef_packets(lora_testdef_t *testdef);

    // Debug functions
    void dbg_print_cur_cfg(void);
    void dbg_print_cfg(lora_cfg_t *cfg, bool title = true);
    void dbg_print_testdef(lora_testdef_t *testdef);

    // Interrupt handling
    void set_interrupt(bool value);
    bool check_interrupt(bool clear = true);

    // Low level radio interface
    RH_RF95 _rf95;
    // Addressed reliable interface
    RHReliableDatagram _rf95_dg;
  private:
    lora_module_t _module_cfg;
    lora_cfg_t _base_cfg;
    lora_cfg_t _cur_cfg;
    radio_msg_buffer_t _tx_buf; // may get trashed during acknowledged transmissions
    radio_msg_buffer_t _rx_buf;
    volatile bool _interrupt;
};

// Hardcoded base configuration file
extern lora_cfg_t hc_base_cfg;

// Global radio instances 
// (multiple in case future dataloggers have separate radios for tx and rx)
extern LoRaModule* g_radio_a;
extern LoRaModule* g_radio_b;

#endif // RADIO_H
