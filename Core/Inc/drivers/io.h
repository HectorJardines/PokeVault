#ifndef _IO_H
#define _IO_H

#include "stm32f4xx.h"
#include "../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_gpio.h"
#include <stdbool.h>
#include <stdint.h>

/*******************************************
 * 				IO MACROS
 *******************************************/
// #define ENABLE		1
// #define	DISABLE		0

// #define SET_BIT		ENABLE

#define NO_OF_PINS_PER_REG    8
#define NO_OF_AFR_BITS        4

#define NO_OF_PINS_IN_CR  4U
#define CR_OFFSET_FACTOR  4U

#define DISABLE_IMR_MASK  1U

#define IO_PORT_CNT           3U
#define IO_PIN_CNT_PER_PORT   16U
#define IO_BSRR_BR_OFFSET       (16U)

#define PRx_SET     1U
#define NO_PRx_SET  23U

/*******************************************
 * 				       NVIC MACROS
 *******************************************/
#define NVIC_ISER0      ((volatile uint32_t*)0xE000E100)
#define NVIC_ISER1      ((volatile uint32_t*)0xE000E104)
#define NVIC_ISER2      ((volatile uint32_t*)0xE000E108)
#define NVIC_ISER3      ((volatile uint32_t*)0xE000E10C)

#define NVIC_ICER0      ((volatile uint32_t*)0xE000E180)
#define NVIC_ICER1      ((volatile uint32_t*)0xE000E184)
#define NVIC_ICER2      ((volatile uint32_t*)0xE000E188)
#define NVIC_ICER3      ((volatile uint32_t*)0xE000E18C)

#define NVIC_PR_BASE_ADDR		((volatile uint32_t*)0xE000E400)

#define TOTAL_NVIC_PR_BITS            8U
#define NO_PR_BITS_EN                 4U
#define NVIC_IPRn_OFFSET_MULTIPLIER   8U

/**************************************
 *            IO ENUMS
 **************************************/

typedef enum
{
  IO_PIN_0,
  IO_PIN_1,
  IO_PIN_2,
  IO_PIN_3,
  IO_PIN_4,
  IO_PIN_5,
  IO_PIN_6,
  IO_PIN_7,
  IO_PIN_8,
  IO_PIN_9,
  IO_PIN_10,
  IO_PIN_11,
  IO_PIN_12,
  IO_PIN_13,
  IO_PIN_14,
  IO_PIN_15
}io_pin_no_e;

typedef enum
{
  // PORTA PINS
  IO_A0, IO_A1, IO_A2, IO_A3, 
  IO_A4, IO_A5, IO_A6, IO_A7, 
  IO_A8, IO_A9, IO_A10, IO_A11, 
  IO_A12, IO_A13, IO_A14, IO_A15,
  
  // PORTB PINS
  IO_B0, IO_B1, IO_B2, IO_B3,
  IO_B4, IO_B5, IO_B6, IO_B7,
  IO_B8, IO_B9, IO_B10, IO_B11,
  IO_B12, IO_B13, IO_B14, IO_B15,

  //PORTC PINS
  IO_C0, IO_C1, IO_C2, IO_C3,
  IO_C4, IO_C5, IO_C6, IO_C7,
  IO_C8, IO_C9, IO_C10, IO_C11,
  IO_C12, IO_C13, IO_C14, IO_C15
} generic_io_e;

typedef enum
{
  // TODO: DEFINE IO PINS WITH NAMES TO BE USED FOR SENSORS,
  //       MOTOR DRIVERS, TIMERS, UART, ETC.
  IO_UNUSED_0 = IO_A0,
  IO_UNUSED_1 = IO_A1,
  IO_UART_TX = IO_A2,
  IO_UART_RX = IO_A3,
  IO_UNUSED_4 = IO_A4,
  IO_TEST_LED = IO_A5,
  IO_UNUSED_6 = IO_A6,
  IO_UNUSED_7 = IO_A7,
  IO_UNUSED_8 = IO_A8,
  IO_UNUSED_9 = IO_A9,
  IO_SPI_CS = IO_A10,
  IO_IR_EMIT_UNIT3 = IO_A11,
  IO_UNUSED_12 = IO_A12,
  IO_UNUSED_13 = IO_A13,
  IO_UNUSED_14 = IO_A14,
  IO_UNUSED_15 = IO_A15,

  IO_UNUSED_16 = IO_B0,
  IO_UNUSED_17 = IO_B1,
  IO_UNUSED_18 = IO_B2,
  IO_SPI1_SCK = IO_B3,
  IO_SPI1_MISO = IO_B4,
  IO_SPI1_MOSI = IO_B5,
  IO_UNUSED_22 = IO_B6,
  IO_I2C1_SDA = IO_B7,
  IO_I2C1_SCL = IO_B8,
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
  IO_IR_EMIT_UNIT1 = IO_C13,
  IO_IR_EMIT_UNIT2 = IO_C14,
  IO_UNUSED_47 = IO_C15,
} io_e;

typedef enum
{
	IO_MODE_INPUT,
	IO_MODE_OUTPUT,
	IO_MODE_ALT_FUN,
	IO_MODE_ANALOG
}io_mode_e;

typedef enum
{
	IO_SPEED_LOW,
	IO_SPEED_MEDIUM,
	IO_SPEED_FAST,
	IO_SPEED_HIGH
} io_speed_e;

typedef enum
{
	IO_OPTYPE_PP,
	IO_OPTYPE_OD,
} io_optype_e;

typedef enum
{
    IO_RES_NOPUPD,
    IO_RES_PU,
    IO_RES_PD
} io_resistance_e;

typedef enum
{
	IO_INTERRPT_RTFT,
	IO_INTERRUPT_RT,
	IO_INTERRUPT_FT
} isr_trigger_e;

typedef enum
{
	SYSCFG_IO_PORTA,
	SYSCFG_IO_PORTB,
	SYSCFG_IO_PORTC,
	SYSCFG_IO_PORTD
} syscfg_io_port_e;

typedef enum
{
	IO_PORTA,
	IO_PORTB,
	IO_PORTC,
	IO_PORTD
} io_port_e;

typedef enum
{
  IO_ALT_FUN_MODE0,
  IO_ALT_FUN_MODE1,
  IO_ALT_FUN_MODE2,
  IO_ALT_FUN_MODE3,
  IO_ALT_FUN_MODE4,
  IO_ALT_FUN_MODE5,
  IO_ALT_FUN_MODE6,
  IO_ALT_FUN_MODE7,
  IO_ALT_FUN_MODE8,
  IO_ALT_FUN_MODE9,
  IO_ALT_FUN_MODE10,
  IO_ALT_FUN_MODE11,
  IO_ALT_FUN_MODE12,
  IO_ALT_FUN_MODE13,
  IO_ALT_FUN_MODE14,
  IO_ALT_FUN_MODE15
} io_mode_alt_fun;

typedef enum
{
  IO_OUT_LOW,
  IO_OUT_HIGH,
  IO_OUT_NULL
} io_output_e;

/* IO CONFIG STRUCT */

typedef struct
{
	uint8_t PIN_NO;
	io_mode_e PIN_MODE;                 /*<! PIN MODE OPTIONS FROM @io_mode_e >*/
	io_speed_e PIN_SPEED;               /*<! PIN SPEED OPTIONS FROM @io_speed_e >*/
	io_optype_e PIN_OPTYPE;             /*<! PIN OUTPUT TYPE OPTIONS FROM @io_optype_e >*/
	io_resistance_e PIN_RESISTANCE;     /*<! PIN RESISTOR OPTIONS FROM @io_resistance_e >*/
    io_mode_alt_fun PIN_ALT_FUN_MODE;
    io_output_e PIN_OUT;
}io_config_t;

typedef struct
{
	GPIO_TypeDef *GPIOx;
	io_config_t  IO_Confg;
} io_handle_t;

typedef enum
{
	EXTI0, EXTI1, EXTI2, EXTI3,
	EXTI4, EXTI5, EXTI6, EXTI7,
	EXTI8, EXTI9, EXTI10, EXTI11,
	EXTI12, EXTI13, EXTI14, EXTI15
} exti_line_e;

typedef enum
{
	EXTI0_IRQ_NO = 6,
	EXTI1_IRQ_NO,
	EXTI2_IRQ_NO,
	EXTI3_IRQ_NO,
	EXTI4_IRQ_NO,
	EXTI9_5_IRQ_NO = 23,
	EXTI15_10_IRQ_NO = 40
}exti_irqn_e;

typedef enum
{
  LOW,
  HIGH
}io_value_e;

/* IO Interrupt APIs */
typedef void (*isr_function)(void);

/********************************************
*                 IO APIs
*********************************************/
/**
 * @brief Intializes and configures all GPIO pins
 * 
 * This API configures and initializes all GPIO pins 
 * to the provided state in the io configurations array.
 * All configuration changes must be changed in the array 
 * in io.c
 * 
 */
void IO_Init(void);

/**
 * @brief configures the EXTI interrtup for the specified io pin
 * 
 * This API configures the EXTI interrupt to trigger on the edge [trigger]
 * and executes the interrupt context [isr].
 * 
 * @param io
 * @param isr_trigger_e
 * @param isr_function
 */
void io_configure_interrupt(io_e io, isr_trigger_e trigger, isr_function isr);

/**
 * @brief Unregisters the ISR and triggger associated with the pin io
 * 
 * @param io
 */
void IO_DeInitIT(uint8_t io);

/**
 * @brief Enables the interrupt for the specified pin
 * 
 * Sets the ISER bit in the NVIC register corresponding to the 
 * pin io.
 * 
 * @param io
 */
void io_irq_enable_interrupt(io_e io);

/**
 * @brief Disables the interrupt service requests for the specified IRQn
 * 
 * Sets the ICER bits in the NVIC ICER bit corresponding 
 * to the provided IRQ number.
 * 
 * @param EXTI_IRQn
 */
void io_irq_disable_interrupt(uint16_t EXTI_IRQn);

/**
 * @brief Sets the interrupt priority
 * 
 * @param EXTI_IRQn externral interrupt IRQ number
 * @param IRQ_PR interrupt priority level
 */
void io_set_interrupt_prio(uint16_t EXTI_IRQn, uint8_t IRQ_PR);

/**
 * @brief Retrives the current running configuration of the specified IO pin
 * 
 * @param io pin to retrieve config of
 * @param io_config_t config struct to store the result in
 */
void io_get_current_config(io_e io, io_config_t *io_config);

/**
 * @brief Compares the pin configuration of two pins
 * 
 * @param config_1
 * @param config_2
 * @return 1 if configurations match; 0 else
 */
bool io_config_compare(const io_config_t *config_1, const io_config_t *config_2);

/**
 * @brief Sets the IO level of the specified pin
 * 
 * @param io
 * @param level
 */
void io_set_out(io_e io, io_output_e level);
/******************************************
*   IO Config Struct enums (for vl53l0x)
*******************************************/

#endif