/**
 * @author Hector Jardines
 * 
 * 
 * Updated IR sensors module, combines the PIR and IR 
 * sensor implementations. Removes ADC usage, in favor 
 * of a more fitting interrupt based approach.
 * 
 */

#include <stdint.h>

/***********************
 * STRUCTS/ENUMS
 ***********************/

typedef enum {
    IR_LINE_BROKEN,
    IR_LINE_CONNECTED
} ir_line_state_e;

typedef enum {
    PIR_PRESENCE,
    PIR_NO_PRESENCE
} pir_presence_state_e;

typedef struct {
    ir_line_state_e line_state;
    pir_presence_state_e presence_state;
} ir_sens_state_t;


/***********************
 * PUBLIC APIs
 **********************/

/**
 * @brief Initializes the IR and PIR sensors for the unit
 * 
 * 
 * 
 */
uint8_t ir_init(void);



/**
 * @brief Retrieves the current state of the IR sensors
 * 
 * 
 * @param[out] unit_state
 */
void ir_read_sens_state(ir_sens_state_t *unit_state);



/**
 * @brief
 * 
 * 
 * @param[]
 */

