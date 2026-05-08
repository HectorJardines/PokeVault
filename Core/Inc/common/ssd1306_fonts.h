#ifndef __SSD1306_FONTS_H__
#define __SSD1306_FONTS_H__

#include <stdint.h>
#define SSD1306_INCLUDE_FONT_6x8

typedef struct {
	const uint8_t FontWidth;    /*!< Font width in pixels */
	uint8_t FontHeight;   /*!< Font height in pixels */
	const uint16_t *data; /*!< Pointer to data font data array */
} Font_t;

#ifdef SSD1306_INCLUDE_FONT_6x8
extern Font_t Font_6x8;
#endif
#ifdef SSD1306_INCLUDE_FONT_7x10
extern Font_t Font_7x10;
#endif
#ifdef SSD1306_INCLUDE_FONT_11x18
extern Font_t Font_11x18;
#endif
#ifdef SSD1306_INCLUDE_FONT_16x26
extern Font_t Font_16x26;
#endif
#ifdef SSD1306_INCLUDE_FONT_16x24
extern Font_t Font_16x24;
#endif
#endif // __SSD1306_FONTS_H__