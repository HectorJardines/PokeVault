#include "movement_detect.h"
#include "io.h"
#include "i2c.h"
#include <stdio.h>

#define DEV_ID  (0x68)
#define MAX_IMU_TX_LEN (64)

/*************************
 * STATIC DECLARATIONS
 *************************/
static uint8_t configure_bmi160_dev(void);
static uint8_t configure_bmi160_int(void);
static void movement_int_anym_isr(void);
static void movement_int_dtap_isr(void);

static int8_t i2c_tx_wrapper(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
static int8_t i2c_rx_wrapper(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);

static movement_state_t unit_movement;
static imu_handle_t h_imu;

/**********************
 *    PUBLIC APIs
 **********************/

static uint8_t initialized = UNITIALIZED;
/**
 * @brief Initialize the BMI160 IMU peripheral
 * 
 * 
 * @return 0 on successful initialization; else 1
 */
uint8_t movement_init(void) {
    uint8_t status = STATUS_OK;
    i2c_init(I2C_DEVICE_AHT20);
    if (!initialized) {
        // configure device structure
        status = configure_bmi160_dev();
        if (status == STATUS_OK) {
            status = configure_bmi160_int();
            if (!status)
                initialized = INITIALIZED;
        }
    }

    // could add handling for init called twice
    return status;
}



/**
 * @brief Checks whether movement of the storage unit has been detected
 * 
 * 
 * 
 * @return 1 on movement detected; 0 else
 */
uint8_t movement_detected(void) {
    union bmi160_int_status stat;
    uint8_t byte = 0x00;
    // bmi160_get_regs(BMI160_INT_ENABLE_0_ADDR, &byte, 1, &h_imu.conf);
    // printf("INT CONFIG: %x\r\n", byte);
    // bmi160_get_int_status(BMI160_INT_STATUS_0, &stat, &h_imu.conf);
    // printf("INT BIT: %d\r\n", stat.bit.anym);
    uint8_t movement = unit_movement.motion_detected;
    return movement;
}


/**
 * @brief Checks whether movement of storage unit has stopped
 * 
 * 
 * 
 * @return 1 on no movement deteceted; 0 else
 */
uint8_t movement_stopped(void) {
    uint8_t no_movement = !unit_movement.motion_detected;
    unit_movement.motion_detected = 0;
    return no_movement;
}



/**
 * @brief Checks if tap on the storage unit has been detected
 * 
 * 
 * @return 1 on tap detected; 0 else
 */
uint8_t movement_tap_detected(void) {
    uint8_t tap = unit_movement.tap_detected;
    unit_movement.tap_detected = 0;
    return tap;
}


/********************
 * STATIC DEFS
 ********************/


static uint8_t configure_bmi160_dev(void) {
    uint8_t status = STATUS_OK;
    // configure interface
    h_imu.conf.intf = BMI160_I2C_INTF;
    h_imu.conf.read = i2c_rx_wrapper;
    h_imu.conf.write = i2c_tx_wrapper;
    h_imu.conf.delay_ms = HAL_Delay;
    h_imu.conf.id = DEV_ID;
    
    status = bmi160_init(&h_imu.conf);
    if (status == STATUS_OK) { // configure accel
        h_imu.conf.accel_cfg.bw = BMI160_ACCEL_BW_NORMAL_AVG4;
        h_imu.conf.accel_cfg.odr = BMI160_ACCEL_ODR_25HZ; // lower ODR is fine we only need wake on motion
        h_imu.conf.accel_cfg.power = BMI160_ACCEL_NORMAL_MODE;
        h_imu.conf.accel_cfg.range = BMI160_ACCEL_RANGE_8G; // lower resolution but we only care about strong movements

        status = bmi160_set_sens_conf(&h_imu.conf);
    }

    return status;
}

static uint8_t configure_bmi160_int(void) {
    uint8_t status = STATUS_OK;

    // configure INT1 pin for any motion interrupts
    h_imu.anym.int_pin_settg.output_en = BMI160_ENABLE; // PIN IN OUTPUT MODE
    h_imu.anym.int_pin_settg.edge_ctrl = BMI160_ENABLE; // EDGE TRIGGERED INTERRUPT
    h_imu.anym.int_pin_settg.output_mode = BMI160_ENABLE; // OPEN_DRAIN OUTPUT MODE
    h_imu.anym.int_pin_settg.output_type = BMI160_DISABLE; // ACTIVE LOW INTERRUPT MODE
    h_imu.anym.int_pin_settg.input_en = BMI160_DISABLE;
    h_imu.anym.int_pin_settg.latch_dur = BMI160_LATCH_DUR_10_MILLI_SEC;
    h_imu.anym.int_channel = BMI160_INT_CHANNEL_1; // SET INTERRUPT ON PIN 1

    h_imu.anym.int_type = BMI160_ACC_ANY_MOTION_INT;
    h_imu.anym.int_type_cfg.acc_any_motion_int.anymotion_x = BMI160_ENABLE;
    h_imu.anym.int_type_cfg.acc_any_motion_int.anymotion_y = BMI160_ENABLE;
    h_imu.anym.int_type_cfg.acc_any_motion_int.anymotion_z = BMI160_ENABLE;
    h_imu.anym.int_type_cfg.acc_any_motion_int.anymotion_en = BMI160_ENABLE;
    h_imu.anym.int_type_cfg.acc_any_motion_int.anymotion_dur = (uint8_t)0x2U;
    h_imu.anym.int_type_cfg.acc_any_motion_int.anymotion_thr = 10; // TODO: ADJUST THIS VALUE IN PRACTICE ( 20 * 15.63mg/LSB = 312.6 mg of force change )
    h_imu.anym.int_type_cfg.acc_any_motion_int.anymotion_data_src = BMI160_DISABLE; // pre-filter data src

    status = bmi160_set_int_config(&h_imu.anym, &h_imu.conf);

    if (status == STATUS_OK) {
        // CONFIGURE INT2 PIN FOR DOUBLE TAP INTERRUPTS
        h_imu.dtap.int_pin_settg.output_en = BMI160_ENABLE; // output mode
        h_imu.dtap.int_pin_settg.edge_ctrl = BMI160_ENABLE; // edge triggered
        h_imu.dtap.int_pin_settg.output_mode = BMI160_ENABLE; // open-drain
        h_imu.dtap.int_pin_settg.output_type = BMI160_DISABLE; // active low
        h_imu.dtap.int_pin_settg.input_en = BMI160_DISABLE;
        h_imu.dtap.int_pin_settg.latch_dur = BMI160_LATCH_DUR_10_MILLI_SEC; // disbale int latching
        h_imu.dtap.int_channel = BMI160_INT_CHANNEL_2;

        h_imu.dtap.int_type = BMI160_ACC_DOUBLE_TAP_INT;
        h_imu.dtap.int_type_cfg.acc_tap_int.tap_data_src = BMI160_DISABLE; // pre-filter data src
        h_imu.dtap.int_type_cfg.acc_tap_int.tap_en = BMI160_ENABLE;
        h_imu.dtap.int_type_cfg.acc_tap_int.tap_quiet = BMI160_ENABLE; // 20ms quiet period
        h_imu.dtap.int_type_cfg.acc_tap_int.tap_shock = BMI160_DISABLE; // 50 ms shock duration
        h_imu.dtap.int_type_cfg.acc_tap_int.tap_dur = 0x01; // 100ms after quiet period will count as dtap
        h_imu.dtap.int_type_cfg.acc_tap_int.tap_thr = 0x00; // 250mg * 0.5 = 125mg force difference

        status = bmi160_set_int_config(&h_imu.dtap, &h_imu.conf);
        
    }

    if (status == STATUS_OK) {
        io_configure_interrupt(IO_INT_ANYM, IO_INTERRUPT_FT, movement_int_anym_isr);
        io_irq_enable_interrupt(IO_INT_ANYM);
        io_configure_interrupt(IO_INT_DTAP, IO_INTERRUPT_FT, movement_int_dtap_isr);
        io_irq_enable_interrupt(IO_INT_DTAP);
    }

    return status;
}


static void movement_int_anym_isr(void) {
    unit_movement.motion_detected = 1;
}


static void movement_int_dtap_isr(void) {
    if (!unit_movement.tap_detected)
        unit_movement.tap_detected = 1;
}


static int8_t i2c_tx_wrapper(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len) {
    int8_t status = STATUS_OK;
    uint8_t buf[MAX_IMU_TX_LEN];
    buf[0] = reg_addr;
    memcpy((void *)&buf[1], (const void *)data, len);
    status = i2c_transmit(dev_addr, buf, len + 1); // send reg addr first
    return status;
}


static int8_t i2c_rx_wrapper(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len) {
    return i2c_write_read(dev_addr, reg_addr, data, len);
}

