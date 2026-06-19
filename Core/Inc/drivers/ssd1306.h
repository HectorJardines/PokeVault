#ifndef _SSD1306_H
#define _SSD1306_H

#include <stdint.h>
#include "../Inc/common/ssd1306_fonts.h"
#include "stm32f4xx.h"

/**
 * SSD1306 FUNDAMENTAL COMMAND BYTES
 */
#define CMD_SET_CONTRAST        (0x81U) // b1000 0001
#define CMD_ENT_DIS_ON_RAM      (0xA4) // turns on entire display and resumes RAM content
#define CMD_ENT_DIS_ON_NO_RAM   (0xA5)
#define CMD_DIS_ORIENT_NORMAL   (0xA6)
#define CMD_DIS_ORIENT_INVERT   (0xA7)
#define CMD_SET_DIS_OFF         (0xAE) // display off (sleep mode)
#define CMD_SET_DIS_ON         (0xAF)

/**
 * SSD1306 ADDRESSING SCHEME COMMAND BYTES
 */
#define CMD_SET_LOW_COL_STRT_ADDR_PG    (0x00) // set lower nibble of col start addr resg for Page addressing mode (using lower 4 bits)
#define CMD_SET_HI_COL_STRT_ADDR_PG     (0x10)

#define CMD_SET_MEM_ADDRESSING_MODE     (0x20) // SEND ADDITIONAL BYTE WITH LOWER 2 BITS AS ADDR MODE
#define OPT_ADDR_MODE_HORI              (0x00)
#define OPT_ADDR_MODE_VERT              (0x01)
#define OPT_ADDR_MODE_PAGE              (0x02)

#define CMD_SET_COL_STRT_END_ADDR       (0x21) // TRIPLE BYTE COMMAND REQUIRES 7 BIT COL START AND END ADDRS (VERT/HORI MODE ONLY)
#define CMD_SET_PAGE_STRT_END_ADDR      (0x22) // TRIPLE BYTE COMMAND SIMILAR AS COL (VERT/HORI MODE ONLY)
#define CMD_SET_PAGE_STRT_ADDR          (0xB0) // MSK LOWER TWO 3 BITS FOR START AT PAGE0-7 (PG ADDR MODE ONLY)


/**
 * SSD1306 HW CONFIG COMMAND BYTES (PANEL RES / LAYOUT)
 */
#define CMD_SET_DISPLAY_STRT_LINE   (0x40) // MSK LOWER 6 BITS FOR LINES 0-63
#define CMD_SET_SEGREMAP_NORMAL     (0xA0U)
#define CMD_SET_SEGREMAP_ALT        (0xA1U)
#define CMD_SET_MULTIPLEX_RATIO     (0xA8) // 2 BYTE COMMAND SUBSEQUENT BYTE CONATINS 6 BIT MUX RATIO VALUE (0-14 INVALID)
#define CMD_SET_COM_SCAN_NORMAL     (0xC0) // SCANS FROM COM0 - COM[N-1] WHERE N IS MUX RATIO
#define CMD_SET_COM_SCAN_REMAP      (0xC8)
#define CMD_SET_DIS_OFFSET          (0xD3) // 2 BYTE CMD; SUBSEQUENT 6 BIT DATA SETS VERTICAL SHIFT FROM COM0-63
#define CMD_SET_COM_HW_CFG          (0xDA) // 2 BYTE CMD; SUBSEQUENT A[5:4] SPECIFIED COM PIN CFG


/**
 * SSD1306 TIMING / DRIVING SCHEME CMD BYTES
 */
#define CMD_SET_CLK_DIV_OSC_FREQ    (0xD5) // 2 BYTE CMD; A[3:0] + 1 defines the display clk div ratio; A[7:4] INCREASE OSC FREQ
#define CMD_SET_PRE_CHARGE_T        (0xD9) // 2 BYTE CMD; A[7:4] defines phase 2 period; A[3:0] defines phase 1 period (BOTH UP TO 15 DCLK)

#define CMD_DEACTIVATE_SCROLL   (0x28U)
#define CMD_SET_CHARGEPUMP      (0x8D)
#define OPT_CHARGEPUMP_ON       (0x14)

/**
 * BITMAPS
 */
extern uint8_t bitmap_cat[1024];

/**
 * ENUMS
 */

typedef enum {
    COLOR_BLACK,
    COLOR_WHITE,
    COLOR_INVERSE
} ssd1306_color_e;

typedef enum {
    ORIENT_HORIZONTAL_NORMAL,
    ORIENT_HORIZONTAL_FLIPPED,
    ORIENT_VERT_ROT_RIGHT,
    ORIENT_VERT_ROT_LEFT
} ssd1306_orientation_e;


/**
 * USER APIS
 */

/**
 * @brief Initializes the SSD1306 driver configuration via I2C transactions
 * 
 * This API configures the SSD1306 display clock, addressing scheme, display layout, etc.
 * It also intializes the display hanlder with the appropriate i2c APIs.
 * 
 * @return void
 */
void ssd1306_init(void);

/**
 * @brief Registers a display buffer flush complete callback function
 * 
 * @param flush_cplt_cb pointer to the flush callback
 */
void ssd1306_install_flush_cb(void (*flush_cplt_cb)(DMA_HandleTypeDef* h_dma));

/**
 * @brief Stores a single pixel in ssd1306 GDDRAM
 * 
 * This API stores a single pixel, represented as an (x,y) coordinate pair in the
 * SSD1306's interal 128x64 bit GDDRAM. This API should be called to populate the 
 * buffer and ssd1306_display should be called to update the display contents.
 * 
 * @param dis_x_coord
 * @param dis_y_coord
 * @param color
 * 
 * @return negative if err; positive else
 */
int8_t ssd1306_draw_pixel(int16_t dis_x_coord, int16_t dis_y_coord, ssd1306_color_e color);

/**
 * @brief Updates the display contents with the contens stored in GDDRAM
 * 
 * Initiates an I2C transaction with the SSD1306 driver that loads all
 * of the pixels drawn (via ssd1306_draw_pixel) into the physical memory
 * of the ssd1306.
 * 
 * @return void
 */
uint8_t ssd1306_display(void);

/**
 * @brief loads bit map into SRAM and displays contents
 * 
 * This function diplays a bitmap of size 128x64 = 1024 bytes on the OLED.
 * Does not need to call ssd1306_display. Use for predefined bitmaps where 
 * writing each pixel is not feasible.
 * 
 * @param bitmap data buffer corresponding to the bitmap to be displayed
 */
void ssd1306_set_pixels(uint8_t *pixel_map, uint8_t start_x, uint8_t start_y, uint8_t end_x, uint8_t end_y);

/**
 * @brief Set cursor postions to which char will be written
 * 
 * @param x position along the x axis of the display
 * @param y position along the y axis of the display
 */
uint8_t ssd1306_set_cursor(uint8_t x, uint8_t y);

/**
 * @brief write a single char at the position set by ssd1306_set_cursor
 * 
 * Writes the specified character, in the specified font and color, at the 
 * OLED position set by ssd1306_set_cursor. Advances the position of the cursor
 * so that subsequent characters do not overwrite the current one.
 * 
 * @param ch a single character to be written
 * @param font the font in which to display the character
 * @param color color of the character
 * 
 * @return the character written to the display
 */
char ssd1306_write_char(char ch, Font_t font, ssd1306_color_e color);

/**
 * @brief writes the given string at cursor position set by ssd1306_set_cursor
 * 
 * @param str character arrary to be displayed on OLED
 * @param font font in which the string will be displayed
 * @param color color in whcih the string will be displayed
 * 
 * @return 0 on success; 1 else
 */
uint8_t ssd1306_write_string(char *str, Font_t font, ssd1306_color_e color);

/**
 * @brief clears the display buffer contents in RAM
 * 
 * @note does not update the display, must call ssd1306_display
 */
void ssd1306_clear_display(void);


#endif