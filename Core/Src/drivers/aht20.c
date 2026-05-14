#include "../Inc/drivers/aht20.h"
#include "../Inc/drivers/i2c.h"

#define NUM_SENSORS             (3U)
#define STORAGE_UNIT_BASE       (0U)
#define MEASUREMENT_DATA_LEN    (6U) // 6 bytes to read after measurement complete

/************************
 * STATIC DECLARATIONS
 ***********************/
static uint8_t aht20_power_on(void);
static uint8_t aht20_start_measurement(aht20_sensor_e sensor);
static uint8_t read_measurement(aht20_sensor_e sensor);
static void aht20_reset(aht20_sensor_e sensor);

static aht20_t device = {.transmit = i2c_transmit, .receive = i2c_receive};

/********************
 * PUBLIC APIs
 ********************/
uint8_t intialized = 0;
void aht20_init(void) {
    uint8_t status = 0;
    i2c_init(I2C_DEVICE_AHT20);
    status = aht20_power_on();

    if (!status)
        intialized = 1;
    return status;
}

uint8_t aht20_read_data(aht20_sensor_e sensor, aht20_sensor_measurements_t *measurements) {
    uint8_t rslt = 0;
    // start measurement for all sensors
    switch (sensor) {
        case AHT20_UNIT0:
        case AHT20_UNIT1:
        case AHT20_UNIT2:
            rslt |= aht20_start_measurement(sensor);
            break;
        case AHT20_ALL_UNITS:
            for (uint8_t sensor_num = 0; sensor_num < NUM_SENSORS; ++sensor_num)
                rslt |= aht20_start_measurement(STORAGE_UNIT_BASE + sensor_num);
            break;
    }
    // delay 75ms (max measurement time)
    HAL_Delay(80);
    // sanity check with status bit read


    // block until all measurements are read
    switch (sensor) {
        case AHT20_UNIT0:
        case AHT20_UNIT1:
        case AHT20_UNIT2:
            rslt |= aht20_read_measurement(sensor, measurements);
            break;
        case AHT20_ALL_UNITS:
            for (uint8_t sensor_num = 0; sensor_num < NUM_SENSORS; ++sensor_num)
                rslt |= aht20_read_measurement(STORAGE_UNIT_BASE + sensor_num, measurements);
            break;
    }

    return rslt;
}


/********************
 * STATIC DEFS
 ********************/
/**
 * @brief Initialize and calibrate all of the AHT20 sensors of the system
 * 
 * 
 * 
 */
static uint8_t aht20_power_on(void) {
    // delay 40ms after device power on
    HAL_Delay(20);
    aht20_reset(AHT20_ALL_UNITS);
    // delay 40ms after device is reset as per datasheet
    HAL_Delay(40);
    // send initialization command
    uint8_t init_buf[3] = {AHT20_CMD_INIT, AHT20_INIT_BYTE1, AHT20_INIT_BYTE2};
    device.transmit(AHT20_DEV_ADDR, init_buf, 3);
    HAL_Delay(10); // delay for initialization to complete
    // check calibration bit is set
    return aht20_get_status(AHT20_CALIBRATION_Msk);
}

/**
 * @brief Retrieves the measurement upon completion and stores the translated input in measurements struct
 */
static uint8_t aht20_read_measurement(aht20_sensor_e sensor, aht20_sensor_measurements_t *measurements) {
    uint8_t rslt = 0;
    aht20_data_t *sensor_measurement;
    uint8_t measurement_data[MEASUREMENT_DATA_LEN];
    switch (sensor) {
        case AHT20_UNIT0:
            sensor_measurement = &measurements->UNIT0_DATA;
            break;
        case AHT20_UNIT1:
            sensor_measurement = &measurements->UNIT1_DATA;
            break;
        case AHT20_UNIT2:
            sensor_measurement = &measurements->UNIT2_DATA;
            break;
        default:
            break;
    }
    
    rslt = device.receive(AHT20_DEV_ADDR, &measurement_data, MEASUREMENT_DATA_LEN);
    if (measurement_data[0] & AHT20_BUSY_Msk) // first byte is AHT20 STATE WORD
        return 1;
    process_sensor_measurements(sensor_measurement, &measurement_data[1]);
    return rslt;
}

/**
 * @brief Starts the temp/humidity measurement, blocks until
 */
static uint8_t aht20_start_measurement(aht20_sensor_e sensor) {
    // start measurement
    uint8_t measurement_buf[3] = {AHT20_CMD_STRT_MEAS, AHT20_MEAS_BYTE1, AHT20_MEAS_BYTE2};
    return device.transmit(AHT20_DEV_ADDR, measurement_buf, 3);
}

/**
 * @brief Perform a soft reset on the AHT20 sensor
 */
static void aht20_reset(aht20_sensor_e sensor) {
    uint8_t reset_cmd = AHT20_CMD_RESET;
    device.transmit(AHT20_DEV_ADDR, &reset_cmd, 1);
    HAL_Delay(20); // soft reset takes no more than 20 ms
}

/**
 * @brief Retrieves the status word from the AHT20 sensor and checks specified mask
 * 
 * @param status_msk bitmask of status bits to be checked
 */
static uint8_t aht20_get_status(uint8_t status_msk) {
    uint8_t status_reg = 0x00;

    status_reg = AHT20_GET_STATUS;
    device.transmit(AHT20_DEV_ADDR, &status_reg, 1); // send status command
    device.receive(AHT20_DEV_ADDR, &status_reg, 1); // retrieve device state
    return status_reg & status_msk;
}

/**
 * @brief Take sensor readings and convert into real values stored in data struct
 */
static void process_sensor_measurements(aht20_data_t *data, uint8_t *measurement_buf) {
    uint32_t temp, humidity;
    // AHT20 provides a 20 bit humidity value, bytes 1 and 2 are the upper 16 bits and upper nibble of byte 3 is lower 4 bits
    humidity = ((measurement_buf[0] << 12) | (measurement_buf[1] << 4) | (measurement_buf[2] & 0xF0));
    // 20 bit temp value, lower nibble of byte 3 is upper 4 bits; bytes 4 and 5 are lower 16 bits
    temp = (((measurement_buf[2] & 0x0F) << 16) | (measurement_buf[3] << 8) | measurement_buf[4]);

    // humidity expressed as percentage (RH[%] = (S_RH/2^20) * 100)
    data->humidity = (humidity * 100) >> 20;
    // calculate temp in celcius as T = (S_t/2^20) * 200 - 50
    data->temp = ((temp * 200) >> 20) - 50;
}