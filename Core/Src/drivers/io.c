/**
  ******************************************************************************
  * @file    io.c
  * @brief   This file provides code for io APIs
  *          of all used io pins.
  ******************************************************************************
**/

#include "drivers/io.h"
#include <stddef.h>
#include "common/defines.h"

#define ANALOG_IO_CONFG(GPIOX, IO_PIN_NO)	{(GPIOX), {(IO_PIN_NO), IO_MODE_ANALOG, IO_SPEED_MEDIUM, IO_OPTYPE_PP, IO_RES_NOPUPD}}
// #define UNUSED_IO_CONFG(GPIOX, IO_PIN_NO) 	{(GPIOX), {(IO_PIN_NO), IO_MODE_OUTPUT, IO_SPEED_FAST, IO_OPTYPE_PP, IO_RES_PD}}
#define UNUSED_IO_CONFG(GPIOX, IO_PIN_NO)	{NULL, {16}}

#define IO_PORT_OFFSET		 	(4U)
#define IO_PORT_MSK			 	(0x3U << IO_PORT_OFFSET)
#define IO_PIN_MSK			 	(0xFU)
#define IO_BITS_PER_PIN_MODE 	(2U)
#define IO_BITS_PER_PIN_PUPDR 	(2U)
#define IO_BSRR_BR_OFFSET       (16U)

#define IO_PIN_COUNT (48U)

/***************************
 * STATIC DECLARATIONS
 ***************************/

/** IO PIN INFORMATION APIs */
static inline io_port_e io_get_port(GPIO_TypeDef *GPIOx);
static bool compare_io_unused(const io_handle_t *io_h);
static GPIO_TypeDef* get_port_typedef(io_e io);
static uint8_t io_port(io_e io);
static uint8_t io_pin_idx(io_e io);
static uint8_t io_pin_bit(io_e io);

/** IO PIN CONFIGURATIONS APIs */
static void IO_SetResistance(GPIO_TypeDef *gpiox, const io_resistance_e res, const uint8_t io);
static void IO_SetMode(GPIO_TypeDef *gpiox, const io_mode_e mode, const uint8_t io);
static void IO_SetSpeed(GPIO_TypeDef *gpiox, const io_speed_e speed, const uint8_t io);
static void IO_SetOPType(GPIO_TypeDef *gpiox, const io_optype_e optype, const uint8_t io);
static void IO_SetAltFunMode(GPIO_TypeDef *gpiox, const io_mode_alt_fun alt_fun_mode, const uint8_t io);
static void  io_peripheral_control(GPIO_TypeDef *gpiox, uint8_t EnOrDi);
static void IO_Config(io_handle_t *io_handle);

/** IO INTERRUPT APIs */
static void IO_SetInterruptTrigger(uint8_t io, isr_trigger_e trigger);
static void IO_RegisterISR(uint8_t io, io_port_e port, isr_function isr);
static void IO_SetEXTILine(uint8_t io, GPIO_TypeDef* gpiox);
static exti_irqn_e io_get_irq_no(const uint8_t pin_idx);
static inline uint8_t retrieve_syscfg_exti_port(exti_line_e EXTIx);
static inline uint8_t get_exti_line(void);

/********************************************
 * 				STATIC IO ARRAYS
 ********************************************/
static isr_function isr_functions[IO_PORT_CNT][IO_PIN_CNT_PER_PORT] = {
	[IO_PORTA] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	[IO_PORTB] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	[IO_PORTC] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

static io_handle_t io_confgs[IO_PORT_CNT * IO_PIN_CNT_PER_PORT] = {
	//UART TX/RX
	[IO_UART_TX] = {GPIOA, {IO_PIN_2, IO_MODE_ALT_FUN, IO_SPEED_MEDIUM, IO_OPTYPE_PP, IO_RES_NOPUPD, IO_ALT_FUN_MODE7}},
	[IO_UART_RX] = {GPIOA, {IO_PIN_3, IO_MODE_ALT_FUN, IO_SPEED_MEDIUM, IO_OPTYPE_PP, IO_RES_NOPUPD, IO_ALT_FUN_MODE7}},

	// TEST LED
	[IO_TEST_LED] = {GPIOA, {IO_PIN_5, IO_MODE_OUTPUT, IO_SPEED_MEDIUM, IO_OPTYPE_PP, IO_RES_NOPUPD}},

	// I2C SDA/SCL PINS
	[IO_I2C1_SCL] = {.GPIOx = GPIOB, {.PIN_NO = IO_PIN_8, .PIN_MODE = IO_MODE_OUTPUT, .PIN_OPTYPE = IO_OPTYPE_OD, .PIN_RESISTANCE = IO_RES_NOPUPD, .PIN_SPEED = IO_SPEED_MEDIUM, .PIN_OUT = IO_OUT_HIGH}},
	[IO_I2C1_SDA] = {.GPIOx = GPIOB, {.PIN_NO = IO_PIN_7, .PIN_MODE = IO_MODE_OUTPUT, .PIN_OPTYPE = IO_OPTYPE_OD, .PIN_RESISTANCE = IO_RES_NOPUPD, .PIN_SPEED = IO_SPEED_MEDIUM, .PIN_OUT = IO_OUT_HIGH}},

    //SPI PINS
    [IO_SPI1_SCK] = {.GPIOx = GPIOB, {.PIN_NO = IO_PIN_3, .PIN_MODE = IO_MODE_ALT_FUN, .PIN_OPTYPE = IO_OPTYPE_PP, .PIN_RESISTANCE = IO_RES_NOPUPD, .PIN_SPEED = IO_SPEED_MEDIUM, .PIN_ALT_FUN_MODE = IO_ALT_FUN_MODE5, .PIN_OUT = IO_OUT_NULL}},
    [IO_SPI1_MISO] = {.GPIOx = GPIOB, {.PIN_NO = IO_PIN_4, .PIN_MODE = IO_MODE_ALT_FUN, .PIN_OPTYPE = IO_OPTYPE_PP, .PIN_RESISTANCE = IO_RES_NOPUPD, .PIN_SPEED = IO_SPEED_MEDIUM, .PIN_ALT_FUN_MODE = IO_ALT_FUN_MODE5, .PIN_OUT = IO_OUT_NULL}},
    [IO_SPI1_MOSI] = {.GPIOx = GPIOB, {.PIN_NO = IO_PIN_5, .PIN_MODE = IO_MODE_ALT_FUN, .PIN_OPTYPE = IO_OPTYPE_PP, .PIN_RESISTANCE = IO_RES_NOPUPD, .PIN_SPEED = IO_SPEED_MEDIUM, .PIN_ALT_FUN_MODE = IO_ALT_FUN_MODE5, .PIN_OUT = IO_OUT_NULL}},

	// IR EMITTERS
	[IO_IR_EMIT_UNIT1] = {.GPIOx = GPIOC, {.PIN_NO = IO_PIN_13, .PIN_MODE = IO_MODE_OUTPUT, .PIN_OPTYPE = IO_OPTYPE_PP, .PIN_RESISTANCE = IO_RES_NOPUPD, .PIN_SPEED = IO_SPEED_MEDIUM, .PIN_OUT = IO_OUT_LOW}},
	[IO_IR_EMIT_UNIT2] = {.GPIOx = GPIOC, {.PIN_NO = IO_PIN_14, .PIN_MODE = IO_MODE_OUTPUT, .PIN_OPTYPE = IO_OPTYPE_PP, .PIN_RESISTANCE = IO_RES_NOPUPD, .PIN_SPEED = IO_SPEED_MEDIUM, .PIN_OUT = IO_OUT_LOW}},
	[IO_IR_EMIT_UNIT3] = {.GPIOx = GPIOC, {.PIN_NO = IO_PIN_15, .PIN_MODE = IO_MODE_OUTPUT, .PIN_OPTYPE = IO_OPTYPE_PP, .PIN_RESISTANCE = IO_RES_NOPUPD, .PIN_SPEED = IO_SPEED_MEDIUM, .PIN_OUT = IO_OUT_LOW}},

	//UNUSED PINS PORTA
	[IO_UNUSED_0] = UNUSED_IO_CONFG(GPIOA, IO_PIN_0), [IO_UNUSED_1] = UNUSED_IO_CONFG(GPIOA, IO_PIN_1), [IO_UNUSED_4] = UNUSED_IO_CONFG(GPIOA, IO_PIN_4), 
	[IO_UNUSED_6] = UNUSED_IO_CONFG(GPIOA, IO_PIN_6), [IO_UNUSED_7] = UNUSED_IO_CONFG(GPIOA, IO_PIN_7), [IO_UNUSED_8] = UNUSED_IO_CONFG(GPIOA, IO_PIN_8), [IO_UNUSED_9] = UNUSED_IO_CONFG(GPIOA, IO_PIN_9), 
	[IO_UNUSED_10] = UNUSED_IO_CONFG(GPIOA, IO_PIN_10), [IO_UNUSED_11] = UNUSED_IO_CONFG(GPIOA, IO_PIN_11), [IO_UNUSED_12] = UNUSED_IO_CONFG(GPIOA, IO_PIN_12), [IO_UNUSED_13] = UNUSED_IO_CONFG(GPIOA, IO_PIN_13),
	[IO_UNUSED_14] = UNUSED_IO_CONFG(GPIOA, IO_PIN_14), [IO_UNUSED_15] = UNUSED_IO_CONFG(GPIOA, IO_PIN_15),

	//UNUSED PINS PORTB
	[IO_UNUSED_16] = UNUSED_IO_CONFG(GPIOB, ), [IO_UNUSED_17] = UNUSED_IO_CONFG(GPIOB, IO_PIN_3), [IO_UNUSED_18] = UNUSED_IO_CONFG(GPIOB, IO_PIN_4),
	[IO_UNUSED_25] = UNUSED_IO_CONFG(GPIOB, IO_PIN_11), [IO_UNUSED_26] = UNUSED_IO_CONFG(GPIOB, IO_PIN_12), [IO_UNUSED_27] = UNUSED_IO_CONFG(GPIOB, IO_PIN_13), 
	[IO_UNUSED_28] = UNUSED_IO_CONFG(GPIOB, IO_PIN_14), [IO_UNUSED_29] = UNUSED_IO_CONFG(GPIOB, IO_PIN_15),

	//UNUSED PINS PORTC
	[IO_UNUSED_30] = UNUSED_IO_CONFG(GPIOC, IO_PIN_0), [IO_UNUSED_31] = UNUSED_IO_CONFG(GPIOC, IO_PIN_1), [IO_UNUSED_34] = UNUSED_IO_CONFG(GPIOC, IO_PIN_4), [IO_UNUSED_35] = UNUSED_IO_CONFG(GPIOC, IO_PIN_5), 
	[IO_UNUSED_37] = UNUSED_IO_CONFG(GPIOC, IO_PIN_7), [IO_UNUSED_39] = UNUSED_IO_CONFG(GPIOC, IO_PIN_9), 
	[IO_UNUSED_41] = UNUSED_IO_CONFG(GPIOC, IO_PIN_11), [IO_UNUSED_42] = UNUSED_IO_CONFG(GPIOC, IO_PIN_12), 
	[IO_UNUSED_44] = UNUSED_IO_CONFG(GPIOC, IO_PIN_14)
};

/********************************************
*                 IO APIs
*********************************************/

/**
 * @brief Initialize all GPIO pins wiht the configurations in io_confgs array
 */
void IO_Init(void)
{
	for (generic_io_e io = (generic_io_e)IO_A0; io < IO_PIN_COUNT; io++)
		if (!compare_io_unused(&io_confgs[io]))
			IO_Config(&io_confgs[io]);
	return;
}

void io_set_out(io_e io, io_output_e value)
{
	if (value == IO_OUT_HIGH)
		io_confgs[io].GPIOx->BSRR |= (IO_OUT_HIGH << io_confgs[io].IO_Confg.PIN_NO);
	else if (value == IO_OUT_LOW)
		io_confgs[io].GPIOx->BSRR |= (IO_OUT_HIGH << (io_confgs[io].IO_Confg.PIN_NO + IO_BSRR_BR_OFFSET));
}

void io_get_current_config(io_e io, io_config_t *io_config)
{
	GPIO_TypeDef *gpiox = get_port_typedef(io);
	const uint8_t pin_idx = io_pin_idx(io);

	uint8_t io_config_check = 0;
	io_config->PIN_NO = pin_idx;

	// Retrieve configured value in GPIO mode register (MODER)
	io_config_check = ((gpiox->MODER & (0x3U << (pin_idx * IO_BITS_PER_PIN_MODE))) >> (pin_idx * IO_BITS_PER_PIN_MODE));
	io_config->PIN_MODE = io_config_check == 0 ? IO_MODE_INPUT : io_config_check == 1 ? IO_MODE_OUTPUT : io_config_check == 2 ? IO_MODE_ALT_FUN : IO_MODE_ANALOG;

	// Retrieve configured value in GPIO output typer register (OTYPER)
	io_config_check = (gpiox->OTYPER & (0x1 << pin_idx));
	io_config->PIN_OPTYPE = io_config_check == 0 ? IO_OPTYPE_PP : IO_OPTYPE_OD;

	// Retrieve configured value in GPIO pull-up/down register (PUPDR)
	io_config_check = (gpiox->PUPDR & (0x3 << (pin_idx * IO_BITS_PER_PIN_PUPDR)));
	io_config->PIN_RESISTANCE = io_config_check == 0 ? IO_RES_NOPUPD : io_config_check == 1 ? IO_RES_PU : IO_RES_PD;

	// Retrieve set value in GPIO output data register (ODR)
	io_config_check = (gpiox->ODR & (0x1 << pin_idx));
	io_config->PIN_OUT = io_config_check == 0 ? IO_OUT_LOW : IO_OUT_HIGH;

	io_config->PIN_SPEED = IO_SPEED_FAST;
}

bool io_config_compare(const io_config_t *config_1, const io_config_t *config_2)
{
	return (config_1->PIN_NO == config_2->PIN_NO) && (config_1->PIN_MODE == config_2->PIN_MODE) && (config_1->PIN_OPTYPE == config_2->PIN_OPTYPE)
			&& (config_1->PIN_RESISTANCE == config_2->PIN_RESISTANCE) && (config_1->PIN_SPEED == config_2->PIN_SPEED)
			&& (config_1->PIN_OUT == config_2->PIN_OUT);
}

/********************************************
*                 IO INTERRUPT APIs
*********************************************/

void io_irq_enable_interrupt(io_e io)
{
	const uint8_t pin_idx = io_pin_idx(io);
	const exti_irqn_e EXTI_IRQn = io_get_irq_no(pin_idx);
	if (EXTI_IRQn < 32)
	{
		*NVIC_ISER0 |= ( ENABLE << EXTI_IRQn );
	}
	else if (EXTI_IRQn < 64)
	{
		*NVIC_ISER1 |= ( ENABLE << ( EXTI_IRQn % 32 ));
	}
	else if (EXTI_IRQn < 96)
	{
		*NVIC_ISER2 |= ( ENABLE << ( EXTI_IRQn % 64 ));
	}
}

void io_irq_disable_interrupt(uint16_t EXTI_IRQn)
{
	if (EXTI_IRQn < 32)
	{
		*NVIC_ICER0 |= ( ENABLE << EXTI_IRQn );
	}
	else if (EXTI_IRQn < 64)
	{
		*NVIC_ICER1 |= ( ENABLE << ( EXTI_IRQn % 32 ));
	}
	else if (EXTI_IRQn < 96)
	{
		*NVIC_ICER2 |= ( ENABLE << ( EXTI_IRQn % 64 ));
	}
}

void io_set_interrupt_prio(uint16_t EXTI_IRQn, uint8_t IRQ_PR)
{
	uint8_t IPRx = EXTI_IRQn / 4;
	uint8_t IPRx_section = EXTI_IRQn % 4;

	uint8_t shift_amount = ((NVIC_IPRn_OFFSET_MULTIPLIER * IPRx_section) + (TOTAL_NVIC_PR_BITS - NO_PR_BITS_EN));
	*(NVIC_PR_BASE_ADDR + IPRx) |= (IRQ_PR << (shift_amount));
}

void io_configure_interrupt(io_e io, isr_trigger_e trigger, isr_function isr)
{
	GPIO_TypeDef *gpiox = get_port_typedef(io);
	const uint8_t pin_idx = io_pin_idx(io);
	/* ENABLE SYSCFG PERIPHERAL IN RCC */
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

	/* SET SYSCFG CONFIG REG TO APPROPRIATE EXTI LINE */
	IO_SetEXTILine(pin_idx, gpiox);

	/* SET INTERRUPT TRIGGER AND REGISTER ISR IN ARRAY */
	EXTI->IMR |= (DISABLE_IMR_MASK << pin_idx);
	IO_SetInterruptTrigger(pin_idx, trigger);
	IO_RegisterISR(pin_idx, io_get_port(gpiox), isr);
}

/********************************************
*                 IO EXTI IRQ Handlers
*********************************************/

void EXTI0_IRQHandler(void)
{
	// 1. Get port configuration from SYSCFG_EXTICR[x]
	uint8_t exti_port = retrieve_syscfg_exti_port(EXTI0);
	// 2. Execute ISR from isr_functions array
	isr_functions[exti_port][EXTI0]();
	// 3. Clear Pending Reg bit
	EXTI->PR |= EXTI_PR_PR0_Msk;
}

void EXTI1_IRQHandler(void)
{
	// 1. Get port configuration from SYSCFG_EXTICR[x]
	uint8_t exti_port = retrieve_syscfg_exti_port(EXTI1);
	// 2. Execute ISR from isr_functions array
	isr_functions[exti_port][EXTI1]();
	// 3. Clear Pending Reg bit
	EXTI->PR |= EXTI_PR_PR1_Msk;
}

void EXTI2_IRQHandler(void)
{
	// 1. Get port configuration from SYSCFG_EXTICR[x]
	uint8_t exti_port = retrieve_syscfg_exti_port(EXTI2);
	// 2. Execute ISR from isr_functions array
	isr_functions[exti_port][EXTI2]();
	// 3. Clear Pending Reg bit
	EXTI->PR |= EXTI_PR_PR2_Msk;
}

void EXTI3_IRQHandler(void)
{
	// 1. Get port configuration from SYSCFG_EXTICR[x]
	uint8_t exti_port = retrieve_syscfg_exti_port(EXTI3);
	// 2. Execute ISR from isr_functions array
	isr_functions[exti_port][EXTI3]();
	// 3. Clear Pending Reg bit
	EXTI->PR |= EXTI_PR_PR3_Msk;	
}

void EXTI4_IRQHandler(void)
{
	// 1. Get port configuration from SYSCFG_EXTICR[x]
	uint8_t exti_port = retrieve_syscfg_exti_port(EXTI4);
	// 2. Execute ISR from isr_functions array
	isr_functions[exti_port][EXTI4]();
	// 3. Clear Pending Reg bit
	EXTI->PR |= EXTI_PR_PR4_Msk;
}

void EXTI9_5_IRQHandler(void)
{
	// // 1. Get the pin that triggered Interrupt
	exti_line_e exti_line_trigger = get_exti_line();
	uint8_t exti_port = retrieve_syscfg_exti_port(exti_line_trigger);
	// // 2. Execute ISR from isr_functions array
	if (isr_functions[exti_port][exti_line_trigger] != NULL)
		isr_functions[exti_port][exti_line_trigger]();
	// 3. Clear Pending Reg bit
	EXTI->PR |= (0x1U << exti_line_trigger);
}

void EXTI15_10_IRQHandler(void)
{
	// // 1. Get the pin that triggered Interrupt
	exti_line_e exti_line_trigger = get_exti_line();
	uint8_t exti_port = retrieve_syscfg_exti_port(exti_line_trigger);
	// // 2. Execute ISR from isr_functions array
	if (isr_functions[exti_port][exti_line_trigger] != NULL)
		isr_functions[exti_port][exti_line_trigger]();
	// 3. Clear Pending Reg bit
	EXTI->PR |= (0x1U << exti_line_trigger);
}

/*****************************************************
 *                  STATIC DECLARATIONS
 *****************************************************/

/**************************
 * STATIC IO CONFIG APIs
 *************************/

/* Set Pin confgs APIs */
static void IO_SetResistance(GPIO_TypeDef *gpiox, const io_resistance_e res, const uint8_t io)
{
    switch(res)
    {
      case IO_RES_NOPUPD:
        gpiox->PUPDR &= ~( 0x3U << (io * 2) );
        break;
      case IO_RES_PU:
        gpiox->PUPDR &= ~( 0x3U << (io * 2) );
        gpiox->PUPDR |= ( IO_RES_PU << (io * 2) );
        break;
      case IO_RES_PD:
        gpiox->PUPDR &= ~( 0x3U << (io * 2) );
        gpiox->PUPDR |= ( IO_RES_PD << (io * 2) );
        break;
    }
}

static void IO_SetMode(GPIO_TypeDef *gpiox, const io_mode_e mode, const uint8_t io)
{
	switch (mode)
	{
		case IO_MODE_INPUT:
			gpiox->MODER &= ~( 0x3U << (2 * io) );
			break;
		case IO_MODE_OUTPUT:
			gpiox->MODER &= ~( 0x3U << (2 * io) );
			gpiox->MODER |= ( IO_MODE_OUTPUT << (2 * io) );
			break;
		case IO_MODE_ALT_FUN:
			gpiox->MODER &= ~( 0x3U << (2 * io) );
			gpiox->MODER |= ( IO_MODE_ALT_FUN << (2 * io) );
			break;
		case IO_MODE_ANALOG:
			gpiox->MODER &= ~( 0x3U << (2 * io) );
			gpiox->MODER |= ( IO_MODE_ANALOG << (2 * io) );
			break;
	}
}
static void IO_SetSpeed(GPIO_TypeDef *gpiox, const io_speed_e speed, const uint8_t io)
{
	switch(speed)
	{
		case IO_SPEED_LOW:
			gpiox->OSPEEDR &= ~( 0x3U << (2 * io) );
			break;
		case IO_SPEED_MEDIUM:
			gpiox->OSPEEDR &= ~( 0x3U << (2 * io) );
			gpiox->OSPEEDR |= ( IO_SPEED_MEDIUM << (2 * io) );
			break;
		case IO_SPEED_FAST:
			gpiox->OSPEEDR &= ~( 0x3U << (2 * io) );
			gpiox->OSPEEDR |= ( 0x2U << (2 * io) );
			break;
		case IO_SPEED_HIGH:
			gpiox->OSPEEDR &= ~( 0x3U << (2 * io) );
			gpiox->OSPEEDR |= ( IO_SPEED_HIGH << (2 * io) );
			break;
	}
}
static void IO_SetOPType(GPIO_TypeDef *gpiox, const io_optype_e optype, const uint8_t io)
{
	switch(optype)
	{
		case IO_OPTYPE_PP:
			gpiox->OTYPER &= ~(0x1U << io);
			break;
		case IO_OPTYPE_OD:
			gpiox->OTYPER &= ~(0x1U << io);
			gpiox->OTYPER |= (0x1U << io);
			break;
	}
}
static void IO_SetAltFunMode(GPIO_TypeDef *gpiox, const io_mode_alt_fun alt_fun_mode, const uint8_t io)
{
	// Retrieve index of Alt Fun register to be used
	uint8_t AFR_Index = io / NO_OF_PINS_PER_REG;
	uint8_t io_bit = io % NO_OF_PINS_PER_REG;
	if (alt_fun_mode > IO_ALT_FUN_MODE0)
		gpiox->AFR[AFR_Index] |= (alt_fun_mode << (io_bit * NO_OF_AFR_BITS));
	else
		gpiox->AFR[AFR_Index] &= ~(0xFU << (io_bit * NO_OF_AFR_BITS));
}

static void  io_peripheral_control(GPIO_TypeDef *gpiox, uint8_t EnOrDi)
{
	if (EnOrDi == ENABLE)
	{
		if (gpiox == GPIOA && !(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN_Msk))
			RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
		if (gpiox == GPIOB && !(RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN_Msk))
			RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
		if (gpiox == GPIOC && !(RCC->AHB1ENR & RCC_AHB1ENR_GPIOCEN_Msk))
			RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	}
	else
	{
		if (gpiox == GPIOA)
			RCC->AHB1ENR &= ~(RCC_AHB1ENR_GPIOAEN);
		if (gpiox == GPIOB)
			RCC->AHB1ENR &= ~(RCC_AHB1ENR_GPIOBEN);
		if (gpiox == GPIOC)
			RCC->AHB1ENR &= ~(RCC_AHB1ENR_GPIOCEN);
	}
}

static void IO_Config(io_handle_t *io_handle)
{
	io_peripheral_control(io_handle->GPIOx, ENABLE);
    IO_SetMode(io_handle->GPIOx, io_handle->IO_Confg.PIN_MODE, io_handle->IO_Confg.PIN_NO);
	if (io_handle->IO_Confg.PIN_MODE != IO_MODE_INPUT) {
		IO_SetOPType(io_handle->GPIOx, io_handle->IO_Confg.PIN_OPTYPE, io_handle->IO_Confg.PIN_NO);
    	IO_SetResistance(io_handle->GPIOx, io_handle->IO_Confg.PIN_RESISTANCE, io_handle->IO_Confg.PIN_NO);
	}
	IO_SetSpeed(io_handle->GPIOx, io_handle->IO_Confg.PIN_SPEED, io_handle->IO_Confg.PIN_NO);
	if (io_handle->IO_Confg.PIN_MODE == IO_MODE_ALT_FUN)
		IO_SetAltFunMode(io_handle->GPIOx, io_handle->IO_Confg.PIN_ALT_FUN_MODE, io_handle->IO_Confg.PIN_NO);
	else
		IO_SetAltFunMode(io_handle->GPIOx, IO_ALT_FUN_MODE0, io_handle->IO_Confg.PIN_NO);

	if (io_handle->IO_Confg.PIN_OUT != IO_OUT_NULL) {
		if (io_handle->IO_Confg.PIN_OUT == IO_OUT_LOW)
			io_handle->GPIOx->ODR &= ~(0x1 << io_handle->IO_Confg.PIN_NO);
		else
			io_handle->GPIOx->ODR |= (0x1 << io_handle->IO_Confg.PIN_NO);
	}
}

/******************************
 * STATIC IO INFO DECLARATIONS
 ******************************/
static inline io_port_e io_get_port(GPIO_TypeDef *GPIOx)
{
	if (GPIOx == GPIOA)
		return IO_PORTA;
	if (GPIOx == GPIOB)
		return IO_PORTB;
	if (GPIOx == GPIOC)
		return IO_PORTC;
	else
		return IO_PORTD;
}

static bool compare_io_unused(const io_handle_t *io_h)
{
	return (io_h->GPIOx == NULL) && (io_h->IO_Confg.PIN_NO == 16);
}

/**
 * @brief Returns the port to which the provided io pin belongs
 * 
 * We represent each pin with 6 bits, lower 4 bits correspond to the pin 
 * number and the 5th bit corresponds to the port number
 */
static uint8_t io_port(io_e io)
{
	return (io & IO_PORT_MSK) >> IO_PORT_OFFSET;
}

/**
 * @brief Returns the GPIO port typedef associated with specified io pin 
 */
static GPIO_TypeDef* get_port_typedef(io_e io)
{
	GPIO_TypeDef* gpiox = NULL;
	const uint8_t port_num = io_port(io);
	switch(port_num)
	{
		case (IO_PORTA):
			gpiox = GPIOA;
			break;
		case IO_PORTB:
			gpiox = GPIOB;
			break;
		case IO_PORTC:
			gpiox = GPIOC;
			break;
	}
	return gpiox;
}

/**
 * @brief returns the pin number associated with the specified io pin
 */
static uint8_t io_pin_idx(io_e io)
{
	return (io & IO_PIN_MSK);
}

/**
 * @brief returns the bit position of the specified io pin
 */
static uint8_t io_pin_bit(io_e io)
{
	return 1 << io_pin_idx(io);
}


/*****************************
 * STATIC IO IT DECLARATIONS
 *****************************/

static void IO_SetInterruptTrigger(uint8_t io, isr_trigger_e trigger)
{
	switch(trigger)
	{
		case IO_INTERRPT_RTFT:
			EXTI->RTSR |= ( 0x1U << io );
			EXTI->FTSR |= ( 0x1U << io );
			break;
		case IO_INTERRUPT_RT:
			EXTI->RTSR |= ( 0x1U << io );
			EXTI->FTSR &= ~( 0x1U << io );
			break;
		case IO_INTERRUPT_FT:
			EXTI->FTSR |= ( 0x1U << io );
			EXTI->RTSR &= ~( 0x1U << io );
			break;
	}
}

static void IO_RegisterISR(uint8_t io, io_port_e port, isr_function isr)
{
	if (isr_functions[port][io] != NULL)
		return;
	isr_functions[port][io] = isr;
}

static void IO_SetEXTILine(uint8_t io, GPIO_TypeDef* gpiox)
{
	const uint8_t EXTI_CRx = io / NO_OF_PINS_IN_CR;
	const uint8_t EXTIx_CR_IDX = ( io % NO_OF_PINS_IN_CR ) * CR_OFFSET_FACTOR;
	if (gpiox == GPIOA)
	{
		SYSCFG->EXTICR[EXTI_CRx] &= ~(SYSCFG_IO_PORTA << EXTIx_CR_IDX);
	}
	else if (gpiox == GPIOB)
	{
		SYSCFG->EXTICR[EXTI_CRx] |= (SYSCFG_IO_PORTB << EXTIx_CR_IDX);
	}
	else if (gpiox == GPIOC)
	{
		SYSCFG->EXTICR[EXTI_CRx] |= (SYSCFG_IO_PORTC << EXTIx_CR_IDX);
	}
	else if (gpiox == GPIOD)
	{
		SYSCFG->EXTICR[EXTI_CRx] |= (SYSCFG_IO_PORTD << EXTIx_CR_IDX);
	}
}

static exti_irqn_e io_get_irq_no(const uint8_t pin_idx)
{
	exti_irqn_e irq_no = 0;
	if (pin_idx == IO_PIN_0)
		irq_no = EXTI0_IRQ_NO;
	else if (pin_idx == IO_PIN_1)
		irq_no = EXTI1_IRQ_NO;
	else if (pin_idx == IO_PIN_2)
		irq_no = EXTI2_IRQ_NO;
	else if (pin_idx == IO_PIN_3)
		irq_no = EXTI3_IRQ_NO;
	else if (pin_idx == IO_PIN_4)
		irq_no = EXTI4_IRQ_NO;
	else if (pin_idx < IO_PIN_10)
		irq_no = EXTI9_5_IRQ_NO;
	else
		irq_no = EXTI15_10_IRQ_NO;

	return irq_no;
}

static inline uint8_t retrieve_syscfg_exti_port(exti_line_e EXTIx)
{
	uint8_t EXTICRx = EXTIx / 4; // 13 / 4 = 3
	uint8_t EXTI_Shift = ((EXTIx % 4) * 4); // 13 % 4 = 1; 1 * 4 = 4
	uint8_t EXTI_Msk = 0x0F;

	uint8_t current_port = (SYSCFG->EXTICR[EXTICRx] >> EXTI_Shift) & EXTI_Msk;
	return current_port;
}

static inline uint8_t get_exti_line(void)
{
	for (uint8_t pin_trigger = EXTI_PR_PR5_Pos; pin_trigger < EXTI_PR_PR16_Pos; ++pin_trigger)
	{
		uint8_t check_pr_set = (EXTI->PR >> pin_trigger) & 0x1;
		if (check_pr_set == PRx_SET)
		{
			return pin_trigger;
		} 
	}
	return NO_PRx_SET;
}
