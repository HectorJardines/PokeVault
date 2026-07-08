#include "ir_sensors.h"
#include "io.h"
#include "common/defines.h"


/**********************
 * static declarations
 **********************/
/**
 * @brief Callback registered for IR pin RTFT interrupt
 * 
 * This callback function is called when the IR sensors pin
 * interrupt fires on either the rising or falling edge. 
 * Verifies which of them triggered interrupt and updates 
 * ir line state accordingly
 * 
 */
static void ir_line_cb(void);


/**
 * @brief Callback registered for PIR pin RTFT interrupt
 * 
 * This callback function is called from the PIR sensor's 
 * ISR registered to IO_PIR_IN triggered on rising or falling
 * edge. Verifies whether rising or falling edge triggered int
 * and updates PIR state accordingly.
 */
static void pir_presence_cb(void);

static ir_sens_state_t active_state;
/********************
 * PUBLICS DEFS
 ********************/

/**
 * @brief Initializes the IR and PIR sensors for the unit
 * 
 * 
 * 
 */
uint8_t ir_init(void) {
    io_configure_interrupt(IO_IR_RCV, IO_INTERRPT_RTFT, ir_line_cb);
    io_configure_interrupt(IO_PIR_RCV, IO_INTERRPT_RTFT, pir_presence_cb);

    return STATUS_OK;
}



/**
 * @brief Retrieves the current state of the IR sensors
 * 
 * 
 * @param[out] unit_state
 */
void ir_read_sens_state(ir_sens_state_t *unit_state) {
    unit_state->line_state = active_state.line_state;
    unit_state->presence_state = active_state.presence_state;
}



/*******************
 * STATIC DEFS
 *******************/
static void ir_line_cb(void) {
    if (io_get_out(IO_IR_RCV) == IO_OUT_HIGH)
        active_state.line_state = IR_LINE_CONNECTED;
    else
        active_state.line_state = IR_LINE_BROKEN;
}

static void pir_presence_cb(void) {
    if (io_get_out(IO_PIR_RCV) == IO_OUT_HIGH)
        active_state.presence_state = PIR_PRESENCE;
    else
        active_state.presence_state = PIR_NO_PRESENCE;
}


