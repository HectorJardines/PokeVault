#include "../Inc/drivers/pir_sensor.h"
#include "../Inc/drivers/adc.h"
#include "../Inc/common/defines.h"

#define NO_OF_PIR_SENSORS   (2U)
#define NO_SMPLS_PER_PIR    (3U)
#define PIR_PRES_THRSHOLD   (1000U) // TODO: VERIFY WHAT VALUE THIS SHOULD BE
#define PRESENCE_DETECTED(sample) ((sample) > PIR_PRES_THRSHOLD)

/***********************
 * STATIC DECLARATIONS
 ***********************/
static uint8_t process_sensor_samples(pir_sensors_t *sensors, uint16_t *samples);

static uint16_t sensor_samples[NO_OF_PIR_SENSORS * NO_SMPLS_PER_PIR];

/*******************
 * PUBLIC APIs
 *********************/

/**
 * @brief Initializes the PIR sensor module
 */

static uint8_t initialized = 0;
void pir_init(void) {
    adc_init();
    initialized = 1;
}

/**
 * @brief Samples the PIR sensors and computes whether a presence is detected or not
*/
pir_status_e pir_check_sensors(pir_sensors_t *presence_sensors) {
    uint8_t status = PIR_OK;
    uint8_t retries = 0xFF; 
    
    do {
        status = adc_read_samples(sensor_samples, MOTION_SENSOR);
        if (status == ADC_ERR)
            return PIR_ERR;
    } while (--retries && status != ADC_OK);

    if (retries == 0 || status == ADC_BSY)
        status = PIR_BSY;
    else {
        status = process_sensor_samples(presence_sensors, sensor_samples);
    }
    return status;
}


/*******************
 * STATIC DEFS
 ******************/

static uint8_t process_sensor_samples(pir_sensors_t *sensors, uint16_t *samples) {
    for (pir_sensor_e sensor_idx = SENSOR_UNIT1; sensor_idx < NO_OF_PIR_SENSORS; ++sensor_idx) {
        uint8_t count = 0;
        for (uint8_t off = 0; off < NO_SMPLS_PER_PIR; ++off) {
            uint8_t offset = NO_SMPLS_PER_PIR * off;
            count += PRESENCE_DETECTED(samples[sensor_idx + offset]);
        }
        
        switch (sensor_idx) {
            case SENSOR_UNIT1:
                sensors->presence_unit1 = (count > (NO_SMPLS_PER_PIR >> DIV_2)) ? PRESENCE_DETECETD : PRESNCE_NONE;
                break;
            case SENSOR_UNIT2:
                sensors->presence_unit2 = (count > (NO_SMPLS_PER_PIR >> DIV_2)) ? PRESENCE_DETECETD : PRESNCE_NONE;
                break;
            case SENSOR_UNIT3:
                sensors->presence_unit3 = (count > (NO_SMPLS_PER_PIR >> DIV_2)) ? PRESENCE_DETECETD : PRESNCE_NONE;
                break;
        }
    }
    return PIR_OK;
 }

