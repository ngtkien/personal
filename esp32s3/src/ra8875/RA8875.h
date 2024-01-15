#pragma once
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>

#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log.h>
#include <driver/gpio.h>
#include <driver/spi_common.h>


#include "_settings/RA8875_CPU_commons.h"

#define swapvals_uint8_t(x, y) { uint8_t t = x; x = y;  y = t;}
#define swapvals_uint16_t(x, y) { uint16_t t = x; x = y;  y = t;}
#define swapvals_int16_t(x, y) { int16_t t = x; x = y;  y = t;}

enum RA8875sizes {
	RA8875_480x272,
	RA8875_800x480,
	RA8875_800x480ALT,
	Adafruit_480x272,
	Adafruit_800x480
};
enum RA8875tcursor {
	NOCURSOR = 0, IBEAM, UNDER, BLOCK
};
//0,1,2,3
enum RA8875tsize {
	X16 = 0, X24, X32
};
//0,1,2
enum RA8875fontSource {
	INT = 0, EXT
};
//0,1
enum RA8875fontCoding {
	ISO_IEC_8859_1, ISO_IEC_8859_2, ISO_IEC_8859_3, ISO_IEC_8859_4
};
enum RA8875extRomType {
	GT21L16T1W,
	GT21H16T1W,
	GT23L16U2W,
	GT30L16U2W,
	GT30H24T3Y,
	GT23L24T3Y,
	GT23L24M1Z,
	GT23L32S4W,
	GT30H32S4W,
	GT30L32S4W,
	ER3303_1,
	ER3304_1,
	ER3301_1
};
enum RA8875extRomCoding {
	GB2312, GB12345, BIG5, UNICODE, ASCII, UNIJIS, JIS0208, LATIN
};
enum RA8875extRomFamily {
	STANDARD, ARIAL, ROMAN, BOLD
};
enum RA8875boolean {
	LAYER1, LAYER2, TRANSPARENT, LIGHTEN, OR, AND, FLOATING
};
enum RA8875writes {
	L1 = 0, L2, CGRAM, PATTERN, CURSOR
};
enum RA8875scrollMode {
	SIMULTANEOUS, LAYER1ONLY, LAYER2ONLY, BUFFERED
};
enum RA8875pattern {
	P8X8, P16X16
};
enum RA8875btedatam {
	CONT, RECT
};
enum RA8875btelayer {
	SOURCE, DEST
};
enum RA8875intlist {
	BTE = 1, TOUCH = 2, DMA = 3, KEY = 4
};

/*
 -------------- UNICODE decode (2 byte char) ---------------------
 Latin:      \u0000 -> \u007F	/u00
 Greek:		\u0370 -> \u03FF	/u03
 Cyrillic:   \u0400 -> \u04FF	/u04
 Hebrew:     \u0590 -> \u05FF	/u05
 Arabic: 	\u0600 -> \u06FF	/u06
 Hiragana:	\u3040 -> \u309F	/u30
 Katakana:   \u30A0 -> \u30FF	/u30
 CJK-Uni:	\u4E00 -> \u9FD5	/u4E ... /u9F
 */
/* ----------------------------DO NOT TOUCH ANITHING FROM HERE ------------------------*/
#include "_settings/font.h"
#include "_settings/RA8875Registers.h"
#include "_settings/RA8875ColorPresets.h"
#include "_settings/RA8875UserSettings.h"

#define boolean uint8_t

typedef struct {

	volatile bool _textMode;
	volatile uint8_t _MWCR0_Reg; //keep track of the register 		  [0x40]
	int16_t RA8875_WIDTH, RA8875_HEIGHT; //absolute
	int16_t _width, _height;
	int16_t _cursorX, _cursorY;
	uint8_t _scaleX, _scaleY;bool _scaling;
	uint8_t _FNTwidth, _FNTheight;
	uint8_t _FNTbaselineLow, _FNTbaselineTop;
	volatile uint8_t _TXTparameters;
	/* It contains several parameters in one byte
	 bit			 parameter
	 0	->		_extFontRom 		i's using an ext rom font
	 1	->		_autoAdvance		after a char the pointer move ahead
	 2	->		_textWrap
	 3	->		_fontFullAlig
	 4	->		_fontRotation       (actually not used)
	 5	->		_alignXToCenter;
	 6	->		_alignYToCenter;
	 7	->		_renderFont active;
	 */
	bool _FNTgrandient;
	uint16_t _FNTgrandientColor1;
	uint16_t _FNTgrandientColor2;bool _FNTcompression;
	int _spaceCharWidth;
	volatile bool _needISRrearm;
	volatile uint8_t _enabledInterrups;

	volatile bool _touchEnabled;
	volatile bool _clearTInt;

	volatile bool _needCTS_ISRrearm;

	uint8_t _rst;
	uint8_t _intPin;
	uint8_t _intNum;bool _useISR;
	const tFont * _currentFont;
	uint8_t _maxTouch; //5 on FT5206, 1 on resistive
// FT5206 specifics
	uint8_t _intCTSNum;
	uint8_t _intCTSPin;
	uint8_t _cptRegisters[28];
	uint8_t _gesture;
	uint8_t _currentTouches; //0...5
	uint8_t _currentTouchState; //0,1,2
	//system vars -------------------------------------------
	bool _inited;		//true when init has been ended
	bool _sleep;
	enum RA8875sizes _displaySize;		//Adafruit_480x272, etc
	bool _portrait;
	uint8_t _rotation;
	uint8_t _initIndex;
	int16_t _activeWindowXL, _activeWindowXR, _activeWindowYT, _activeWindowYB;
	uint8_t _errorCode;
	//color vars ----------------------------------------------
	uint16_t _foreColor;
	uint16_t _backColor;bool _backTransparent;
	uint8_t _colorIndex;
	uint16_t _TXTForeColor;
	uint16_t _TXTBackColor;bool _TXTrecoverColor;
	//text vars-------------------------------------------------
	uint8_t _FNTspacing;
	uint8_t _FNTinterline;
	enum RA8875extRomFamily _EXTFNTfamily;
	enum RA8875extRomType _EXTFNTrom;
	enum RA8875extRomCoding _EXTFNTcoding;
	enum RA8875tsize _EXTFNTsize;		//X16,X24,X32
	enum RA8875fontSource _FNTsource;
	enum RA8875tcursor _FNTcursorType;
	//centering -------------------------------
	bool _relativeCenter;bool _absoluteCenter;
	//layer vars -----------------------------
	uint8_t _maxLayers;bool _useMultiLayers;
	uint8_t _currentLayer;bool _hasLayerLimits;		//helper
	//scroll vars ----------------------------
	int16_t _scrollXL, _scrollXR, _scrollYT, _scrollYB;
	//color space-----------------------------
	uint8_t _color_bpp;		//8=256, 16=64K colors
	uint8_t _brightness;
	//various
	float _arcAngle_max;
	int _arcAngle_offset;
	int _angle_offset;
	// Register containers -----------------------------------------
	// this needed to  prevent readRegister from chip that it's slow.

	uint8_t _DPCR_Reg;  ////Display Configuration		  	  [0x20]
	uint8_t _FNCR0_Reg; //Font Control Register 0 		  	  [0x21]
	uint8_t _FNCR1_Reg; //Font Control Register1 			  [0x22]
	uint8_t _FWTSET_Reg; //Font Write Type Setting Register   [0x2E]
	uint8_t _SFRSET_Reg; //Serial Font ROM Setting 		  	  [0x2F]
	uint8_t _INTC1_Reg; //Interrupt Control Register1		  [0xF0]
	bool _keyMatrixEnabled;
} RA8875_struct;

//		Defines
#define PI 3.1415
#define false 0
#define true 1
#define bitWrite(v, bit, val) { if (val) { v = v | (1<<bit);} else { v = v & (0xff - (1 <<bit));}}
#define bitRead(v, bit) ((v >> bit) & 1)

// Functions in the C-code
uint8_t _color16To8bpp(uint16_t color);
void Color565ToRGB(uint16_t color, uint8_t *r, uint8_t *g, uint8_t *b);
uint16_t Color565(uint8_t r, uint8_t g, uint8_t b);
uint16_t Color24To565(int32_t color_);
uint16_t htmlTo565(int32_t color_);
float RA8875_cosDeg_helper(float angle);
float RA8875_sinDeg_helper(float angle);
void RA8875_checkLimits_helper(RA8875_struct *ra8875, int16_t *x, int16_t *y);
void RA8875_begin(RA8875_struct *ra8875, const enum RA8875sizes s,
		uint8_t colors);
void RA8875_HW_initialize(RA8875_struct *ra8875);
void RA8875__setSysClock(uint8_t pll1, uint8_t pll2, uint8_t pixclk);
void RA8875_getCursorFast(RA8875_struct *ra8875, int16_t *x, int16_t *y);
int16_t RA8875_getCursorX(RA8875_struct *ra8875);
int16_t RA8875_getCursorY(RA8875_struct *ra8875);
void RA8875_setCursor(RA8875_struct *ra8875, int16_t x, int16_t y,
bool autocenter);
void RA8875_setForegroundColor(RA8875_struct *ra8875, uint16_t color);
void RA8875_setForegroundColorRGB(RA8875_struct *ra8875, uint8_t R, uint8_t G,
		uint8_t B);
void RA8875_setBackgroundColor(RA8875_struct *ra8875, uint16_t color);
void RA8875_setBackgroundColorRGB(RA8875_struct *ra8875, uint8_t R, uint8_t G,
		uint8_t B);
void RA8875_setTextMode(RA8875_struct *ra8875, bool m);
void RA8875_displayOn(RA8875_struct *ra8875, boolean on);
void RA8875_setRotation(RA8875_struct *ra8875, uint8_t rotation);
void RA8875_scanDirection(RA8875_struct *ra8875, boolean invertH,
boolean invertV);
void RA8875_setActiveWindowXXYY(RA8875_struct *ra8875, int16_t XL, int16_t XR,
		int16_t YT, int16_t YB);
void RA8875_setActiveWindow(RA8875_struct *ra8875);
void RA8875_updateActiveWindow(RA8875_struct *ra8875,
bool full);
void RA8875_textPosition(RA8875_struct *ra8875, int16_t x, int16_t y,
bool update);
void RA8875_backlight(RA8875_struct *ra8875, boolean on);
void RA8875_fillWindow(RA8875_struct *ra8875, uint16_t color);
void RA8875_line_addressing(RA8875_struct *ra8875, int16_t x0, int16_t y0,
		int16_t x1, int16_t y1);
void RA8875_clearMemory(RA8875_struct *ra8875, bool stop);
void RA8875_setFont(RA8875_struct *ra8875, enum RA8875fontSource s);
void RA8875_setFontSize(RA8875_struct *ra8875, enum RA8875tsize ts);
void RA8875_setFNTdimensions(RA8875_struct *ra8875, uint8_t index);
void RA8875_setIntFontCoding(RA8875_struct *ra8875, enum RA8875fontCoding f);
void RA8875_setCursorBlinkRate(RA8875_struct *ra8875, uint8_t rate);
void RA8875_setTextColor(RA8875_struct *ra8875, uint16_t fcolor,
		uint16_t bcolor);
void RA8875_setTextColorForeground(RA8875_struct *ra8875, uint16_t fcolor);
void RA8875_charWriteR(RA8875_struct *ra8875, const char c, uint8_t offset,
		uint16_t fcolor, uint16_t bcolor);
void RA8875_charWrite(RA8875_struct *ra8875, const char c, uint8_t offset);
int RA8875_getCharCode(RA8875_struct *ra8875, uint8_t ch);
void RA8875_fillRect(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w,
		int16_t h, uint16_t color);
void RA8875_drawPixel(RA8875_struct *ra8875, int16_t x, int16_t y,
		uint16_t color);
void RA8875_rect_helper(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w,
		int16_t h, uint16_t color,
		bool filled);
void RA8875_setXY(RA8875_struct *ra8875, int16_t x, int16_t y);
void RA8875_setX(RA8875_struct *ra8875, int16_t x);
void RA8875_setY(RA8875_struct *ra8875, int16_t y);
void RA8875_drawRect(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w,
		int16_t h, uint16_t color);
uint16_t RA8875_width(RA8875_struct *ra8875, bool absolute);
uint16_t RA8875_height(RA8875_struct *ra8875, bool absolute);
void RA8875_uploadUserChar(RA8875_struct *ra8875, const uint8_t symbol[],
		uint8_t address);
void RA8875_showUserChar(RA8875_struct *ra8875, uint8_t symbolAddrs,
		uint8_t wide);
void RA8875_textWrite(RA8875_struct *ra8875, const char* buffer, uint16_t len);
int16_t RA8875_STRlen_helper(RA8875_struct *ra8875, const char* buffer,
		uint16_t len);
void RA8875_drawChar_unc(RA8875_struct *ra8875, int16_t x, int16_t y, int charW,
		int index, uint16_t fcolor);
void RA8875_charLineRender(RA8875_struct *ra8875,
bool lineBuffer[], int charW, int16_t x, int16_t y, int16_t currentYposition,
		uint16_t fcolor);
uint16_t RA8875_colorInterpolation(uint16_t color1, uint16_t color2,
		uint16_t pos, uint16_t div);
uint16_t RA8875_colorInterpolationRGB(uint8_t r1, uint8_t g1, uint8_t b1,
		uint8_t r2, uint8_t g2, uint8_t b2, uint16_t pos, uint16_t div);
void RA8875_setFontExt(RA8875_struct *ra8875, const tFont *font);
void RA8875_setFontFullAlign(RA8875_struct *ra8875, boolean align);
void RA8875_setFontScale(RA8875_struct *ra8875, uint8_t scale);
void RA8875_setFontScaleXY(RA8875_struct *ra8875, uint8_t xscale,
		uint8_t yscale);
void RA8875_roundRect_helper(RA8875_struct *ra8875, int16_t x, int16_t y,
		int16_t w, int16_t h, int16_t r, uint16_t color, bool filled);
void RA8875_drawRoundRect(RA8875_struct *ra8875, int16_t x, int16_t y,
		int16_t w, int16_t h, int16_t r, uint16_t color);
void RA8875_fillRoundRect(RA8875_struct *ra8875, int16_t x, int16_t y,
		int16_t w, int16_t h, int16_t r, uint16_t color);
void RA8875_fillCircle(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t r,
		uint16_t color);
void RA8875_drawEllipse(RA8875_struct *ra8875, int16_t xCenter, int16_t yCenter, int16_t longAxis,
        int16_t shortAxis, uint16_t color);
void RA8875_fillEllipse(RA8875_struct *ra8875, int16_t xCenter, int16_t yCenter, int16_t longAxis,
        int16_t shortAxis, uint16_t color);
void RA8875_circle_helper(RA8875_struct *ra8875, int16_t x0, int16_t y0,
		int16_t r, uint16_t color,
		bool filled);
void RA8875_curve_addressing(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void RA8875_ellipseCurve_helper(RA8875_struct *ra8875, int16_t xCenter, int16_t yCenter,
		int16_t longAxis, int16_t shortAxis, uint8_t curvePart, uint16_t color, bool filled);
void RA8875_drawArc(RA8875_struct *ra8875, uint16_t cx, uint16_t cy,
		uint16_t radius, uint16_t thickness, float start, float end,
		uint16_t color);
void RA8875_drawArc_helper(RA8875_struct *ra8875, uint16_t cx, uint16_t cy,
		uint16_t radius, uint16_t thickness, float start, float end,
		uint16_t color);
void RA8875_drawFastVLine(RA8875_struct *ra8875, int16_t x, int16_t y,
		int16_t h, uint16_t color);
void RA8875_drawFastHLine(RA8875_struct *ra8875, int16_t x, int16_t y,
		int16_t w, uint16_t color);
void RA8875_drawLine(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t x1,
		int16_t y1, uint16_t color);
void RA8875_writeTo(RA8875_struct *ra8875, enum RA8875writes d);
void RA8875_useLayers(RA8875_struct *ra8875, boolean on);
void RA8875_setColorBpp(RA8875_struct *ra8875, uint8_t colors);
uint8_t RA8875_getColorBpp(RA8875_struct *ra8875);
void RA8875_clearActiveWindow(RA8875_struct *ra8875, bool full);

void RA8875_clearMemory(RA8875_struct *ra8875, bool stop);
void RA8875_drawTriangle(RA8875_struct *ra8875,int16_t x0, int16_t y0, int16_t x1, int16_t y1,
        int16_t x2, int16_t y2, uint16_t color);
void RA8875_fillTriangle(RA8875_struct *ra8875,int16_t x0, int16_t y0, int16_t x1, int16_t y1,
        int16_t x2, int16_t y2, uint16_t color);
void RA8875_triangle_helper(RA8875_struct *ra8875, int16_t x0, int16_t y0,
        int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color,
        bool filled) ;
void RA8875_setExtFontFamily(RA8875_struct *ra8875, enum RA8875extRomFamily erf, boolean setReg);

// lowlevel.c

void RA8875_writeData(uint8_t data);
void RA8875_writeCommand(uint8_t data);
void RA8875_PWMsetup(uint8_t pw, uint8_t on, uint8_t clock);
void RA8875_PWMout(uint8_t pw, uint8_t p);
uint8_t RA8875_readData(uint8_t stat);
void RA8875_writeRegister(uint8_t reg, uint8_t val);
uint8_t RA8875_readRegister(const uint8_t reg);
void RA8875_writeData16(uint16_t data);
uint8_t RA8875_waitPoll(uint8_t regname, uint8_t waitflag);
void RA8875_waitBusy(uint8_t res);
void RAA8875_perFormHwReset();
