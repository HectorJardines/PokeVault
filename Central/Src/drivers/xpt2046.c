#include "../../Inc/drivers/xpt2046.h"
#include "../../Inc/common/printf-stdarg.h"

/******************
 * MACROS
 *****************/
#define XPT_CS_HIGH()		(io_set_out(IO_SPI_CS_TOUCH, HIGH))
#define XPT_CS_LOW()		(io_set_out(IO_SPI_CS_TOUCH, LOW))
#define IS_LOW_PENIRQ()		(!io_get_out(IO_TOUCH_IT))
#define IS_HIGH_PENIRQ()	(io_get_out(IO_TOUCH_IT))

#define X_RAW_MIN	320
#define X_RAW_MAX	3670
#define Y_RAW_MIN	235
#define Y_RAW_MAX	3630

/*******************
 * STATIC DECLARATIONS
 *********************/
static TouchScreen_CoordinatesRaw ts_CoordinatesRaw;
static TouchScreen_Coordinates ts_Coordinates;
static TouchScreen_OrientationTypeDef ts_Orientation;
static TouchScreen_Size ts_Size;

static TouchScreen_ControlByte ts_ControlByte;
static uint16_t ts_Cs_Pin;
static uint16_t ts_Penirq_Pin;
static uint8_t command;


static int32_t avg_closest_two(uint32_t *samples);
static uint16_t xpt2046_zthreshold(void);
static uint8_t xpt2046_pressed(void);
static uint8_t xpt2046_interruptions_activated(void);
static void xpt2046_control_byte_update(void);
/*******************
 * PUBLIC APIs
 *******************/


void xpt2046_reg_spi_io_cb(xpt_transmit tx_cb, xpt_receive rx_cb);
void xpt2046_reg_spi_cs_cb(xpt_select select_cb, xpt_deselect deselect_cb);

/**
 * Init the moduel
 */
void xpt2046_init()
{
	ts_ControlByte.bitMode = XPT2046_12BIT_MODE;
	ts_ControlByte.powerMode = XPT2046_REFERENCE_OFF_ADC_ON;
	ts_ControlByte.reference = XPT2046_DFR_MODE;
	ts_ControlByte.channel = XPT2046_DFR_X; //by default
	ts_ControlByte.startBit = XPT2046_START;
	ts_Orientation = XPT2046_ORIENTATION_PORTRAIT_MIRROR;
	ts_Size.width = XPT2046_WIDTH;
	ts_Size.height = XPT2046_HEIGHT;

	command = ts_ControlByte.bitMode |  ts_ControlByte.powerMode |ts_ControlByte.reference |ts_ControlByte.channel |ts_ControlByte.startBit;
	XPT_CS_HIGH();
}


void xpt2046_orientation(TouchScreen_OrientationTypeDef orientation_)
{
	ts_Orientation = orientation_;
}


void xpt2046_bit_mode(uint8_t b)
{
	ts_ControlByte.bitMode = b;
}


/**
 * Set Size of Touch screen
 */

void xtp2046_set_size(uint16_t w, uint16_t h)
{
	ts_Size.width = w;
	ts_Size.height = h;
}


/**
 * Update X,Y,Z values of touchScreen
 */
void xpt2046_update(){
	

	if(xpt2046_interruptions_activated())
	{
		/* this means Interrupt pin was enabled*/
		if(!IS_LOW_PENIRQ())
		{
			return;
		}


	}
	
	uint32_t x_samples[NUM_SAMPLES], z1_samples[NUM_SAMPLES], y_samples[NUM_SAMPLES];
	if (spi_lock(DEV_TOUCH) == 1) {
		XPT_CS_LOW();
		
		ts_ControlByte.powerMode = XPT2046_REFERENCE_OFF_ADC_ON;
		for(int i = 0; i < NUM_SAMPLES; i++)
		{
			ts_ControlByte.channel = XPT2046_DFR_X;
			xpt2046_control_byte_update();

			uint8_t receiveByteX[2] = {0x00,0x00};

			spi_transmit(DEV_TOUCH, &command, sizeof(command));
			spi_receive(DEV_TOUCH, receiveByteX, sizeof(receiveByteX));

			printf("0x%x,0x%x\r\n", receiveByteX[1], receiveByteX[0]);
			if (i == NUM_SAMPLES - 1) {
				xpt2046_control_byte_update();
			}
			ts_ControlByte.channel = XPT2046_DFR_Y;
			xpt2046_control_byte_update();

			uint8_t receiveByteY[2] = {0x00,0x00};
			spi_transmit(DEV_TOUCH, &command, sizeof(command));
			spi_receive(DEV_TOUCH, receiveByteY, sizeof(receiveByteY));


			ts_ControlByte.channel = XPT2046_DFR_Z1;
			if (i == NUM_SAMPLES - 1) // need to set PDO LOW to ensure touch IRQ works on next send
				ts_ControlByte.powerMode = XPT2046_POWER_DOWN;
			xpt2046_control_byte_update();

			uint8_t receiveByteZ1[2] = {0x00,0x00};
			spi_transmit(DEV_TOUCH, &command, sizeof(command));
			spi_receive(DEV_TOUCH, receiveByteZ1, sizeof(receiveByteZ1));

			if(ts_ControlByte.bitMode == XPT2046_12BIT_MODE)
			{
				x_samples[i]  = 	((receiveByteX[0] & 0x7F)	<< 5) | (receiveByteX[1] 	>> 3);
				y_samples [i] = 	((receiveByteY[0] & 0x7F) 	<< 5) | (receiveByteY[1] 	>> 3);
				z1_samples[i] = 	((receiveByteZ1[0] & 0x7F) 	<< 5) | (receiveByteZ1[1] 	>> 3);
			}else
			{
				x_samples[i]  = 	(receiveByteX[1]);
				y_samples [i] = 	(receiveByteY[1]) ;
				z1_samples[i] = 	(receiveByteZ1[1]);
			}
		}
		XPT_CS_HIGH();
		spi_unlock(DEV_TOUCH);
	}
	else goto cleanup; // spi lock not obtained...


	ts_CoordinatesRaw.x = avg_closest_two(x_samples);
	ts_CoordinatesRaw.y = avg_closest_two(y_samples);
	ts_CoordinatesRaw.z1 = avg_closest_two(z1_samples);
	/* Consider (0,0) as the top left point of the screen */
	switch(ts_Orientation)
	{
	case XPT2046_ORIENTATION_PORTRAIT:
		ts_Coordinates.x = (ts_Size.width * (ts_CoordinatesRaw.x - X_RAW_MIN)) / (X_RAW_MAX - X_RAW_MIN);
		ts_Coordinates.y = (ts_Size.height * (ts_CoordinatesRaw.y - Y_RAW_MIN))/ (Y_RAW_MAX - Y_RAW_MIN);
		ts_Coordinates.z = ts_CoordinatesRaw.z1;
		break;
	case XPT2046_ORIENTATION_LANDSCAPE:
		// ts_Coordinates.x = (int32_t)(ts_CoordinatesRaw.y - Y_RAW_MIN) * ts_Size.width / (Y_RAW_MAX - Y_RAW_MIN);
		// ts_Coordinates.y = (int32_t)(ts_CoordinatesRaw.x - X_RAW_MIN) * ts_Size.height / (X_RAW_MAX - X_RAW_MIN);
		// // ts_Coordinates.x = (ts_Size.width * (max - ts_CoordinatesRaw.y))/max - XPT2046_Y_OFFSET;
		// ts_Coordinates.y = (ts_Size.height * (ts_CoordinatesRaw.x))/max - XPT2046_X_OFFSET;
		// ts_Coordinates.z = ts_CoordinatesRaw.z1;
		break;
	case XPT2046_ORIENTATION_PORTRAIT_MIRROR:
		ts_Coordinates.x = ts_Size.width - (ts_Size.width * (ts_CoordinatesRaw.x - X_RAW_MIN)) / (X_RAW_MAX - X_RAW_MIN);
		ts_Coordinates.y = (ts_Size.height * (ts_CoordinatesRaw.y - Y_RAW_MIN))/ (Y_RAW_MAX - Y_RAW_MIN);
		ts_Coordinates.z = ts_CoordinatesRaw.z1;
		break;
	case XPT2046_ORIENTATION_LANDSCAPE_MIRROR:
		// ts_Coordinates.x = (ts_Size.width *  (ts_CoordinatesRaw.y))/max - XPT2046_Y_OFFSET;
		// ts_Coordinates.y = (ts_Size.height * (max- ts_CoordinatesRaw.x))/max - XPT2046_X_OFFSET;
		// ts_Coordinates.z = ts_CoordinatesRaw.z1;
		break;
	default:
		break;
	}

cleanup:
}

/* by default return 0,0 if the screen isn't pressed*/
void xpt2046_read_position(int32_t* x, int32_t* y)
{
	xpt2046_update();
	if(xpt2046_pressed())
	{
		*x = ts_Coordinates.x;
		*y = ts_Coordinates.y;
	}else
	{
		*x = -1;
		*y = -1;
	}
}

/*******************
 * STATIC DEFINITIONS
 *********************/
static int32_t avg_closest_two(uint32_t *samples) {
	uint8_t idx_1 = 0, idx_2 = 0;
	int32_t smallest_diff = 0x7FFFFFFF;
	for (uint8_t i = 1; i < NUM_SAMPLES; ++i) {
		for (uint8_t j = 1; j < NUM_SAMPLES; ++j) {
			if (i == j) continue;
			if ((samples[i] > samples[j]) && samples[i] - samples[j] < smallest_diff) {
				smallest_diff = samples[i] - samples[j];
				idx_1 = i; idx_2 = j;
			}
			else if ((samples[j] > samples[i]) && samples[j] - samples[i] < smallest_diff) {
				smallest_diff = samples[j] - samples[i];
				idx_1 = i; idx_2 = j;
			}
		}
	}

	return ((samples[idx_1] + samples[idx_2]) >> 1);
}



static uint16_t xpt2046_zthreshold(void)
{
	if(ts_ControlByte.bitMode == XPT2046_8BIT_MODE)
		return Z_THRESHOLD_08BIT;
	else
		return Z_THRESHOLD_12BIT;
}



static uint8_t xpt2046_pressed(void)
{
	uint16_t zt =  xpt2046_zthreshold();
	if(xpt2046_interruptions_activated())
		return IS_LOW_PENIRQ() && (ts_CoordinatesRaw.z1 > zt );
	else
		return (ts_CoordinatesRaw.z1 > zt );
}



static uint8_t xpt2046_interruptions_activated(void)
{
	return (ts_ControlByte.powerMode == XPT2046_POWER_DOWN || ts_ControlByte.powerMode == XPT2046_REFERENCE_ON_ADC_OFF);
}



/**
 * Generate control byte
 */
static void xpt2046_control_byte_update(void)
{
	command =  ts_ControlByte.startBit |  ts_ControlByte.channel | ts_ControlByte.bitMode | ts_ControlByte.reference | ts_ControlByte.powerMode  ;
}


