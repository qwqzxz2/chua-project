/**
 * @file    ssd1306.h
 * @brief   SSD1306 OLED display driver - Header file
 * @details Supports 128x64 pixel OLED displays via I2C interface.
 *          Provides font rendering, primitive drawing, and framebuffer management.
 */

#ifndef SSD1306_H
#define SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/*===================================================================*/
/*                          Display Constants                        */
/*===================================================================*/

/** @brief SSD1306 I2C address (shifted for HAL: 0x3C << 1 = 0x78) */
#define SSD1306_I2C_ADDR        0x78U

/** @brief Display width in pixels */
#define SSD1306_WIDTH           128U

/** @brief Display height in pixels */
#define SSD1306_HEIGHT          64U

/** @brief Framebuffer size in bytes (128 x 64 / 8) */
#define SSD1306_FRAMEBUFFER_SIZE 1024U

/** @brief Number of pages (8 pixels per page) */
#define SSD1306_PAGES           8U

/*===================================================================*/
/*                        Status / Error Codes                       */
/*===================================================================*/

/**
 * @brief SSD1306 operation status codes
 */
typedef enum
{
    SSD1306_OK       = 0x00U,  /**< Operation completed successfully */
    SSD1306_ERROR    = 0x01U,  /**< General error */
    SSD1306_TIMEOUT  = 0x02U   /**< I2C communication timed out */
} SSD1306_Status_t;

/*===================================================================*/
/*                           Font Structure                          */
/*===================================================================*/

/**
 * @brief Font descriptor structure
 * @details Defines a monospace bitmap font. The bitmap data is stored
 *          row-by-row. For fonts 8 pixels wide or less, each row is
 *          one byte (bit 7 = leftmost pixel). For wider fonts, each
 *          row uses two bytes (big-endian, bit 15 = leftmost pixel).
 */
typedef struct
{
    uint8_t        Width;   /**< Character width in pixels */
    uint8_t        Height;  /**< Character height in pixels */
    const uint8_t* Data;    /**< Pointer to font bitmap data array */
} SSD1306_Font_t;

/*===================================================================*/
/*                    External Font Declarations                      */
/*===================================================================*/

/** @brief 7x10 pixel monospace font (ASCII 32-126) */
extern const SSD1306_Font_t Font_7x10;

/** @brief 11x18 pixel monospace font (ASCII 32-126) */
extern const SSD1306_Font_t Font_11x18;

/*===================================================================*/
/*                     Function Declarations                         */
/*===================================================================*/

/**
 * @brief  Initialize the SSD1306 OLED display
 * @param  hi2c Pointer to I2C handle (e.g., &hi2c1)
 * @return SSD1306_OK on success, error code otherwise
 */
SSD1306_Status_t SSD1306_Init(void* hi2c);

/**
 * @brief  Clear the internal framebuffer (all pixels off)
 * @note   Does NOT update the display; call SSD1306_UpdateScreen() afterwards
 */
void SSD1306_Clear(void);

/**
 * @brief  Set or clear a single pixel in the framebuffer
 * @param  x X coordinate (0 .. SSD1306_WIDTH-1)
 * @param  y Y coordinate (0 .. SSD1306_HEIGHT-1)
 * @param  color 0 = pixel off, non-zero = pixel on
 */
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);

/**
 * @brief  Draw a string at the given position using the specified font
 * @param  x    X coordinate of the top-left corner
 * @param  y    Y coordinate of the top-left corner
 * @param  str  Null-terminated string to display
 * @param  font Pointer to the font descriptor (e.g., &Font_7x10)
 * @return Number of characters drawn
 */
uint8_t SSD1306_DrawString(uint8_t x, uint8_t y, const char* str, const SSD1306_Font_t* font);

/**
 * @brief  Draw a line between two points using Bresenham's algorithm
 * @param  x0 Start X
 * @param  y0 Start Y
 * @param  x1 End X
 * @param  y1 End Y
 */
void SSD1306_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);

/**
 * @brief  Draw a rectangle (outline or filled)
 * @param  x      Top-left X
 * @param  y      Top-left Y
 * @param  w      Width in pixels
 * @param  h      Height in pixels
 * @param  filled 0 = outline, non-zero = filled
 */
void SSD1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t filled);

/**
 * @brief  Draw a circle using the midpoint circle algorithm
 * @param  x0  Center X
 * @param  y0  Center Y
 * @param  r   Radius in pixels
 */
void SSD1306_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r);

/**
 * @brief  Transmit the internal framebuffer to the SSD1306 via I2C
 * @return SSD1306_OK on success, error code otherwise
 * @note    Updates the entire 128x64 display
 */
SSD1306_Status_t SSD1306_UpdateScreen(void);

/**
 * @brief  Draw a single character at the given position
 * @param  x     X coordinate of the top-left corner
 * @param  y     Y coordinate of the top-left corner
 * @param  ch    ASCII character to draw (32-126)
 * @param  font  Pointer to the font descriptor
 * @return 1 if character was drawn, 0 if out of range
 */
uint8_t SSD1306_DrawChar(uint8_t x, uint8_t y, char ch, const SSD1306_Font_t* font);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
