/*
  TODO
*/

#ifndef RADIO_H
#define RADIO_H

#include <Arduino.h>
#include <RH_RF95.h>
//#include "breakout.h"

#define TESTDEF_ID_LEN 10

/*
  TODO
*/
typedef struct lora_module_t {
	uint8_t pin_cs;
	uint8_t pin_rst;
	uint8_t pin_int;
} lora_module_t;

/*
  TODO
*/
typedef struct lora_cfg_t {
	float freq;
	uint8_t sf;
	int8_t tx_dbm;
	long bw;
	uint8_t cr4_denom; // (4 / Val)
	bool crc;
} lora_cfg_t;

/*
  TODO
*/
typedef struct lora_testdef_t {
	char id[TESTDEF_ID_LEN];
	uint16_t packet_cnt;
	uint8_t packet_len;
	lora_cfg_t cfg;
} lora_testdef_t;

/*
  TODO
*/
typedef enum radio_msg_type_t {
	msg_test_err = 0,
	msg_test_rdy,
	msg_test_ack,
	msg_test_testdef,
	msg_test_packet,
} radio_msg_type_t;

/*
  TODO
	N.B: Use RadioHead datagram packet for device ID
*/
typedef struct radio_msg_t {
	radio_msg_type_t type;
} radio_msg_t;

// Hardcoded positions to place data in a message
#define MSG_HEADER_START (0)
#define MSG_PAYLOAD_START (sizeof(radio_msg_t))

#define LEN_MSG_EMPTY (sizeof(radio_msg_t))
#define LEN_MSG_WITH_PAYLOAD(PAYLOAD_LENGTH) (EMPTY_MSG_LENGTH + PAYLOAD_LENGTH)
/*
  TODO
*/
typedef struct radio_msg_buffer_t {
    uint8_t len;
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; 
    radio_msg_t* p_hdr = (radio_msg_t*) &buf[MSG_HEADER_START];
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
		void send_testdef(lora_testdef_t *tx_testdef);
		void recv_testdef(lora_testdef_t *recv_testdef);
		void send_test_packet();
		void recv_test_packet();
		void calc_exp_airtime();

		// Debug functions
		void dbg_print_cur_cfg(void);
		void dbg_print_cfg(lora_cfg_t *cfg, bool title = true);

		// Public instance of lower level radio interface
		RH_RF95 _rf95;
 private:
  	lora_module_t _module_cfg;
  	lora_cfg_t _base_cfg;
		lora_cfg_t _cur_cfg;
};

// Hardcoded base configuration file
extern lora_cfg_t hc_base_cfg;

// Global radio instances 
// (multiple in case future dataloggers have multiple radios)
extern LoRaModule* g_radio_a;
extern LoRaModule* g_radio_b;
extern LoRaModule* g_radio_c;




#endif // RADIO_H
