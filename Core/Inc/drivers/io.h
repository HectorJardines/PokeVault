/**
 * This module handles GPIO pin initialization and implements
 * GPIO pin control functions. Also implements EXTI interrupt control 
 * functions.
 */
#ifndef _IO_H
#define _IO_H

#include "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_gpio.h"

/*************
 * ENUMS
 *************/
typedef enum {
    IO_A0, IO_A1, IO_A2, IO_A3,
    IO_A4, IO_A5, IO_A6, IO_A7,
    IO_A8, IO_A9, IO_A10, IO_A11,
    IO_A12, IO_A13, IO_A14, IO_A15,

    IO_B0, IO_B1, IO_B2, IO_B3,
    IO_B4, IO_B5, IO_B6, IO_B7,
    IO_B8, IO_B9, IO_B10, IO_B11,
    IO_B12, IO_B13, IO_B14, IO_B15,

    IO_C0, IO_C1, IO_C2, IO_C3,
    IO_C4, IO_C5, IO_C6, IO_C7,
    IO_C8, IO_C9, IO_C10, IO_C11,
    IO_C12, IO_C13, IO_C14, IO_C15
} io_pin_name_e;

typedef enum {
    IO_UNUSED_0 = IO_A0,
    IO_UNUSED_1 = IO_A1,
    IO_UNUSED_2 = IO_A2,
    IO_UNUSED_3 = IO_A3,
    IO_UNUSED_4 = IO_A4,
    IO_UNUSED_5 = IO_A5,
    IO_UNUSED_6 = IO_A6,
    IO_UNUSED_7 = IO_A7,
    IO_UNUSED_8 = IO_A8,
    IO_UNUSED_9 = IO_A9,
    IO_UNUSED_10 = IO_A10,
    IO_UNUSED_11 = IO_A11,
    IO_UNUSED_12 = IO_A12,
    IO_UNUSED_13 = IO_A13,
    IO_UNUSED_14 = IO_A14,
    IO_UNUSED_15 = IO_A15,

    IO_UNUSED_16 = IO_B0,
    IO_UNUSED_17 = IO_B1,
    IO_UNUSED_18 = IO_B2,
    IO_UNUSED_19 = IO_B3,
    IO_UNUSED_20 = IO_B4,
    IO_UNUSED_21 = IO_B5,
    IO_UNUSED_22 = IO_B6,
    IO_UNUSED_23 = IO_B7,
    IO_UNUSED_24 = IO_B8,
    IO_UNUSED_25 = IO_B9,
    IO_UNUSED_26 = IO_B10,
    IO_UNUSED_27 = IO_B11,
    IO_UNUSED_28 = IO_B12,
    IO_UNUSED_29 = IO_B13,
    IO_UNUSED_30 = IO_B14,
    IO_UNUSED_31 = IO_B15,

    IO_UNUSED_32 = IO_C0,
    IO_UNUSED_33 = IO_C1,
    IO_UNUSED_34 = IO_C2,
    IO_UNUSED_35 = IO_C3,
    IO_UNUSED_36 = IO_C4,
    IO_UNUSED_37 = IO_C5,
    IO_UNUSED_38 = IO_C6,
    IO_UNUSED_39 = IO_C7,
    IO_UNUSED_40 = IO_C8,
    IO_UNUSED_41 = IO_C9,
    IO_UNUSED_42 = IO_C10,
    IO_UNUSED_43 = IO_C11,
    IO_UNUSED_44 = IO_C12,
    IO_UNUSED_45 = IO_C13,
    IO_UNUSED_46 = IO_C14,
    IO_UNUSED_47 = IO_C15
} io_e;

typedef void(*isr_t)(void);

/*****************
 * PUBLIC APIs
 *****************/
/**
 * @brief Initializes all of the systems pins with the predefined pin configurations
 */
void io_init(void);

/**
 * @brief Registers the specified interrupt service routine to the specified pin
 * 
 * An array of interrupt service routines is stored mapping each EXTI compatible
 * pin to an ISR (NULL for those that are not used). This function configures the 
 * pin for EXTI interrupts and stores the corresponding ISR in the array.
 * 
 * @param isr the ISR that is to be associated with the pin
 * @param pin the pin to which the ISR will be associated
 * @param trigger the trigger (rising/falling edge) of the EXTI interrupt
 */
void io_register_isr(isr_t isr, io_e pin, uint8_t trigger);

#endif