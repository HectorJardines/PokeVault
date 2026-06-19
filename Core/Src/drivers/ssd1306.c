#include "../Inc/drivers/ssd1306.h"
#include "../Inc/drivers/i2c.h"
#include "../Inc/common/log.h"
#include "../Inc/common/defines.h"

#include <string.h>

#define SSD1306_OK  (0U)
#define SSD1306_ERR (1U)

#define SINGLE_BYTE     (2U) // need to send CTL byte and DATA/CMD byte as a pair
#define DISPLAY_HEIGHT  (64U)
#define DISPLAY_WIDTH    (128U)
#define SSD1306_DEV_ADDR        (0x3CU) // addr can be 0b0111100 or 0b0111101
/**
 *  
 * CTL BYTE FORMAT = Co Bit | D/C# Bit | b'0' * 6
 * Co - Continuation Bit; when cleared, next byte conatins data only
 * D/C# - when cleared, next byte is interpreted as command. when set, next byte is data stored in GDDRAM
 * 
 * GDDRAM STRUCTURE - 8 pages each with 8 rows and 128 columns each data write
 * stores a byte into column i of page n, each bit is stored MSB first.
 * i.e. D0 in row 0 and D7 in row 7
 */  
#define SSD1306_CTL_BYTE        (0x00U)
#define SSD1306_CTL_BYTE_DATA   (SSD1306_CTL_BYTE | 0x40U)
#define SSD1306_CTL_BYTE_CMD    (SSD1306_CTL_BYTE)
#define LEN_DATA_CTL_AND_DATA   (1025U)

#define ADDR_VERT   (0U)
#define ADDR_HORI   (1U)

/**************************
 *  STATIC DECLARATIONS
 **************************/
struct ssd1306_oled_disp_t {
    uint8_t (*i2c_tx)(uint8_t dev_addr, uint8_t *data, uint32_t len);
    uint8_t (*i2c_rx)(uint8_t dev_addr, uint8_t *data, uint32_t len);
    uint8_t (*i2c_tx_dma)(uint8_t dev_addr, uint8_t *data, uint32_t len);
    uint8_t *gddr_buf;
    uint16_t buf_len;
    uint16_t width;
    uint16_t height;
    ssd1306_orientation_e orientation;
    uint8_t cursor_x;
    uint8_t cursor_y;
};

// 1024 BYTES FOR OUR USE CASE, FIGURE OUT WHY THIS WORKS...
static uint8_t oled_display_buf[DISPLAY_HEIGHT * ((DISPLAY_WIDTH + 7) >> DIV_8)];
static uint8_t data_buf[LEN_DATA_CTL_AND_DATA]; // copy data buf here to include data ctl byte
static struct ssd1306_oled_disp_t display = {.i2c_rx = i2c_receive, .i2c_tx = i2c_transmit, 
    .i2c_tx_dma = i2c_transmit_dma, .gddr_buf = oled_display_buf, .width = DISPLAY_WIDTH, .height = DISPLAY_HEIGHT, 
    .orientation = ORIENT_HORIZONTAL_NORMAL, .cursor_x = 0, .cursor_y = 0};

static uint8_t set_addressing_scheme(void);
static int8_t ssd1306_pwr_on(void);
static int8_t send_single_byte_cmd(uint8_t cmd);
static int8_t send_multi_byte_cmd(uint8_t *cmds, uint8_t len);
static int8_t send_single_byte_data(uint8_t *data);
static int8_t send_multi_byte_data(uint8_t *data, uint32_t len);


/****************************
 *      SSD1306 APIs
 ****************************/

static bool initialized = false; 
 /**
 * @brief 
 */
void ssd1306_init(void) {
    i2c_init(I2C_DEVICE_SSD1306);
    display.buf_len = (display.width * ((display.height + 7) >> DIV_8));
    set_addressing_scheme();
    ssd1306_pwr_on();
    ssd1306_clear_display();
    initialized = true;
}

void ssd1306_install_flush_cb(void (*flush_cplt_cb)(DMA_HandleTypeDef* h_dma)) {
    i2c_set_dma_tx_cplt_cb(flush_cplt_cb);
}

/**
 * @brief updates the pixel position in GDDRAM
 * 
 * @note must call ssd1306_display to actually render the drawn pixels
 */
int8_t ssd1306_draw_pixel(int16_t dis_x_coord, int16_t dis_y_coord, ssd1306_color_e color) {
    if ((dis_x_coord < 0 || dis_x_coord >= DISPLAY_WIDTH) || (dis_y_coord < 0 || dis_y_coord >= DISPLAY_HEIGHT))
        return SSD1306_ERR; // pixel coordinate out of bounds
    switch(display.orientation) {
        case ORIENT_HORIZONTAL_NORMAL:
            // DO NOTHING X/Y DONT NEED TO BE MODIFIED
            break;
        // NOT IMPLEMENTED CURRENTLY
        case ORIENT_HORIZONTAL_FLIPPED:
        case ORIENT_VERT_ROT_RIGHT:
        case ORIENT_VERT_ROT_LEFT:
            break;
    }

    switch(color) {
        case COLOR_INVERSE:
            // y corresponds to row, each row is of size = display width (div 8 since each byte is 8 rows), x coord is offset from start of row
            display.gddr_buf[dis_x_coord + (dis_y_coord >> DIV_8) * display.width] ^= (1 << (dis_y_coord & 7));
            break;
        case COLOR_BLACK:
            display.gddr_buf[dis_x_coord + (dis_y_coord >> DIV_8) * display.width] &= ~(1 << (dis_y_coord & 7));
            break;
        case COLOR_WHITE:
            display.gddr_buf[dis_x_coord + (dis_y_coord >> DIV_8) * display.width] |= (1 << (dis_y_coord & 7));
            break;
    }
    return SSD1306_OK;
}

/**
 * @brief sets the position of display "cursor" to where next char will be written
 */
uint8_t ssd1306_set_cursor(uint8_t x, uint8_t y) {
    if (x > DISPLAY_WIDTH - 1 || x < 0 || y > DISPLAY_HEIGHT - 1 || y < 0)
        return -1; // error x/y position out of bounds
    display.cursor_x = x;
    display.cursor_y = y;
    return 0;
}

char ssd1306_write_char(char ch, Font_t font, ssd1306_color_e color) {
    if (ch < 32 || ch > 126)
        return -1; // not a valid ASCII character
    if (display.cursor_x + font.FontWidth > DISPLAY_WIDTH - 1 || display.cursor_y + font.FontHeight > DISPLAY_HEIGHT)
        return -1; // char won't fit on screen
    
    uint32_t c;
    for (int i = 0; i < font.FontHeight; ++i) {
        c = font.data[(ch - 32) * font.FontHeight + i];
        for (int j = 0; j < font.FontWidth; ++j) {
            if ((c << j) & 0x8000) // check if pixel is on
                ssd1306_draw_pixel(display.cursor_x  + j, display.cursor_y + i, color);
            else
                ssd1306_draw_pixel(display.cursor_x  + j, display.cursor_y + i, !color);
        }
    }

    ssd1306_set_cursor(display.cursor_x + font.FontWidth, display.cursor_y);
    return ch;
}

uint8_t ssd1306_write_string(char *str, Font_t font, ssd1306_color_e color) {
    char c;
    uint8_t res;
    while (*str) {
        c = *str;
        res = ssd1306_write_char(c, font, color);
        if (res != *str)
            return -1; // error writing char
        str++;
    }
    return 0;
}

/**
 * @brief writes all bytes in display.gddr_buf into ssd1306 RAM
 */
uint8_t ssd1306_display(void) {
    // uint8_t res = set_addressing_scheme();
    uint16_t count = display.width * ((display.height + 7) >> DIV_8);
    uint8_t *display_data = display.gddr_buf;
    uint8_t res = send_multi_byte_data(display_data, count);

    return res;
}

/**
 * @brief Display a pixel_map to the screen
 * 
 * @note does not require ssd1305_display to be called
 */
void ssd1306_set_pixels(uint8_t *pixel_map, uint8_t start_x, uint8_t start_y, uint8_t end_x, uint8_t end_y) {
    uint8_t width = (end_x - start_x) + 1;
    uint8_t height = (end_y - start_y) + 1;
    uint8_t pages = height >> 3;

    for (uint8_t pg = 0; pg < pages; ++pg) {
        for (uint8_t x = 0; x < width; ++x) {
            uint8_t curr_y = (start_y >> 3) + pg;
            uint8_t curr_x = (start_x + x);

            uint16_t gddr_buf_idx = (curr_y * display.width) + curr_x;
            display.gddr_buf[gddr_buf_idx] = pixel_map[(x * height + (pg * 8)) / 8];
        }
    }
}

void ssd1306_clear_display(void) {
    for (uint16_t i = 0; i < display.buf_len; i++)
        display.gddr_buf[i] = 0;
}

/*************************
 *  STATIC DEFINITIONS
 *************************/

static int8_t ssd1306_pwr_on(void) {
    uint8_t init_cmds[] = {SSD1306_CTL_BYTE_CMD,
                        CMD_SET_DIS_OFF, 
                        CMD_SET_CLK_DIV_OSC_FREQ, 
                        0x80, 
                        CMD_SET_MULTIPLEX_RATIO,
                        DISPLAY_HEIGHT - 1};
    int8_t res = send_multi_byte_cmd(init_cmds, sizeof(init_cmds));

    uint8_t layout_cmds[] = {SSD1306_CTL_BYTE_CMD,
                            CMD_SET_DIS_OFFSET,
                            0x00,
                            CMD_SET_DISPLAY_STRT_LINE | 0x00,
                            CMD_SET_COM_SCAN_NORMAL,
                            CMD_SET_SEGREMAP_NORMAL};
    res = send_multi_byte_cmd(layout_cmds, sizeof(layout_cmds));

    uint8_t hw_config_cmds[] = {SSD1306_CTL_BYTE_CMD,
                                CMD_SET_COM_HW_CFG,
                                0x12,
                                CMD_SET_CONTRAST,
                                0x9F,
                                CMD_SET_PRE_CHARGE_T,
                                0xF1, // phase1 = 15, phase2 = 1
                                CMD_SET_CHARGEPUMP,
                                0x14}; // chargepump on
    res = send_multi_byte_cmd(hw_config_cmds, sizeof(hw_config_cmds));

    uint8_t display_on_cmds[] = {SSD1306_CTL_BYTE_CMD,
                                CMD_ENT_DIS_ON_RAM,
                                CMD_DIS_ORIENT_NORMAL,
                                CMD_DEACTIVATE_SCROLL,
                                CMD_SET_DIS_ON};
    res = send_multi_byte_cmd(display_on_cmds, sizeof(display_on_cmds));

    return res;
}

static uint8_t set_addressing_scheme(void) {
    uint8_t hori_addressing[] = {SSD1306_CTL_BYTE_CMD, 
                                CMD_SET_MEM_ADDRESSING_MODE, 
                                OPT_ADDR_MODE_HORI};
    int8_t res = send_multi_byte_cmd(hori_addressing, sizeof(hori_addressing));
    uint8_t hori_strt_end_col[] = {SSD1306_CTL_BYTE_CMD, 
                                    CMD_SET_COL_STRT_END_ADDR, 
                                    0x00, // start column
                                    0x7F}; // end column
    res |= send_multi_byte_cmd(hori_strt_end_col, sizeof(hori_strt_end_col));
    uint8_t hori_strt_end_pg[] = {SSD1306_CTL_BYTE_CMD, 
                                    CMD_SET_PAGE_STRT_END_ADDR, 
                                    0x00, // start row
                                    0x07}; // end row
    res |= send_multi_byte_cmd(hori_strt_end_pg, sizeof(hori_strt_end_pg));

    return res;
}

static int8_t send_single_byte_cmd(uint8_t cmd) {
    int8_t res = I2C_BUSY_IN_TX;
    uint16_t retry = UINT16_MAX;
    uint8_t cmd_pair[] = {SSD1306_CTL_BYTE_CMD, cmd};
    while (retry-- && res == I2C_BUSY_IN_TX)
        res = display.i2c_tx(SSD1306_DEV_ADDR, cmd_pair, SINGLE_BYTE);
    return res;
}

static int8_t send_multi_byte_cmd(uint8_t *cmds, uint8_t len) {
    uint16_t retry = UINT16_MAX;
    int8_t res = I2C_BUSY_IN_TX;
    while (retry-- && res == I2C_BUSY_IN_TX)
        res = display.i2c_tx(SSD1306_DEV_ADDR, cmds, len);
    return res;
}

static int8_t send_single_byte_data(uint8_t *data) {
    int8_t res = I2C_BUSY_IN_TX;
    uint16_t retry = UINT16_MAX;
    uint8_t data_pair[] = {SSD1306_CTL_BYTE_DATA, *data};
    while (retry-- && res == I2C_BUSY_IN_TX)
        res = display.i2c_tx(SSD1306_DEV_ADDR, data_pair, SINGLE_BYTE);
    return res;
}

static int8_t send_multi_byte_data(uint8_t *data, uint32_t len) {
    int8_t res = I2C_OK;
    uint16_t retry = UINT16_MAX;
    data_buf[0] = SSD1306_CTL_BYTE_DATA;
    memcpy(&data_buf[1], data, len);
    // while (retry-- && res == I2C_BUSY_IN_TX)
    res = display.i2c_tx_dma(SSD1306_DEV_ADDR, data_buf, LEN_DATA_CTL_AND_DATA);
    return res;
}

