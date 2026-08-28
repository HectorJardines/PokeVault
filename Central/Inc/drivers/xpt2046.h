/*
 * Touch Screen Driver Library for HR2046 Controller
 * Based on :
 * https://github.com/PaulStoffregen/XPT2046_Touchscreen
 * Thanks for the help
 * To use it set the peirq pin with name TC_PEN
 *
 * All the code was based on the next driver:
 * https://ldm-systems.ru/f/doc/catalog/HY-TFT-2,8/XPT2046.pdf
 * This assumes the next structure of control byte:
 * [S | A2 | A1 | A0 | MODE | SR/DF | PD1 | PD0 ]
 * */

#ifndef XPT2046_H
#define XPT2046_H

#include "./io.h"
#include "./spi.h"

#define XPT2046_START_BIT  	(0x01U << 7)
#define XPT2046_A2_ENABLE  	(0x01U << 6)
#define XPT2046_A1_ENABLE  	(0x01U << 5)
#define XPT2046_A0_ENABLE  	(0x01U << 4)
#define XPT2046_MODE_8BIT  	(0x01U << 3)
#define XPT2046_MODE_SER	(0x01U << 2)
#define XPT2046_PD1_ENABLE	(0x01U << 1)
#define XPT2046_PD0_ENABLE	(0x01U << 0)


#define NUM_SAMPLES				5
#define Z_THRESHOLD_12BIT		40
#define Z_THRESHOLD_08BIT		7

#define XPT2046_X_OFFSET 		10
#define XPT2046_Y_OFFSET		0

#define XPT2046_WIDTH			240
#define XPT2046_HEIGHT			320
/* This pins must be defined as in your project */

#define XPT2046_CS_Port 		TOUCH_CS_GPIO_Port
#define XPT2046_CS_Pin			TOUCH_CS_Pin
#define XPT2046_PENIRQ_Port 	TC_PEN_GPIO_Port
#define XPT2046_PENIRQ_Pin 		TC_PEN_Pin

typedef enum pressedModes {
	XPT2046_NOT_PRESSED = 0,
	XPT2046_PRESSED = 1
}XPT2046_PressedModes;

typedef enum powerModes{
	XPT2046_POWER_DOWN			 	= 0x00,
	XPT2046_REFERENCE_OFF_ADC_ON 	= XPT2046_PD0_ENABLE,
	XPT2046_REFERENCE_ON_ADC_OFF 	= XPT2046_PD1_ENABLE,
	XPT2046_DEVICE_ALWAYS_ON 		= XPT2046_PD1_ENABLE | XPT2046_PD0_ENABLE
}XPT2046_PowerModes;

typedef enum referenceModes{
	XPT2046_DFR_MODE 				= 0,
	XPT2046_SER_MODE 				= XPT2046_MODE_SER
}XPT2046_ReferenceModes;

typedef enum channelModes{
	XPT2046_SER_TEMP0 			= 0,
	XPT2046_SER_Y				= XPT2046_A0_ENABLE,
	XPT2046_SER_VBAT 			= XPT2046_A1_ENABLE,
	XPT2046_SER_Z1	 			= XPT2046_A1_ENABLE | XPT2046_A0_ENABLE,
	XPT2046_SER_Z2				= XPT2046_A2_ENABLE,
	XPT2046_SER_X				= XPT2046_A2_ENABLE |XPT2046_A0_ENABLE ,
	XPT2046_SER_AUXIN 			= XPT2046_A2_ENABLE|XPT2046_A1_ENABLE,
	XPT2046_SER_TEMP1 			= XPT2046_A2_ENABLE|XPT2046_A1_ENABLE|XPT2046_A0_ENABLE,
	XPT2046_DFR_Y		 		= XPT2046_A0_ENABLE,
	XPT2046_DFR_Z1		 		= XPT2046_A1_ENABLE | XPT2046_A0_ENABLE,
	XPT2046_DFR_Z2		 		= XPT2046_A2_ENABLE,
	XPT2046_DFR_X		 		= XPT2046_A2_ENABLE |XPT2046_A0_ENABLE
}XPT2046_ChannelModes;

typedef enum bitModes {
	XPT2046_12BIT_MODE 			= 0,
	XPT2046_8BIT_MODE 			= XPT2046_MODE_8BIT
}XPT2046_BitModes;

typedef enum startBits {
	XPT2046_NONE			= 0x00,
	XPT2046_START 			= XPT2046_START_BIT
}XPT2046_StartModes;


typedef struct {
	uint8_t startBit;
	uint8_t channel;
	uint8_t  bitMode;
	uint8_t reference;
	uint8_t powerMode;
}TouchScreen_ControlByte;

/* This are the same values as the ili9341 library*/
typedef enum
{
	XPT2046_ORIENTATION_PORTRAIT 			= 0,
	XPT2046_ORIENTATION_LANDSCAPE 			= 1,
	XPT2046_ORIENTATION_PORTRAIT_MIRROR 	= 2,
	XPT2046_ORIENTATION_LANDSCAPE_MIRROR 	= 3
}TouchScreen_OrientationTypeDef;

/*
 * Touch Screen Coordinates Point
 * */
typedef struct
{
	uint16_t x;
	uint16_t y;
	uint16_t z1;
	uint16_t z2;
}TouchScreen_CoordinatesRaw;

typedef struct
{
	uint16_t x;
	uint16_t y;
	uint16_t z;
}TouchScreen_Coordinates;

typedef struct
{
	uint16_t width;
	uint16_t height;
} TouchScreen_Size;

typedef void(*xpt_transmit)(uint8_t *data, uint16_t len);
typedef void(*xpt_receive)(uint8_t *data, uint16_t len);
typedef void(*xpt_select)(void);
typedef void(*xpt_deselect)(void);



void xpt2046_init(void);
void xpt2046_set_size(uint16_t w, uint16_t h);
void xpt2046_bit_mode(uint8_t b);
void xpt2046_update(void);
void xpt2046_read_position(int32_t* x, int32_t* y);
/**
 * @brief Resets the XPT2046 IRQ trigger in case of power loss during touch read
 * 
 * 
 */
int xpt2046_reset_state(void);
#endif
