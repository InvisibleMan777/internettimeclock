#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#define I2C_BUS_PORT 0 // I2C port number for the OLED display
#define I2C_CLK_HZ 400000 // I2C clock speed in Hz

#define OLED_WIDTH 128 // OLED display width in pixels
#define OLED_HEIGHT 64 // OLED display height in pixels
#define OLED_PAGES (OLED_HEIGHT / 8) // Number of 8-pixel pages in the OLED display

esp_lcd_panel_handle_t panel_handle = NULL;

/**
 * This font set provides bitmap representations for ASCII characters (32-126)
 * optimized for small OLED displays. Each character is represented as a 5-byte
 * array where each byte corresponds to one column of the character glyph.
 * 
 * Font Creation Method:
 * - Each glyph is 5 pixels wide and 7 pixels tall (5x7 bitmap)
 * - Each uint8_t value represents a vertical column (7 bits for 7 rows)
 * - Bit set (1) = pixel ON, Bit clear (0) = pixel OFF
 * - LSB represents the bottom row, MSB represents the top row
 * - Total of 5 bytes per character creates the complete horizontal representation
 * 
 * Example interpretation of glyph_I {0x00, 0x41, 0x7f, 0x41, 0x00}:
 *   Column 0: 0x00 = 0000000 (empty column)
 *   Column 1: 0x41 = 0100001 (top and bottom pixels)
 *   Column 2: 0x7f = 1111111 (full vertical line)
 *   Column 3: 0x41 = 0100001 (top and bottom pixels)
 *   Column 4: 0x00 = 0000000 (empty column)
 */

// Simple 5x7 font for ASCII characters 32-126
static const uint8_t glyph_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t glyph_D[5] = {0x7f, 0x41, 0x41, 0x22, 0x1c};
static const uint8_t glyph_E[5] = {0x7f, 0x49, 0x49, 0x49, 0x41};
static const uint8_t glyph_H[5] = {0x7f, 0x08, 0x08, 0x08, 0x7f};
static const uint8_t glyph_L[5] = {0x7f, 0x40, 0x40, 0x40, 0x40};
static const uint8_t glyph_O[5] = {0x3e, 0x41, 0x41, 0x41, 0x3e};
static const uint8_t glyph_A[5] = {0x7e, 0x11, 0x11, 0x11, 0x7e};
static const uint8_t glyph_B[5] = {0x7f, 0x49, 0x49, 0x49, 0x36};
static const uint8_t glyph_C[5] = {0x3e, 0x41, 0x41, 0x41, 0x22};
static const uint8_t glyph_F[5] = {0x7f, 0x09, 0x09, 0x09, 0x01};
static const uint8_t glyph_G[5] = {0x3e, 0x41, 0x49, 0x49, 0x32};
static const uint8_t glyph_I[5] = {0x00, 0x41, 0x7f, 0x41, 0x00};
static const uint8_t glyph_J[5] = {0x20, 0x40, 0x41, 0x3f, 0x01};
static const uint8_t glyph_K[5] = {0x7f, 0x08, 0x14, 0x22, 0x41};
static const uint8_t glyph_M[5] = {0x7f, 0x02, 0x04, 0x02, 0x7f};
static const uint8_t glyph_N[5] = {0x7f, 0x04, 0x08, 0x10, 0x7f};
static const uint8_t glyph_P[5] = {0x7f, 0x09, 0x09, 0x09, 0x06};
static const uint8_t glyph_Q[5] = {0x3e, 0x41, 0x41, 0x21, 0x5e};
static const uint8_t glyph_R[5] = {0x7f, 0x09, 0x09, 0x19, 0x66};
static const uint8_t glyph_S[5] = {0x26, 0x49, 0x49, 0x49, 0x32};
static const uint8_t glyph_T[5] = {0x01, 0x01, 0x7f, 0x01, 0x01};
static const uint8_t glyph_U[5] = {0x3f, 0x40, 0x40, 0x40, 0x3f};
static const uint8_t glyph_V[5] = {0x1f, 0x20, 0x40, 0x20, 0x1f};
static const uint8_t glyph_W[5] = {0x3f, 0x40, 0x38, 0x40, 0x3f};
static const uint8_t glyph_X[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
static const uint8_t glyph_Y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};
static const uint8_t glyph_Z[5] = {0x61, 0x51, 0x49, 0x45, 0x43};
static const uint8_t glyph_0[5] = {0x3e, 0x51, 0x49, 0x45, 0x3e};
static const uint8_t glyph_1[5] = {0x00, 0x42, 0x7f, 0x40, 0x00};
static const uint8_t glyph_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
static const uint8_t glyph_3[5] = {0x21, 0x41, 0x45, 0x4b, 0x31};
static const uint8_t glyph_4[5] = {0x18, 0x14, 0x12, 0x7f, 0x10};
static const uint8_t glyph_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
static const uint8_t glyph_6[5] = {0x3c, 0x4a, 0x49, 0x49, 0x30};
static const uint8_t glyph_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
static const uint8_t glyph_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
static const uint8_t glyph_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1e};
static const uint8_t glyph_dot[5] = {0x00, 0x00, 0x60, 0x60, 0x00};
static const uint8_t glyph_dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
static const uint8_t glyph_plus[5] = {0x08, 0x08, 0x3e, 0x08, 0x08};
static const uint8_t glyph_double_dot[5] = {0x00, 0x00, 0x66, 0x66, 0x00};
static const uint8_t glyph_low_dash[5] = {0x40, 0x40, 0x40, 0x40, 0x40};
static const uint8_t glyph_at[5] = {0x3e, 0x41, 0x5d, 0x55, 0x1e};

// Function to get the glyph data for a given character
static const uint8_t *oled_glyph_for_char(char c) {
    switch (toupper((unsigned char)c)) {
        case 'A': return glyph_A;
        case 'B': return glyph_B;
        case 'C': return glyph_C;
        case 'D': return glyph_D;
        case 'E': return glyph_E;
        case 'F': return glyph_F;
        case 'G': return glyph_G;
        case 'H': return glyph_H;
        case 'I': return glyph_I;
        case 'J': return glyph_J;
        case 'K': return glyph_K;
        case 'L': return glyph_L;
        case 'M': return glyph_M;
        case 'N': return glyph_N;
        case 'O': return glyph_O;
        case 'P': return glyph_P;
        case 'Q': return glyph_Q;
        case 'R': return glyph_R;
        case 'S': return glyph_S;
        case 'T': return glyph_T;
        case 'U': return glyph_U;
        case 'V': return glyph_V;
        case 'W': return glyph_W;
        case 'X': return glyph_X;
        case 'Y': return glyph_Y;
        case 'Z': return glyph_Z;
        case '0': return glyph_0;
        case '1': return glyph_1;
        case '2': return glyph_2;
        case '3': return glyph_3;
        case '4': return glyph_4;
        case '5': return glyph_5;
        case '6': return glyph_6;
        case '7': return glyph_7;
        case '8': return glyph_8;
        case '9': return glyph_9;
        case '.': return glyph_dot;
        case '-': return glyph_dash;
        case '+': return glyph_plus;
        case ':': return glyph_double_dot;
        case '@': return glyph_at;
        case '_': return glyph_low_dash;
        case ' ':
        default: return glyph_space;
    }
}

// Buffer to hold the pixel data for the OLED display (1 byte per 8 vertical pixels)
static uint8_t oled_buffer[OLED_WIDTH * OLED_PAGES];

// Function to clear the OLED buffer
static void oled_clear_buffer(void) {
    // Clear the buffer by setting all bytes to 0 (turning off all pixels)
    memset(oled_buffer, 0x00, sizeof(oled_buffer));
}

// Function to draw a single character at a specific page and column
static void oled_draw_char_page(uint8_t x, uint8_t page, char c) {
    // Check if the page and column are within bounds 
    if (page >= OLED_PAGES || x >= OLED_WIDTH) {
        return;
    }

    // Get the glyph data for the character
    const uint8_t *glyph = oled_glyph_for_char(c);

    // Draw the character by copying the glyph data into the OLED buffer
    for (uint8_t col = 0; col < 5; col++) {
        if (x + col >= OLED_WIDTH) {
            break;
        }
        oled_buffer[page * OLED_WIDTH + x + col] = glyph[col];
    }

    // Add a space column after the character
    if (x + 5 < OLED_WIDTH) {
        oled_buffer[page * OLED_WIDTH + x + 5] = 0x00;
    }
}

// Function to write a string starting at a specific page and column
static void oled_write_string_page(uint8_t x, uint8_t page, const char *str) {
    uint8_t cursor = x;

    // Write characters until we reach the end of the string or run out of space on the page
    while (*str && cursor + 5 < OLED_WIDTH) {
        oled_draw_char_page(cursor, page, *str++);
        cursor = (uint8_t)(cursor + 6);
    }
}

// Function to initialize the OLED display
int initialize_oled(uint32_t i2c_addr, gpio_num_t sda_gpio, gpio_num_t scl_gpio) {
     // Create I2C master bus
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));


    // Create LCD panel IO handle for I2C communication
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = i2c_addr,
        .scl_speed_hz = I2C_CLK_HZ,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_bit_offset = 6,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    // configure panwel handle for the SSD1306 OLED display
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = OLED_HEIGHT,
    };
    panel_config.vendor_config = &ssd1306_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    return 0;
}

// Function to write a string starting at a specific page and column
int write_to_oled(const char *line1, const char *line2) {
    // Write some text to the OLED buffer
    oled_clear_buffer();
    oled_write_string_page(0, 0, line1);
    oled_write_string_page(0, 2, line2);

    // Draw the OLED buffer to the display
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, OLED_WIDTH, OLED_HEIGHT, oled_buffer));

    return 0;
}