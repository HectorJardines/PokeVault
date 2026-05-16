#include "../Inc/drivers/ir_sensor.h"
#include  "../Inc/drivers/adc.h"
#include "../Inc/common/defines.h"

#define NO_OF_IR_SENSORS    (3U)
#define NO_OF_SMPL_PER_SENS (3U)
#define SENS_SAMPLE_OFFSET  (3U) // subsequent samples are offset by 3

#define IR_THRSH (1000U)
#define IS_LINE_CONNECTED(sample) ((sample) > IR_THRSH)

/************************
 * STATIC DECLARATIONS
 ************************/
static void ir_process_samples(ir_lines_t *lines, uint16_t *samples);

static uint16_t ir_samples[NO_OF_IR_SENSORS * NO_OF_SMPL_PER_SENS];
static io_e ir_emitters[NO_OF_IR_SENSORS] = {IO_IR_EMIT_UNIT1, IO_IR_EMIT_UNIT2, IO_IR_EMIT_UNIT3};

/***********************
 * PUBLIC APIs
 ***********************/

static uint8_t initialized = 0;
void ir_init(void) {
    adc_init();
    initialized = 1;
}

/**
 * @brief Retrieves and processes the ADC samples of all units
 * 
 * Each sensor is sampled three times in the DMA xfer, if all the 
 * majority of the samples have pass the line connected 
 * threshold, the line is considered to be connected.
 */
ir_status_e ir_check_lines(ir_lines_t *lines) {
    uint8_t status = 0, retries = 0xFF;
    do {
        status = adc_read_samples(ir_samples, IR_RECEIVER);
        if (status == ADC_ERR)
            return IR_ERR;
    } while (status != ADC_OK && --retries);

    if (status == ADC_BSY || retries == 0)
        status = IR_BSY;

    ir_process_samples(lines, ir_samples);
    return status;
}

/**
 * @brief This API enables the IR emitter associated with the specified unti
 */
void ir_enable_line(ir_line_unit_e unit) {
    io_set_out(ir_emitters[unit], HIGH);
}

/**
 * @brief This API disables the IR emitter associated with the specified unit
 * 
 * We can't disable a single unit's ADC conversions without disabling all of the
 * other units. This API disables the corresponding IR emitter so that 
 * the line cannot be connected when lifting the storage unit lid.
 * 
 */
void ir_disable_line(ir_line_unit_e unit) {
    io_set_out(ir_emitters[unit], LOW);
}


/************************
 * STATIC DEFS
 ***********************/

/**
 * @brief Process all samples and store results in lines struct
 * 
 * Each sensor is sampled three times in the DMA xfer process. 
 * If the majority of the samples indicate that the line is connected
 * (i.e. adc sample > connected_threshold) then the specific line
 * is considered to be connected.
 */
static void ir_process_samples(ir_lines_t *lines, uint16_t *samples) {
    for (ir_line_unit_e sens_idx = IR_LINE_UNIT1; sens_idx < NO_OF_IR_SENSORS; ++sens_idx) {
        uint8_t count = 0; // number of samples past THRSHOLD
        for (uint8_t i = 0; i < NO_OF_SMPL_PER_SENS; ++i) {
            uint8_t offset = SENS_SAMPLE_OFFSET * i; // samples are stored in a repeating seq. e.g. [SENS 0, SENS1, SENS2, SENS0, ...]
            count += IS_LINE_CONNECTED(samples[sens_idx + offset]);
        }

        // set the corresponding line state
        switch (sens_idx) {
            case IR_LINE_UNIT1:
                lines->unit1_status = (count > (NO_OF_SMPL_PER_SENS >> DIV_2)) ? IR_LINE_CONNECTED : IR_LINE_BROKEN;
                break;
            case IR_LINE_UNIT2:
                lines->unit2_status = (count > (NO_OF_SMPL_PER_SENS >> DIV_2)) ? IR_LINE_CONNECTED : IR_LINE_BROKEN;
                break;
            case IR_LINE_UNIT3:
                lines->unit3_status = (count > (NO_OF_SMPL_PER_SENS >> DIV_2)) ? IR_LINE_CONNECTED : IR_LINE_BROKEN;
                break;
        }
    }
}

