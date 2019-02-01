/*
  Interface for the datalogger breakout board.
  @author David Jones (dsj1n15)
*/

#ifndef BREAKOUT_H
#define BREAKOUT_H

#include <Arduino.h>

// LED pin definitions
#define BO_LED_1 (1)
#define BO_LED_2 (2)
#define BO_LED_3 (3)

// Software switch pin definitions
#define BO_SWITCH_PIN1  (5)
#define BO_SWITCH_PIN2  (6)

// Radio pins
#define RFM95_CS  (24)
#define RFM95_RST (25)
#define RFM95_INT (26)

/*
  Enumeration of possible switch states.
*/
typedef enum sw_state_t {
	sw_state_top = 0,
	sw_state_mid,
	sw_state_bot,
  sw_state_unknown,
} sw_state_t;

/**
 * First time configuration of breakout board. Call before using any functionality.
 */
void breakout_init(void);

/**
 * Enable/Disable an LED. Note that no checks occur so ensure the pin provided is
 * an LED (or similar). Use provided breakout definitions.
 * 
 * @param led The led pin to configure
 * @param enable Whether the pin should be enabled or disabled
 */
void breakout_set_led(uint8_t led, bool enable);

/**
 * @return Switch position as enumerated state value
 */
sw_state_t breakout_get_switch_state(void);

#endif // BREAKOUT_H
