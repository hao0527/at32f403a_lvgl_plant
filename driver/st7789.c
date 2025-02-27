/**
 * @file st7789.c
 *
 * Mostly taken from lbthomsen/esp-idf-littlevgl github.
 */

/*********************
 *      INCLUDES
 *********************/
#include "st7789.h"
#include "lv_mcu_driver.h"
#include "at32f403a_407_gpio.h"
#include "my_delay.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "st7789"

#define ST7789_USE_RST                (1)
#define ST7789_INVERT_COLORS          (0)	// tao:反显
#define CONFIG_LV_DISPLAY_ORIENTATION (2)	// tao:0/1/2/3

/**********************
 *      TYPEDEFS
 **********************/

/*The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct. */
typedef struct {
	uint8_t cmd;
	uint8_t data[16];
	uint8_t databytes;    // No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void st7789_set_orientation(uint8_t orientation);
static void st7789_send_color(void *data, size_t length);
static void st7789_send_cmd(uint8_t cmd);
static void st7789_send_data(void *data, uint16_t length);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void st7789_init(void)
{
	/* clang-format off */
	lcd_init_cmd_t st7789_init_cmds[] = {
		{0xCF, {0x00, 0x83, 0X30}, 3},
		{0xED, {0x64, 0x03, 0X12, 0X81}, 4},
		{ST7789_PWCTRL2, {0x85, 0x01, 0x79}, 3},
		{0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
		{0xF7, {0x20}, 1},
		{0xEA, {0x00, 0x00}, 2},
		{ST7789_LCMCTRL, {0x26}, 1},
		{ST7789_IDSET, {0x11}, 1},
		{ST7789_VCMOFSET, {0x35, 0x3E}, 2},
		{ST7789_CABCCTRL, {0xBE}, 1},
		{ST7789_MADCTL, {0x00}, 1}, // Set to 0x28 if your display is flipped
		{ST7789_COLMOD, {0x55}, 1},

#if ST7789_INVERT_COLORS == 1
		{ST7789_INVON, {0}, 0}, // set inverted mode
#else
 		{ST7789_INVOFF, {0}, 0}, // set non-inverted mode
#endif

		{ST7789_RGBCTRL, {0x00, 0x1B}, 2},
		{0xF2, {0x08}, 1},
		{ST7789_GAMSET, {0x01}, 1},
		{ST7789_PVGAMCTRL, {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0E, 0x12, 0x14, 0x17}, 14},
		{ST7789_NVGAMCTRL, {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x31, 0x54, 0x47, 0x0E, 0x1C, 0x17, 0x1B, 0x1E}, 14},
		{ST7789_CASET, {0x00, 0x00, 0x00, 0xEF}, 4},
		{ST7789_RASET, {0x00, 0x00, 0x01, 0x3f}, 4},
		{ST7789_RAMWR, {0}, 0},
		{ST7789_GCTRL, {0x07}, 1},
		{0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
		{ST7789_SLPOUT, {0}, 0x80},
		{ST7789_DISPON, {0}, 0x80},
		{0, {0}, 0xff},
	};
	/* clang-format on */

	// Initialize non-SPI GPIOs
	// 外部已经初始化过了，这里不用再初始化GPIO
	LV_GPIO_SET(ST7789_BL_PORT, ST7789_BL_PIN);   // 打开背光

	// Reset the display
#if !defined(ST7789_SOFT_RST)
	// Reset the display
	LV_GPIO_CLR(ST7789_RST_PORT, ST7789_RST_PIN);
	delay_ms(100);
	LV_GPIO_SET(ST7789_RST_PORT, ST7789_RST_PIN);
	delay_ms(100);
#else
	st7789_send_cmd(ST7789_SWRESET);
#endif

	// Send all the commands
	uint16_t cmd = 0;
	while (st7789_init_cmds[cmd].databytes != 0xff) {
		st7789_send_cmd(st7789_init_cmds[cmd].cmd);
		st7789_send_data(st7789_init_cmds[cmd].data, st7789_init_cmds[cmd].databytes & 0x1F);
		if (st7789_init_cmds[cmd].databytes & 0x80) {
			delay_ms(100);
		}
		cmd++;
	}

	st7789_set_orientation(CONFIG_LV_DISPLAY_ORIENTATION);
}

/* The ST7789 display controller can drive up to 320*240 displays, when using a 240*240 or 240*135
 * displays there's a gap of 80px or 40/52/53px respectively. 52px or 53x offset depends on display orientation.
 * We need to edit the coordinates to take into account those gaps, this is not necessary in all orientations. */
void st7789_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
	uint8_t data[4] = {0};

	uint16_t offsetx1 = area->x1;
	uint16_t offsetx2 = area->x2;
	uint16_t offsety1 = area->y1;
	uint16_t offsety2 = area->y2;

#if (CONFIG_LV_TFT_DISPLAY_OFFSETS)
	offsetx1 += CONFIG_LV_TFT_DISPLAY_X_OFFSET;
	offsetx2 += CONFIG_LV_TFT_DISPLAY_X_OFFSET;
	offsety1 += CONFIG_LV_TFT_DISPLAY_Y_OFFSET;
	offsety2 += CONFIG_LV_TFT_DISPLAY_Y_OFFSET;

#elif (LV_HOR_RES_MAX == 240) && (LV_VER_RES_MAX == 240)
	#if (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT)
	offsetx1 += 80;
	offsetx2 += 80;
	#elif (CONFIG_LV_DISPLAY_ORIENTATION_LANDSCAPE_INVERTED)
	offsety1 += 80;
	offsety2 += 80;
	#endif
#elif (LV_HOR_RES_MAX == 240) && (LV_VER_RES_MAX == 135)
	#if (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT) || \
		(CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT_INVERTED)
	offsetx1 += 40;
	offsetx2 += 40;
	offsety1 += 53;
	offsety2 += 53;
	#endif
#elif (LV_HOR_RES_MAX == 135) && (LV_VER_RES_MAX == 240)
	#if (CONFIG_LV_DISPLAY_ORIENTATION_LANDSCAPE) || \
		(CONFIG_LV_DISPLAY_ORIENTATION_LANDSCAPE_INVERTED)
	offsetx1 += 52;
	offsetx2 += 52;
	offsety1 += 40;
	offsety2 += 40;
	#endif
#endif

	/*Column addresses*/
	st7789_send_cmd(ST7789_CASET);
	data[0] = (offsetx1 >> 8) & 0xFF;
	data[1] = offsetx1 & 0xFF;
	data[2] = (offsetx2 >> 8) & 0xFF;
	data[3] = offsetx2 & 0xFF;
	st7789_send_data(data, 4);

	/*Page addresses*/
	st7789_send_cmd(ST7789_RASET);
	data[0] = (offsety1 >> 8) & 0xFF;
	data[1] = offsety1 & 0xFF;
	data[2] = (offsety2 >> 8) & 0xFF;
	data[3] = offsety2 & 0xFF;
	st7789_send_data(data, 4);

	/*Memory write*/
	st7789_send_cmd(ST7789_RAMWR);

	size_t size = (size_t)lv_area_get_width(area) * (size_t)lv_area_get_height(area);

	st7789_send_color((void *)color_map, size * 2);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void st7789_send_cmd(uint8_t cmd)
{
	while(!lv_mcu_spiIsReady());
	LV_GPIO_CLR(ST7789_DC_PORT, ST7789_DC_PIN); /*Command mode*/
	lv_mcu_spiSendData(&cmd, 1);
}

static void st7789_send_data(void *data, uint16_t length)
{
	while(!lv_mcu_spiIsReady());
	LV_GPIO_SET(ST7789_DC_PORT, ST7789_DC_PIN); /*Data mode*/
	lv_mcu_spiSendData(data, length);
}

static void st7789_send_color(void *data, size_t length)
{
	while(!lv_mcu_spiIsReady());
	LV_GPIO_SET(ST7789_DC_PORT, ST7789_DC_PIN); /*Data mode*/
	lv_mcu_spiSendColor(data, length);
}

static void st7789_set_orientation(uint8_t orientation)
{
	uint8_t data[] =
	{
#if CONFIG_LV_PREDEFINED_DISPLAY_TTGO
		0x60,
		0xA0,
		0x00,
		0xC0
#else
		0xC0,
		0x00,
		0x60,
		0xA0
#endif
	};

	st7789_send_cmd(ST7789_MADCTL);
	st7789_send_data((void *)&data[orientation], 1);
}
