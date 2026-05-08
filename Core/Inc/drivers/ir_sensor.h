/**
 * This module implements the ir sensor line break detection
 * functionality. Utilizes the adc peripheral.
 */
#ifndef _IR_SENSOR_H
#define _IR_SENSOR_H

#include "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_adc.h"

/*******************
 *  ENUMS/STRUCTS
 *******************/
typedef enum {
    IR_LINE_BROKEN, // LINE BROKEN, UNIT CLOSED
    IR_LINE_CONNECTED // LINE CONNECTED, UNIT OPEN
} ir_line_status_e;

typedef enum {
    IR_LINE_UNIT1,
    IR_LINE_UNIT2,
    IR_LINE_UNIT3
} ir_line_unit_e; // each ir line corresponds to a storage unit

typedef struct {
    ir_line_status_e unit1_status;
    ir_line_status_e unit2_status;
    ir_line_status_e unit3_status;
} ir_lines_t;

/*******************
 *  USER APIs
 *******************/

/**
 * @brief Intializes the ADC peripheral used to read values from the IR emitters
 */
void ir_init(void);

/**
 * @brief Checks the status of the lines in each of the storage units
 * 
 * The system consists of three storage unit, each with an ir line break 
 * mechanism that indicates whether the unit is open or closed. When the "line"
 * is connected the box is open, otherwise it is closed.
 * 
 * @param lines struct that holds line status values
 */
void ir_check_lines(ir_lines_t *lines);

/**
 * @brief Enables the ir line for the specified unit
 * 
 * This API allows the user to enable the IR line for a specified 
 * storage unit. Used when the system security measures are enabled.
 * 
 * @param unit the storage unit to disable ir line of
 */
void ir_enable_line(ir_line_unit_e unit);

/**
 * @brief Disables the ir line for the specified unit
 * 
 * This API allows the user to disable the IR line for the specified
 * storage unit. Used when the system security measures are disabled.
 * 
 * @param unit The storage unit whose line is to be disabled
 */
void ir_disable_line(ir_line_unit_e unit);

#endif