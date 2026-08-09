#include "../../Inc/drivers/xpt2046.h"

/******************
 * MACROS
 *****************/
#define XPT_CS_HIGH()		(io_set_out(IO_SPI_CS_DISP, HIGH))
#define XPT_CS_LOW()		(io_set_out(IO_SPI_CS_DISP, LOW))
#define IS_LOW_PENIRQ()		(!io_get_out(IO_TOUCH_IT))
#define IS_HIGH_PENIRQ()	(io_get_out(IO_TOUCH_IT))


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
/*******************
 * PUBLIC APIs
 *******************/


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
	ts_Orientation = XPT2046_ORIENTATION_LANDSCAPE;
	ts_Size.width = XPT2046_WIDTH;
	ts_Size.height = XPT2046_HEIGHT;

	command = ts_ControlByte.bitMode |  ts_ControlByte.powerMode |ts_ControlByte.reference |ts_ControlByte.channel |ts_ControlByte.startBit;
	// xpt2046_unselect();
	XPT_CS_LOW();
}


void xpt2046_orientation(TouchScreen_OrientationTypeDef orientation_)
{
	ts_Orientation = orientation_;
}

void xpt2046_bit_mode(uint8_t b)
{
	ts_ControlByte.bitMode = b;
}
void xpt2046_set_size(uint16_t w, uint16_t h)
{
	ts_Size.width = w;
	ts_Size.height = h;
}


uint16_t xpt2046_zthreshold()
{
	if(ts_ControlByte.bitMode == XPT2046_8BIT_MODE)
	{
		return Z_THRESHOLD_08BIT;
	}else
	{
		return Z_THRESHOLD_12BIT;
	}
}



uint8_t xpt2046_pressed()
{
	uint16_t zt =  xpt2046_zthreshold();
	if(xpt2046_interruptions_activated())
		return IS_LOW_PENIRQ() && (ts_CoordinatesRaw.z1 > zt );
	else
		return (ts_CoordinatesRaw.z1 > zt );
}



uint8_t xpt2046_interruptions_activated()
{
	return (ts_ControlByte.powerMode == XPT2046_POWER_DOWN || ts_ControlByte.powerMode == XPT2046_REFERENCE_ON_ADC_OFF);
}



/**
 * Generate control byte
 */
void xpt2046_control_byte_update()
{
	command =  ts_ControlByte.startBit |  ts_ControlByte.channel | ts_ControlByte.bitMode | ts_ControlByte.reference | ts_ControlByte.powerMode  ;
}



/**
 * Return max value depending on bit mode.
 */
uint16_t xpt2046_max_measurement(){
	if(ts_ControlByte.bitMode == XPT2046_8BIT_MODE)
	{
		return 120; //This must be calibrated
	}else
	{
		return 1900; // this value must be calibrated on your on touch screen
	}
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
	
	uint8_t zeros[2] = { 0x00,0x00};
	uint32_t x_avg = 0, y_avg = 0,z1_avg=0,z2_avg=0;

	if (spi_lock(DEV_TOUCH) == 1) {
		XPT_CS_LOW();
		for(int i = 0; i < AVERAGE_POINTS; i++)
		{
			ts_ControlByte.channel = XPT2046_DFR_X;
			xpt2046_control_byte_update();

			uint8_t receiveByteX[2] = {0x00,0x00};

			spi_transmit(DEV_TOUCH, &command, sizeof(command));
			spi_receive(DEV_TOUCH, receiveByteX, sizeof(receiveByteX));


			ts_ControlByte.channel = XPT2046_DFR_Y;
			xpt2046_control_byte_update();

			uint8_t receiveByteY[2] = {0x00,0x00};
			spi_transmit(DEV_TOUCH, &command, sizeof(command));
			spi_receive(DEV_TOUCH, receiveByteY, sizeof(receiveByteY));


			ts_ControlByte.channel = XPT2046_DFR_Z1;
			xpt2046_control_byte_update();
			uint8_t receiveByteZ1[2] = {0x00,0x00};

			spi_transmit(DEV_TOUCH, &command, sizeof(command));
			spi_receive(DEV_TOUCH, receiveByteZ1, sizeof(receiveByteZ1));


			ts_ControlByte.channel = XPT2046_DFR_Z2;
			xpt2046_control_byte_update();
			uint8_t receiveByteZ2[2] = {0x00,0x00};

			spi_transmit(DEV_TOUCH, &command, sizeof(command));
			spi_receive(DEV_TOUCH, receiveByteZ2, sizeof(receiveByteZ2));

			if(ts_ControlByte.bitMode == XPT2046_12BIT_MODE)
			{
				x_avg+= 	(receiveByteX[1] 	<< 4) | (receiveByteX[0] 	>> 4);
				y_avg+= 	(receiveByteY[1] 	<< 4) | (receiveByteY[0] 	>> 4);
				z1_avg+= 	(receiveByteZ1[1] 	<< 4) | (receiveByteZ1[0] 	>> 4);
				z2_avg+= 	(receiveByteZ2[1] 	<< 4) | (receiveByteZ2[0] 	>> 4);
			}else
			{
				x_avg+= 	(receiveByteX[1]);
				y_avg+= 	(receiveByteY[1]) ;
				z1_avg+= 	(receiveByteZ1[1]);
				z2_avg+= 	(receiveByteZ2[1]);
			}

		}
		XPT_CS_HIGH();
		spi_unlock(DEV_TOUCH);
	}

	ts_CoordinatesRaw.x = x_avg/AVERAGE_POINTS;
	ts_CoordinatesRaw.y = y_avg/AVERAGE_POINTS;
	ts_CoordinatesRaw.z1 = z1_avg/AVERAGE_POINTS;
	ts_CoordinatesRaw.z2 = z2_avg/AVERAGE_POINTS;
	uint16_t max = xpt2046_max_measurement();
	/* Consider (0,0) as the top left point of the screen */
	switch(ts_Orientation)
	{
		case XPT2046_ORIENTATION_PORTRAIT:
		{
			ts_Coordinates.x = (ts_Size.width * (max - ts_CoordinatesRaw.x))/max - XPT2046_X_OFFSET;
			ts_Coordinates.y = (ts_Size.height * (ts_CoordinatesRaw.y))/max - XPT2046_Y_OFFSET;
			ts_Coordinates.z = ts_CoordinatesRaw.z1;
			break;
		}
		case XPT2046_ORIENTATION_LANDSCAPE:
		{
			ts_Coordinates.x = (ts_Size.width * (max - ts_CoordinatesRaw.y))/max - XPT2046_Y_OFFSET;
			ts_Coordinates.y = (ts_Size.height * (ts_CoordinatesRaw.x))/max - XPT2046_X_OFFSET;
			ts_Coordinates.z = ts_CoordinatesRaw.z1;
			break;
		}
		case XPT2046_ORIENTATION_PORTRAIT_MIRROR:
		{
			ts_Coordinates.x = (ts_Size.width * (ts_CoordinatesRaw.x))/max - XPT2046_X_OFFSET;
			ts_Coordinates.y = (ts_Size.height * (max - ts_CoordinatesRaw.y))/max - XPT2046_Y_OFFSET;
			ts_Coordinates.z = ts_CoordinatesRaw.z1;
			break;
		}
		case XPT2046_ORIENTATION_LANDSCAPE_MIRROR:
		{
			ts_Coordinates.x = (ts_Size.width *  (ts_CoordinatesRaw.y))/max - XPT2046_Y_OFFSET;
			ts_Coordinates.y = (ts_Size.height * (max- ts_CoordinatesRaw.x))/max - XPT2046_X_OFFSET;
			ts_Coordinates.z = ts_CoordinatesRaw.z1;
			break;
		}
		default:
		{
			break;
		}
	}
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
