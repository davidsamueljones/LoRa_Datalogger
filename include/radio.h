/*
  TODO
*/

#ifndef RADIO_H
#define RADIO_H

#include <Arduino.h>
#include <RH_RF95.h>
#include <RHReliableDatagram.h>

#include <SdFat.h>

#define TESTDEF_ID_LEN (10)

#define NO_TIMEOUT (0)
#define NO_ATTEMPT_LIMIT (0)

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
  Those denoted by (!) can be different between nodes, the others
  must be consistent.
*/
typedef struct lora_cfg_t {
  float freq;             // MHz
  uint8_t sf;
  int8_t tx_dbm;          // (!)
  long bw;                // Hz 
  uint8_t cr4_denom;      // (4 / Val) (!)
  uint8_t preamble_syms;
  bool crc;               // (!)
} lora_cfg_t;

/*
  Full definition for a packet test.
*/
typedef struct lora_testdef_t {
  char id[TESTDEF_ID_LEN];
  // An arbitary user chosen value, can be used to discard testdefs
  // that are almost guarenteed to fail
  uint8_t exp_range; 
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
  msg_test_qry,     // Query if ready to handle a testdef
  msg_test_rdy,     // Alert that ready to handle a test
  msg_test_testdef, // Packet contains test definition
  msg_test_packet,  // Test packet
  msg_heartbeat,    // Heartbeat packet
} radio_msg_type_t;

/*
  Commands that can be used to trigger certain radio behaviour. 
*/
typedef enum radio_cmd_t {
  cmd_invalid = 0,  // Unconfigured message  
  cmd_testdef,
  cmd_heartbeat,
} radio_cmd_t;

/*
  Structure of our higher level packets, payload should directly
  follow this header.  We have our own ID separate from RadioHead
  so that we have more than 8 bits to work with.
  N.B: Use RadioHead datagram packet for addressed packets. 
*/
typedef struct radio_msg_t {
  radio_msg_type_t type;
  uint16_t id;
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

    bool acknowledged_tx(radio_msg_buffer_t *tx_buf, uint8_t attempts = NO_ATTEMPT_LIMIT);
    bool unacknowledged_tx(radio_msg_buffer_t *tx_buf);
    
    bool acknowledged_rx(radio_msg_buffer_t *rx_buf, uint16_t timeout = NO_TIMEOUT);
    bool unacknowledged_rx(radio_msg_buffer_t *rx_buf, uint16_t timeout = NO_TIMEOUT);
    
    bool send_testdef(lora_testdef_t *tx_testdef);

    radio_cmd_t recv_command(uint8_t *master_id);
    bool recv_testdef(lora_testdef_t *recv_testdef);

    bool send_testdef_packets(lora_testdef_t *testdef);
    bool recv_testdef_packets(lora_testdef_t *testdef, uint16_t *recv_packets, File* log_file);

    bool send_heartbeat(void);
    void ack_heartbeat(uint8_t master_id);

    static bool is_low_datarate_required(lora_cfg_t *cfg);
    static uint32_t calculate_packet_airtime(lora_cfg_t *cfg, uint16_t packet_len);

    // Debug functions
    void dbg_print_cur_cfg(void);
    static void dbg_print_cfg(lora_cfg_t *cfg, bool title = true);
    static void dbg_print_testdef(lora_testdef_t *testdef);

    // Interrupt handling
    void set_interrupt(bool value);
    bool check_interrupt(bool clear = false);
    
    // Low level radio interface
    RH_RF95 _rf95;
    // Addressed reliable interface
    RHReliableDatagram _rf95_dg;
  private:
    uint16_t rx_bad_since_last_check(void);

    // The pin configuration of the module 
    lora_module_t _module_cfg;
    // The radio module configuration that is hardcoded and therefore predictable
    lora_cfg_t _base_cfg;
    // The current radio module configuration
    lora_cfg_t _cur_cfg;
    // Buffer used by default for all class defined transmissions
    radio_msg_buffer_t _tx_buf; // may get trashed during acknowledged transmissions
    // Buffer used by default for all class defined receives
    radio_msg_buffer_t _rx_buf;
    // Flag set when an external source wants current behaviour to finish
    volatile bool _interrupt;
};

// Hardcoded base configuration file
extern lora_cfg_t hc_base_cfg;

// Global radio instances 
// (multiple in case future dataloggers have separate radios for tx and rx)
extern LoRaModule* g_radio_a;
extern LoRaModule* g_radio_b;

#endif // RADIO_H
