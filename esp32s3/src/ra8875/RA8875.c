#include "RA8875.h"
#include <driver/gpio.h>
#include <driver/spi_common.h>
LOG_MODULE_REGISTER(ra8875);

#define RA8875_RS DT_ALIAS(ra8875_rs)
#define RA8875_NODE DT_NODELABEL(ra8875)
/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
const struct gpio_dt_spec ra8875_rs = GPIO_DT_SPEC_GET(RA8875_RS, gpios);
const struct device *display = DEVICE_DT_GET(RA8875_NODE);


/*
 Part of RA8875 library from https://github.com/sumotoy/RA8875
 License:GNU General Public License v3.0

 RA8875 fast SPI library for RAiO SPI RA8875 drived TFT
 Copyright (C) 2014  egidio massimo costa sumotoy (a t) gmail.com

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 * @brief Contains the low level graphics functions for the display
 *
 */

/**
 * @addtogroup display
 *
 * @{
 */



#include "RA8875.h"
#include "math.h"
#include "string.h"

void Display_perFormHwReset() {
    gpio_pin_set_dt(&ra8875_rs, 1);
    k_msleep(10);
    gpio_pin_set_dt(&ra8875_rs, 0);
}   

/**
 * @brief Color conversion from 16 bytes to 8 bytes
 *
 * @param color - the 16 bit color
 * @return the 8 bit accoring color
 */
uint8_t _color16To8bpp(uint16_t color) {
	return ((color & 0x3800) >> 6 | (color & 0x00E0) >> 3 | (color & 0x0003));
}

void Color565ToRGB(uint16_t color, uint8_t *r, uint8_t *g, uint8_t *b) {
	*r = (((color & 0xF800) >> 11) * 527 + 23) >> 6;
	*g = (((color & 0x07E0) >> 5) * 259 + 33) >> 6;
	*b = ((color & 0x001F) * 527 + 23) >> 6;
}
uint16_t Color565(uint8_t r, uint8_t g, uint8_t b) {
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
uint16_t Color24To565(int32_t color_) {
	return ((((color_ >> 16) & 0xFF) / 8) << 11) | ((((color_ >> 8) & 0xFF) / 4) << 5) | (((color_) & 0xFF) / 8);
}
uint16_t htmlTo565(int32_t color_) {
	return (uint16_t) (((color_ & 0xF80000) >> 8) | ((color_ & 0x00FC00) >> 5) | ((color_ & 0x0000F8) >> 3));
}
/**************************************************************************/
/*!
 sin e cos helpers
 [private]
 */
/**************************************************************************/
#define PI 3.1415
float RA8875_cosDeg_helper(float angle) {
	float radians = angle / (float) 360 * 2 * PI;
	return cos(radians);
}

float RA8875_sinDeg_helper(float angle) {
	float radians = angle / (float) 360 * 2 * PI;
	return sin(radians);
}
void RA8875_checkLimits_helper(RA8875_struct *ra8875, int16_t *x, int16_t *y) {
	if (*x < 0)
		*x = 0;
	if (*y < 0)
		*y = 0;
	if (*x >= ra8875->RA8875_WIDTH)
		*x = ra8875->RA8875_WIDTH - 1;
	if (*y >= ra8875->RA8875_HEIGHT)
		*y = ra8875->RA8875_HEIGHT - 1;
}
static volatile uint8_t _RA8875_INTS = 0b00000000; //container for INT states
/*------------------------------
 Bit:	Called by:		In use:
 --------------------------------
 0: 		isr triggered	[*]
 1: 		Resistive TS	[*]
 2: 		KeyScan			[*]
 3: 		DMA
 4: 		BTE
 5: 		FT5206 TS		[*]
 6: 		-na-
 7: 		-na-
 --------------------------------*/

/**************************************************************************/
/*!
 Initialize library and SPI
 Parameter:
 (display type)
 RA8875_480x272 (4.3" displays)
 RA8875_800x480 (5" and 7" displays)
 Adafruit_480x272 (4.3" Adafruit displays)
 Adafruit_800x480 (5" and 7" Adafruit displays)
 (colors) - The color depth (default 16) 8 or 16 (bit)
 -------------------------------------------------------------
 UPDATE! in Energia IDE some devices needs an extra parameter!
 module: sets the SPI interface (it depends from MCU). Default:0
 */
/**************************************************************************/
void RA8875_begin(RA8875_struct *ra8875, const enum RA8875sizes s, uint8_t colors) {
	ra8875->_errorCode = 0;
	ra8875->_displaySize = s;
	ra8875->_rotation = 0;
	ra8875->_portrait = false;
	ra8875->_inited = false;
	ra8875->_sleep = false;
	ra8875->_hasLayerLimits = false;
	ra8875->_intPin = 255;
	ra8875->_intNum = 0;
	ra8875->_useISR = false;
	ra8875->_enabledInterrups = 0b00000000;
	/* used to understand wat causes an INT
	 bit
	 0:
	 1:
	 2: Touch (resistive)
	 3:
	 4:
	 5:
	 6:
	 7:
	 8:
	 */
	ra8875->_intCTSPin = 255;
	ra8875->_intCTSNum = 0;
	ra8875->_TXTForeColor = _RA8875_DEFAULTTXTFRGRND;
	ra8875->_TXTBackColor = _RA8875_DEFAULTTXTBKGRND;
	ra8875->_TXTrecoverColor = false;
	ra8875->_maxLayers = 2;
	ra8875->_currentLayer = 0;
	ra8875->_useMultiLayers = false; //starts with one layer only
	ra8875->_textMode = false;
	ra8875->_brightness = 255;
	ra8875->_cursorX = 0;
	ra8875->_cursorY = 0;
	ra8875->_scrollXL = 0;
	ra8875->_scrollXR = 0;
	ra8875->_scrollYT = 0;
	ra8875->_scrollYB = 0;
	ra8875->_scaleX = 1;
	ra8875->_scaleY = 1;
	ra8875->_scaling = false;
	ra8875->_EXTFNTsize = X16;
	ra8875->_FNTspacing = 0;
	//_FNTrender = false;
	/* set-->  _TXTparameters  <--
	 0:_extFontRom = false;
	 1:_autoAdvance = true;
	 2:_textWrap = user defined
	 3:_fontFullAlig = false;
	 4:_fontRotation = false;//not used
	 5:_alignXToCenter = false;
	 6:_alignYToCenter = false;
	 7: render         = false;
	 */
	ra8875->_TXTparameters = 0b00000010;
	bitWrite(ra8875->_TXTparameters, 2, _DFT_RA8875_TEXTWRAP); //set _textWrap
	ra8875->_relativeCenter = false;
	ra8875->_absoluteCenter = false;
	ra8875->_EXTFNTrom = _DFT_RA8875_EXTFONTROMTYPE;
	ra8875->_EXTFNTcoding = _DFT_RA8875_EXTFONTROMCODING;
	//_FNTsource = INT;
	ra8875->_FNTinterline = 0;
	ra8875->_EXTFNTfamily = STANDARD;
	ra8875->_FNTcursorType = NOCURSOR;
	ra8875->_FNTgrandient = false;
	ra8875->_arcAngle_max = ARC_ANGLE_MAX;
	ra8875->_arcAngle_offset = ARC_ANGLE_OFFSET;
	ra8875->_angle_offset = ANGLE_OFFSET;
	ra8875->_color_bpp = 16;
	ra8875->_colorIndex = 0;

	if (colors != 16) {
		ra8875->_color_bpp = 8;
		ra8875->_colorIndex = 3;
	}

	switch (ra8875->_displaySize) {
	case RA8875_480x272:
	case Adafruit_480x272:
		ra8875->_width = 480;
		ra8875->_height = 272;
		ra8875->_initIndex = 0;
		break;
	case RA8875_800x480:
	case Adafruit_800x480:
	case RA8875_800x480ALT:
		ra8875->_width = 800;
		ra8875->_height = 480;
		ra8875->_hasLayerLimits = true;
		ra8875->_maxLayers = 1;
		if (ra8875->_color_bpp < 16)
			ra8875->_maxLayers = 2;
		ra8875->_initIndex = 1;
		if (ra8875->_displaySize == RA8875_800x480ALT)
			ra8875->_initIndex = 2;
		break;
	default:
		ra8875->_errorCode |= (1 << 0); //set
		return;
	}
	ra8875->RA8875_WIDTH = ra8875->_width;
	ra8875->RA8875_HEIGHT = ra8875->_height;
	ra8875->_activeWindowXL = 0;
	ra8875->_activeWindowXR = ra8875->RA8875_WIDTH;
	ra8875->_activeWindowYT = 0;
	ra8875->_activeWindowYB = ra8875->RA8875_HEIGHT;
#if !defined(_AVOID_TOUCHSCREEN)//common to all touch
	ra8875->_clearTInt = false;
	ra8875->_touchEnabled = false;
#endif

	ra8875->_keyMatrixEnabled = false;
	/* Display Configuration Register	  [0x20]
	 7: (Layer Setting Control) 0:one Layer, 1:two Layers
	 6,5,4: (na)
	 3: (Horizontal Scan Direction) 0: SEG0 to SEG(n-1), 1: SEG(n-1) to SEG0
	 2: (Vertical Scan direction) 0: COM0 to COM(n-1), 1: COM(n-1) to COM0
	 1,0: (na) */
	ra8875->_DPCR_Reg = 0b00000000;
	/*	Memory Write Control Register 0 [0x40]
	 7: 0(graphic mode), 1(textx mode)
	 6: 0(font-memory cursor not visible), 1(visible)
	 5: 0(normal), 1(blinking)
	 4: na
	 3-2: 00(LR,TB), 01(RL,TB), 10(TB,LR), 11(BT,LR)
	 1: 0(Auto Increase in write), 1(no)
	 0: 0(Auto Increase in read), 1(no) */
	ra8875->_MWCR0_Reg = 0b00000000;

	/*	Font Control Register 0 [0x21]
	 7: 0(CGROM font is selected), 1(CGRAM font is selected)
	 6: na
	 5: 0(Internal CGROM [reg 0x2F to 00]), 1(External CGROM [0x2E reg, bit6,7 to 0)
	 4-2: na
	 1-0: 00(ISO/IEC 8859-1), 01(ISO/IEC 8859-2), 10(ISO/IEC 8859-3), 11(ISO/IEC 8859-4)*/

	ra8875->_FNCR0_Reg = 0b00000000;
	/*	Font Control Register 1 [0x22]
	 7: 0(Full Alignment off), 1(Full Alignment on)
	 6: 0(no-trasparent), 1(trasparent)
	 5: na
	 4: 0(normal), 1(90degrees)
	 3-2: 00(x1), 01(x2), 10(x3), 11(x3) Horizontal font scale
	 1-0: 00(x1), 01(x2), 10(x3), 11(x3) Vertical font scale */
	ra8875->_FNCR1_Reg = 0b00000000;

	/*	Font Write Type Setting Register [0x2E]
	 7-6: 00(16x16,8x16,nx16), 01(24x24,12x24,nx24), 1x(32x32,16x32, nx32)
	 5-0: 00...3F (font width off to 63 pixels)*/
	ra8875->_FWTSET_Reg = 0b00000000;

	/*	Serial Font ROM Setting [0x2F]
	 GT Serial Font ROM Select
	 7-5: 000(GT21L16TW/GT21H16T1W),001(GT30L16U2W),010(GT30L24T3Y/GT30H24T3Y),011(GT30L24M1Z),111(GT30L32S4W/GT30H32S4W)
	 FONT ROM Coding Setting
	 4-2: 000(GB2312),001(GB12345/GB18030),010(BIG5),011(UNICODE),100(ASCII),101(UNI-Japanese),110(JIS0208),111(Latin/Greek/Cyrillic/Arabic)
	 1-0: 00...11
	 bits	ASCII		Lat/Gr/Cyr		Arabic
	 00		normal		normal			na
	 01		Arial		var Wdth		Pres Forms A
	 10		Roman		na				Pres Forms B
	 11		Bold		na				na */
	ra8875->_SFRSET_Reg = 0b00000000;

	/*	Interrupt Control Register1		  [0xF0]
	 7,6,5: (na)
	 4: KEYSCAN Interrupt Enable Bit
	 3: DMA Interrupt Enable Bit
	 2: TOUCH Panel Interrupt Enable Bit
	 1: BTE Process Complete Interrupt Enable Bit
	 0:
	 When MCU-relative BTE operation is selected(*1) and BTE
	 Function is Enabled(REG[50h] Bit7 = 1), this bit is used to
	 Enable the BTE Interrupt for MCU R/W:
	 0 : Disable BTE interrupt for MCU R/W.
	 1 : Enable BTE interrupt for MCU R/W.
	 When the BTE Function is Disabled, this bit is used to
	 Enable the Interrupt of Font Write Function:
	 0 : Disable font write interrupt.
	 1 : Enable font write interrupt.
	 */
	ra8875->_INTC1_Reg = 0b00000000;

	if (ra8875->_errorCode != 0)
		return; //ouch, error/s.Better stop here! 


    if (!device_is_ready(display)) {
        LOG_ERR("display device is not ready");
        return;
    }

    LOG_INF("display device is ready");

    if (!gpio_is_ready_dt(&ra8875_rs)) {
        LOG_ERR("ra8875_cs device is not ready");
		return;
	}
    int ret;
    ret = gpio_pin_configure_dt(&ra8875_rs, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
        LOG_ERR("ra8875_cs device configure fail");
		return;
	}
    LOG_INF("ra8875_cs device is ready");

	RA8875_HW_initialize(ra8875); //----->Time to Initialize the RA8875!

    
 
	//------- time for capacitive touch stuff -----------------
	// // vTaskDelay(10);
	ra8875->_maxTouch = 5;
	ra8875->_gesture = 0;
	ra8875->_currentTouches = 0;
	ra8875->_currentTouchState = 0;
	ra8875->_needISRrearm = false;
	//TO DO
}

/************************* Initialization *********************************/

/**************************************************************************/
/*!
 Hardware initialization of RA8875 and turn on
 [private]
 */
/**************************************************************************/
void RA8875_HW_initialize(RA8875_struct *ra8875) {
	Display_perFormHwReset();

	ra8875->_inited = false;
	if (ra8875->_rst == 255) {		// soft reset time ?
		RA8875_writeCommand(RA8875_PWRR);
		RA8875_writeData(RA8875_PWRR_SOFTRESET);
		// // vTaskDelay(20);
		RA8875_writeData(RA8875_PWRR_NORMAL);
		// // vTaskDelay(200);
	}
	//set the sysClock
	RA8875__setSysClock(initStrings[ra8875->_initIndex][0], initStrings[ra8875->_initIndex][1],
			initStrings[ra8875->_initIndex][2]);

	//color space setup
	if (ra8875->_color_bpp < 16) {		//256
		RA8875_writeRegister(RA8875_SYSR, 0x00);		//256
		ra8875->_colorIndex = 3;
	} else {
		RA8875_writeRegister(RA8875_SYSR, 0x0C);		//65K
		ra8875->_colorIndex = 0;
	}

	RA8875_writeRegister(RA8875_HDWR, initStrings[ra8875->_initIndex][3]);		//LCD Horizontal Display Width Register
	RA8875_writeRegister(RA8875_HNDFTR, initStrings[ra8875->_initIndex][4]);//Horizontal Non-Display Period Fine Tuning Option Register
	RA8875_writeRegister(RA8875_HNDR, initStrings[ra8875->_initIndex][5]);	//LCD Horizontal Non-Display Period Register
	RA8875_writeRegister(RA8875_HSTR, initStrings[ra8875->_initIndex][6]);		//HSYNC Start Position Register
	RA8875_writeRegister(RA8875_HPWR, initStrings[ra8875->_initIndex][7]);		//HSYNC Pulse Width Register
	RA8875_writeRegister(RA8875_VDHR0, initStrings[ra8875->_initIndex][8]);	//LCD Vertical Display Height Register0
	RA8875_writeRegister(RA8875_VDHR0 + 1, initStrings[ra8875->_initIndex][9]);	//LCD Vertical Display Height Register1
	RA8875_writeRegister(RA8875_VNDR0, initStrings[ra8875->_initIndex][10]);//LCD Vertical Non-Display Period Register 0
	RA8875_writeRegister(RA8875_VNDR0 + 1, initStrings[ra8875->_initIndex][11]);//LCD Vertical Non-Display Period Register 1
	RA8875_writeRegister(RA8875_VSTR0, initStrings[ra8875->_initIndex][12]);	//VSYNC Start Position Register 0
	RA8875_writeRegister(RA8875_VSTR0 + 1, initStrings[ra8875->_initIndex][13]);	//VSYNC Start Position Register 1
	RA8875_writeRegister(RA8875_VPWR, initStrings[ra8875->_initIndex][14]);	//VSYNC Pulse Width Register
	RA8875_updateActiveWindow(ra8875, true);	//set the whole screen as active
	//clearActiveWindow();
	// // vTaskDelay(10);	//100
	RA8875_setCursorBlinkRate(ra8875, DEFAULTCURSORBLINKRATE);	//set default blink rate
	RA8875_setIntFontCoding(ra8875, DEFAULTINTENCODING);	//set default internal font encoding
	RA8875_setFont(ra8875, INT);	//set internal font use
	//postburner PLL!
	RA8875__setSysClock(sysClockPar[ra8875->_initIndex][0], sysClockPar[ra8875->_initIndex][1],
			initStrings[ra8875->_initIndex][2]);
	ra8875->_inited = true;	//from here we will go at high speed!

	// // vTaskDelay(1);
	RA8875_clearMemory(ra8875, false);	//clearMemory(true);
	// // vTaskDelay(1);
	RA8875_displayOn(ra8875, true);	//turn On Display
	// // vTaskDelay(1);
	RA8875_fillWindow(ra8875, _RA8875_DEFAULTBACKLIGHT);	//set screen black
	RA8875_backlight(ra8875, true);
	RA8875_setRotation(ra8875, _RA8875_DEFAULTSCRROT);
	RA8875_setTextMode(ra8875, false);
	RA8875_setActiveWindow(ra8875);
	RA8875_setForegroundColor(ra8875, ra8875->_TXTForeColor);
	RA8875_setBackgroundColor(ra8875, ra8875->_TXTBackColor);
	ra8875->_backTransparent = false;
	ra8875->_FNCR1_Reg &= ~(1 << 6);	//clear
	RA8875_writeRegister(RA8875_FNCR1, ra8875->_FNCR1_Reg);

	RA8875_setCursor(ra8875, 0, 0, false);
}

/**************************************************************************/
/*!
 This function set the sysClock accordly datasheet
 Parameters:
 pll1: PLL Control Register 1
 pll2: PLL Control Register 2
 pixclk: Pixel Clock Setting Register
 [private]
 */
/**************************************************************************/
void RA8875__setSysClock(uint8_t pll1, uint8_t pll2, uint8_t pixclk) {
	RA8875_writeRegister(RA8875_PLLC1, pll1);	////PLL Control Register 1
	// // vTaskDelay(1);
	RA8875_writeRegister(RA8875_PLLC1 + 1, pll2);	////PLL Control Register 2
	// // vTaskDelay(1);
	RA8875_writeRegister(RA8875_PCSR, pixclk);	//Pixel Clock Setting Register
	// // vTaskDelay(1);
}

/**************************************************************************/
/*!
 Give you back the current text cursor position as tracked by library (fast)
 Parameters:
 x: horizontal pos in pixels
 y: vertical pos in pixels
 note: works also with rendered fonts
 */
/**************************************************************************/
void RA8875_getCursorFast(RA8875_struct *ra8875, int16_t *x, int16_t *y) {
	*x = ra8875->_cursorX;
	*y = ra8875->_cursorY;
	if (ra8875->_portrait)
		swapvals_int16_t(*x, *y);
}

int16_t RA8875_getCursorX(RA8875_struct *ra8875) {
	if (ra8875->_portrait)
		return ra8875->_cursorY;
	return ra8875->_cursorX;
}

int16_t RA8875_getCursorY(RA8875_struct *ra8875) {
	if (ra8875->_portrait)
		return ra8875->_cursorX;
	return ra8875->_cursorY;
}

/*!
 Set the Text position for write Text only.
 Parameters:
 x:horizontal in pixels or CENTER(of the screen)
 y:vertical in pixels or CENTER(of the screen)
 autocenter:center text to choosed x,y regardless text lenght
 false: |ABCD
 true:  AB|CD
 NOTE: works with any font
 */
/**************************************************************************/
void RA8875_setCursor(RA8875_struct *ra8875, int16_t x, int16_t y,
bool autocenter) {
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	ra8875->_absoluteCenter = autocenter;

	if (ra8875->_portrait) {		//rotation 1,3
		swapvals_int16_t(x, y);
		if (y == CENTER) {		//swapped OK
			y = ra8875->_width / 2;
			if (!autocenter) {
				ra8875->_relativeCenter = true;
				ra8875->_TXTparameters |= (1 << 6);		//set x flag
			}
		}
		if (x == CENTER) {		//swapped
			x = ra8875->_height / 2;
			if (!autocenter) {
				ra8875->_relativeCenter = true;
				ra8875->_TXTparameters |= (1 << 5);		//set y flag
			}
		}
	} else {		//rotation 0,2
		if (x == CENTER) {
			x = ra8875->_width / 2;
			if (!autocenter) {
				ra8875->_relativeCenter = true;
				ra8875->_TXTparameters |= (1 << 5);
			}
		}
		if (y == CENTER) {
			y = ra8875->_height / 2;
			if (!autocenter) {
				ra8875->_relativeCenter = true;
				ra8875->_TXTparameters |= (1 << 6);
			}
		}
	}
	//TODO: This one? Useless?
	if (bitRead(ra8875->_TXTparameters, 2) == 0) {		//textWrap
		if (x >= ra8875->_width)
			x = ra8875->_width - 1;
		if (y >= ra8875->_height)
			y = ra8875->_height - 1;
	}

	ra8875->_cursorX = x;
	ra8875->_cursorY = y;

	//if _relativeCenter or _absoluteCenter do not apply to registers yet!
	// Have to go to _textWrite first to calculate the lenght of the entire string and recalculate the correct x,y
	if (ra8875->_relativeCenter || ra8875->_absoluteCenter)
		return;
	if ( bitRead(ra8875->_TXTparameters, 7) == 0)
		RA8875_textPosition(ra8875, x, y, false);
}

/**************************************************************************/
/*!
 Sets set the foreground color using 16bit RGB565 color
 It handles automatically color conversion when in 8 bit!
 Parameters:
 color: 16bit color RGB565
 */
/**************************************************************************/
void RA8875_setForegroundColor(RA8875_struct *ra8875, uint16_t color) {
	ra8875->_foreColor = color;  //keep track
	RA8875_writeRegister(RA8875_FGCR0, ((color & 0xF800) >> _RA8875colorMask[ra8875->_colorIndex]));
	RA8875_writeRegister(RA8875_FGCR0 + 1, ((color & 0x07E0) >> _RA8875colorMask[ra8875->_colorIndex + 1]));
	RA8875_writeRegister(RA8875_FGCR0 + 2, ((color & 0x001F) >> _RA8875colorMask[ra8875->_colorIndex + 2]));
}
/**************************************************************************/
/*!
 Sets set the foreground color using 8bit R,G,B
 Parameters:
 R: 8bit RED
 G: 8bit GREEN
 B: 8bit BLUE
 */
/**************************************************************************/
void RA8875_setForegroundColorRGB(RA8875_struct *ra8875, uint8_t R, uint8_t G, uint8_t B) {
	ra8875->_foreColor = Color565(R, G, B); //keep track
	RA8875_writeRegister(RA8875_FGCR0, R);
	RA8875_writeRegister(RA8875_FGCR0 + 1, G);
	RA8875_writeRegister(RA8875_FGCR0 + 2, B);
}
/**************************************************************************/
/*!
 Sets set the background color using 16bit RGB565 color
 It handles automatically color conversion when in 8 bit!
 Parameters:
 color: 16bit color RGB565
 Note: will set background Trasparency OFF
 */
/**************************************************************************/
void RA8875_setBackgroundColor(RA8875_struct *ra8875, uint16_t color) {
	ra8875->_backColor = color;  //keep track
	RA8875_writeRegister(RA8875_BGCR0, ((color & 0xF800) >> _RA8875colorMask[ra8875->_colorIndex]));  //11
	RA8875_writeRegister(RA8875_BGCR0 + 1, ((color & 0x07E0) >> _RA8875colorMask[ra8875->_colorIndex + 1])); //5
	RA8875_writeRegister(RA8875_BGCR0 + 2, ((color & 0x001F) >> _RA8875colorMask[ra8875->_colorIndex + 2])); //0
}
/**************************************************************************/
/*!
 Sets set the background color using 8bit R,G,B
 Parameters:
 R: 8bit RED
 G: 8bit GREEN
 B: 8bit BLUE
 Note: will set background Trasparency OFF
 */
/**************************************************************************/
void RA8875_setBackgroundColorRGB(RA8875_struct *ra8875, uint8_t R, uint8_t G, uint8_t B) {
	ra8875->_backColor = Color565(R, G, B); //keep track
	RA8875_writeRegister(RA8875_BGCR0, R);
	RA8875_writeRegister(RA8875_BGCR0 + 1, G);
	RA8875_writeRegister(RA8875_BGCR0 + 2, B);
}

/**************************************************************************/
/*!
 Change the mode between graphic and text
 Parameters:
 m: can be GRAPHIC or TEXT
 [private]
 */
/**************************************************************************/
void RA8875_setTextMode(RA8875_struct *ra8875, bool m) {
	if (m == ra8875->_textMode)
		return;
	RA8875_writeCommand(RA8875_MWCR0);
	//if (m != 0){//text
	if (m) {	//text
		ra8875->_MWCR0_Reg |= (1 << 7);
		ra8875->_textMode = true;
	} else {	//graph
		ra8875->_MWCR0_Reg &= ~(1 << 7);
		ra8875->_textMode = false;
	}
	RA8875_writeData(ra8875->_MWCR0_Reg);
}

/**************************************************************************/
/*!
 turn display on/off
 */
/**************************************************************************/
void RA8875_displayOn(RA8875_struct *ra8875, boolean on) {
	on == true ? RA8875_writeRegister(RA8875_PWRR,
	RA8875_PWRR_NORMAL | RA8875_PWRR_DISPON) :
					RA8875_writeRegister(RA8875_PWRR,
					RA8875_PWRR_NORMAL | RA8875_PWRR_DISPOFF);
}
/*!
 Change the rotation of the screen
 Parameters:
 rotation:
 0 = default
 1 = 90
 2 = 180
 3 = 270
 */
/**************************************************************************/
void RA8875_setRotation(RA8875_struct *ra8875, uint8_t rotation)	//0.69b32 - less code
{
	ra8875->_rotation = rotation % 4; //limit to the range 0-3
	switch (ra8875->_rotation) {
	case 0:
		//default, connector to bottom
		ra8875->_portrait = false;
		RA8875_scanDirection(ra8875, 0, 0);
		break;
	case 1:
		//90
		ra8875->_portrait = true;
		RA8875_scanDirection(ra8875, 1, 0);
		break;
	case 2:
		//180
		ra8875->_portrait = false;
		RA8875_scanDirection(ra8875, 1, 1);
		break;
	case 3:
		//270
		ra8875->_portrait = true;
		RA8875_scanDirection(ra8875, 0, 1);
		break;
	}
	if (ra8875->_portrait) {
		ra8875->_width = ra8875->RA8875_HEIGHT;
		ra8875->_height = ra8875->RA8875_WIDTH;
		ra8875->_FNCR1_Reg |= (1 << 4);
	} else {
		ra8875->_width = ra8875->RA8875_WIDTH;
		ra8875->_height = ra8875->RA8875_HEIGHT;
		ra8875->_FNCR1_Reg &= ~(1 << 4);
	}
	RA8875_writeRegister(RA8875_FNCR1, ra8875->_FNCR1_Reg); //0.69b21
	RA8875_setActiveWindow(ra8875);
}

/**************************************************************************/
/*!
 Change the beam scan direction on display
 Parameters:
 invertH: true(inverted),false(normal) horizontal
 invertV: true(inverted),false(normal) vertical
 */
/**************************************************************************/
void RA8875_scanDirection(RA8875_struct *ra8875, boolean invertH,
boolean invertV) {
	if (invertH)
		ra8875->_DPCR_Reg |= (1 << 3);
	else
		ra8875->_DPCR_Reg &= ~(1 << 3);
	if (invertV)
		ra8875->_DPCR_Reg |= (1 << 2);
	else
		ra8875->_DPCR_Reg &= ~(1 << 2);
	RA8875_writeRegister(RA8875_DPCR, ra8875->_DPCR_Reg);
}

/**************************************************************************/
/*!
 Set the Active Window
 Parameters:
 XL: Horizontal Left
 XR: Horizontal Right
 YT: Vertical TOP
 YB: Vertical Bottom
 */
/**************************************************************************/
void RA8875_setActiveWindowXXYY(RA8875_struct *ra8875, int16_t XL, int16_t XR, int16_t YT, int16_t YB) {
	if (ra8875->_portrait) {
		swapvals_uint16_t(XL, YT);
		swapvals_uint16_t(XR, YB);
	}

	if (XR >= ra8875->RA8875_WIDTH)
		XR = ra8875->RA8875_WIDTH;
	if (YB >= ra8875->RA8875_HEIGHT)
		YB = ra8875->RA8875_HEIGHT;

	ra8875->_activeWindowXL = XL;
	ra8875->_activeWindowXR = XR;
	ra8875->_activeWindowYT = YT;
	ra8875->_activeWindowYB = YB;
	RA8875_updateActiveWindow(ra8875, false);
}

/**************************************************************************/
/*!
 Set the Active Window as FULL SCREEN
 */
/**************************************************************************/
void RA8875_setActiveWindow(RA8875_struct *ra8875) {
	ra8875->_activeWindowXL = 0;
	ra8875->_activeWindowXR = ra8875->RA8875_WIDTH;
	ra8875->_activeWindowYT = 0;
	ra8875->_activeWindowYB = ra8875->RA8875_HEIGHT;
	if (ra8875->_portrait) {
		swapvals_int16_t(ra8875->_activeWindowXL, ra8875->_activeWindowYT);
		swapvals_int16_t(ra8875->_activeWindowXR, ra8875->_activeWindowYB);
	}
	RA8875_updateActiveWindow(ra8875, true);
}

/**************************************************************************/
/*!
 this update the RA8875 Active Window registers
 [private]
 */
/**************************************************************************/
void RA8875_updateActiveWindow(RA8875_struct *ra8875,
bool full) {
	if (full) {
		// X
		RA8875_writeRegister(RA8875_HSAW0, 0x00);
		RA8875_writeRegister(
		RA8875_HSAW0 + 1, 0x00);
		RA8875_writeRegister(RA8875_HEAW0, (ra8875->RA8875_WIDTH) & 0xFF);
		RA8875_writeRegister(
		RA8875_HEAW0 + 1, (ra8875->RA8875_WIDTH) >> 8);
		// Y
		RA8875_writeRegister(RA8875_VSAW0, 0x00);
		RA8875_writeRegister(
		RA8875_VSAW0 + 1, 0x00);
		RA8875_writeRegister(RA8875_VEAW0, (ra8875->RA8875_HEIGHT) & 0xFF);
		RA8875_writeRegister(
		RA8875_VEAW0 + 1, (ra8875->RA8875_HEIGHT) >> 8);
	} else {
		// X
		RA8875_writeRegister(RA8875_HSAW0, ra8875->_activeWindowXL & 0xFF);
		RA8875_writeRegister(
		RA8875_HSAW0 + 1, ra8875->_activeWindowXL >> 8);
		RA8875_writeRegister(RA8875_HEAW0, ra8875->_activeWindowXR & 0xFF);
		RA8875_writeRegister(
		RA8875_HEAW0 + 1, ra8875->_activeWindowXR >> 8);
		// Y
		RA8875_writeRegister(RA8875_VSAW0, ra8875->_activeWindowYT & 0xFF);
		RA8875_writeRegister(
		RA8875_VSAW0 + 1, ra8875->_activeWindowYT >> 8);
		RA8875_writeRegister(RA8875_VEAW0, ra8875->_activeWindowYB & 0xFF);
		RA8875_writeRegister(
		RA8875_VEAW0 + 1, ra8875->_activeWindowYB >> 8);
	}
}

/**************************************************************************/
/*!
 Set the x,y position for text only
 Parameters:
 x: horizontal pos in pixels
 y: vertical pos in pixels
 update: true track the actual text position internally
 note: not active with rendered fonts, just set x,y internal tracked param
 [private]
 */
/**************************************************************************/
void RA8875_textPosition(RA8875_struct *ra8875, int16_t x, int16_t y,
bool update) {
	RA8875_writeRegister(RA8875_F_CURXL, (x & 0xFF));
	RA8875_writeRegister(RA8875_F_CURXH, (x >> 8));
	RA8875_writeRegister(RA8875_F_CURYL, (y & 0xFF));
	RA8875_writeRegister(RA8875_F_CURYH, (y >> 8));
	if (update) {
		ra8875->_cursorX = x;
		ra8875->_cursorY = y;
	}
}

/**************************************************************************/
/*!
 controls the backligh by using PWM engine.
 It handles adafruit board separately
 Parameters:
 on: true(backlight on), false(backlight off)
 */
/**************************************************************************/
void RA8875_backlight(RA8875_struct *ra8875, boolean on) //0.69b31 (fixed an issue with adafruit backlight)
{
	if (on == true) {
		RA8875_PWMsetup(1, true,
		RA8875_PWM_CLK_DIV1024); //setup PWM ch 1 for backlight
		RA8875_PWMout(1, ra8875->_brightness); //turn on PWM1
	} else {
		RA8875_PWMsetup(1, false,
		RA8875_PWM_CLK_DIV1024); //setup PWM ch 1 for backlight
	}
}

/**************************************************************************/
/*!
 Fill the ActiveWindow by using a specified RGB565 color
 Parameters:
 color: RGB565 color (default=BLACK)
 */
/**************************************************************************/
void RA8875_fillWindow(RA8875_struct *ra8875, uint16_t color) {
	RA8875_line_addressing(ra8875, 0, 0, ra8875->RA8875_WIDTH - 1, ra8875->RA8875_HEIGHT - 1);
	RA8875_setForegroundColor(ra8875, color);
	RA8875_writeCommand(RA8875_DCR);
	RA8875_writeData(0xB0);
	RA8875_waitPoll(RA8875_DCR,
	RA8875_DCR_LINESQUTRI_STATUS);
	ra8875->_TXTrecoverColor = true;
}

/**************************************************************************/
/*!
 Graphic line addressing helper
 [private]
 */
/**************************************************************************/
void RA8875_line_addressing(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
	//X0
	RA8875_writeRegister(RA8875_DLHSR0, x0 & 0xFF);
	RA8875_writeRegister(
	RA8875_DLHSR0 + 1, x0 >> 8);
	//Y0
	RA8875_writeRegister(RA8875_DLVSR0, y0 & 0xFF);
	RA8875_writeRegister(
	RA8875_DLVSR0 + 1, y0 >> 8);
	//X1
	RA8875_writeRegister(RA8875_DLHER0, x1 & 0xFF);
	RA8875_writeRegister(
	RA8875_DLHER0 + 1, x1 >> 8);
	//Y1
	RA8875_writeRegister(RA8875_DLVER0, y1 & 0xFF);
	RA8875_writeRegister(
	RA8875_DLVER0 + 1, y1 >> 8);
}

/**************************************************************************/
/*!
 Clear memory (different from fillWindow!)
 Parameters:
 stop: stop clear memory operation
 */
/**************************************************************************/
void RA8875_clearMemory(RA8875_struct *ra8875, bool stop) {
	uint8_t temp;
	temp = RA8875_readRegister(RA8875_MCLR);
	if (stop)
		temp &= ~(1 << 7);
	else
		temp |= (1 << 7);
	RA8875_writeData(temp);
	if (!stop)
		RA8875_waitBusy(0x80);
}

/**************************************************************************/
/*!
 choose from internal/external (if exist) Font Rom
 Parameters:
 s: Font source (INT,EXT)
 */
/**************************************************************************/
void RA8875_setFont(RA8875_struct *ra8875, enum RA8875fontSource s) {
	if (!ra8875->_textMode)
		RA8875_setTextMode(ra8875, true);
	; //we are in graph mode?
	ra8875->_TXTparameters &= ~(1 << 7); //render OFF
	if (s == INT) {
		RA8875_setFNTdimensions(ra8875, 0);
		//check the font coding
		if (bitRead(ra8875->_TXTparameters, 0) == 1) { //0.96b22 _extFontRom = true
			RA8875_setFontSize(ra8875, X16);
			RA8875_writeRegister(RA8875_SFRSET, 0b00000000); //_SFRSET_Reg
		}
		ra8875->_FNCR0_Reg &= ~((1 << 7) | (1 << 5)); // Clear bits 7 and 5 (set internal CGROM)
		RA8875_writeRegister(RA8875_FNCR0, ra8875->_FNCR0_Reg);
		ra8875->_FNTsource = s;
		// // vTaskDelay(1);
	} else if (s == EXT) {
		if ( bitRead(ra8875->_TXTparameters, 0) == 1) { //0.96b22 _extFontRom = true
			ra8875->_FNTsource = s;
			//now switch
			ra8875->_FNCR0_Reg &= ~(1 << 7); //clearBit 7
			ra8875->_FNCR0_Reg |= (1 << 5); //setBit 5
			RA8875_writeRegister(RA8875_FNCR0, ra8875->_FNCR0_Reg); //0x21
			// vTaskDelay(1);
			RA8875_writeRegister(RA8875_SFCLR, 0x03); //0x02 Serial Flash/ROM CLK frequency/2
			RA8875_setFontSize(ra8875, X16);
			RA8875_writeRegister(RA8875_SFRSET, ra8875->_SFRSET_Reg); //at this point should be already set
			// vTaskDelay(4);
			RA8875_writeRegister(RA8875_SROC, 0x28); // 0x28 rom 0,24bit adrs,wave 3,1 byte dummy,font mode, single mode 00101000
			// vTaskDelay(4);
		} else {
			RA8875_setFont(ra8875, INT);
			RA8875_setFNTdimensions(ra8875, 0);
		}
	} else {
		return;
	}
	ra8875->_spaceCharWidth = ra8875->_FNTwidth;
	//setFontScale(0);
	ra8875->_scaleX = 1;
	ra8875->_scaleY = 1; //reset font scale
}

/**************************************************************************/
/*!
 Choose between 16x16(8x16) - 24x24(12x24) - 32x32(16x32)
 for External Font ROM
 Parameters:
 ts: X16,X24,X32
 Note: Inactive with rendered fonts
 TODO: Modify font size variables accordly font size!
 */
/**************************************************************************/
void RA8875_setFontSize(RA8875_struct *ra8875, enum RA8875tsize ts) {
	if (ra8875->_FNTsource == EXT && bitRead(ra8875->_TXTparameters, 7) == 0) {
		switch (ts) {
		case X16:
			ra8875->_FWTSET_Reg &= 0x3F;
			RA8875_setFNTdimensions(ra8875, 1);
			break;
		case X24:
			ra8875->_FWTSET_Reg &= 0x3F;
			ra8875->_FWTSET_Reg |= 0x40;
			RA8875_setFNTdimensions(ra8875, 2);
			break;
		case X32:
			ra8875->_FWTSET_Reg &= 0x3F;
			ra8875->_FWTSET_Reg |= 0x80;
			RA8875_setFNTdimensions(ra8875, 3);
			break;
		default:
			return;
		}
		ra8875->_EXTFNTsize = ts;
		RA8875_writeRegister(RA8875_FWTSET, ra8875->_FWTSET_Reg);
	}
}
void RA8875_setFNTdimensions(RA8875_struct *ra8875, uint8_t index) {
	ra8875->_FNTwidth = fontDimPar[index][0];
	ra8875->_FNTheight = fontDimPar[index][1];
	ra8875->_FNTbaselineLow = fontDimPar[index][2];
	ra8875->_FNTbaselineTop = fontDimPar[index][3];
}

/**************************************************************************/
/*!
 Set internal Font Encoding
 Parameters:
 f: ISO_IEC_8859_1, ISO_IEC_8859_2, ISO_IEC_8859_3, ISO_IEC_8859_4
 default: ISO_IEC_8859_1
 */
/**************************************************************************/
void RA8875_setIntFontCoding(RA8875_struct *ra8875, enum RA8875fontCoding f) {
	uint8_t temp = ra8875->_FNCR0_Reg;
	temp &= ~((1 << 1) | (1 << 0)); // Clear bits 1 and 0
	switch (f) {
	case ISO_IEC_8859_1:
		//do nothing
		break;
	case ISO_IEC_8859_2:
		temp |= (1 << 0);
		break;
	case ISO_IEC_8859_3:
		temp |= (1 << 1);
		break;
	case ISO_IEC_8859_4:
		temp |= ((1 << 1) | (1 << 0)); // Set bits 1 and 0
		break;
	default:
		return;
	}
	ra8875->_FNCR0_Reg = temp;
	RA8875_writeRegister(RA8875_FNCR0, ra8875->_FNCR0_Reg);
}
/**************************************************************************/
/*!     Set cursor property blink and his rate
 Parameters:
 rate: blink speed (fast 0...255 slow)
 note: not active with rendered fonts
 */
/**************************************************************************/
void RA8875_setCursorBlinkRate(RA8875_struct *ra8875, uint8_t rate) {
	RA8875_writeRegister(RA8875_BTCR, rate);    //set blink rate
}

/**************************************************************************/
/*!
 set the text color and his background
 Parameters:
 fcolor: 16bit foreground color (text) RGB565
 bcolor: 16bit background color RGB565
 NOTE: will set background trasparent OFF
 It also works with rendered fonts.
 */
/**************************************************************************/
void RA8875_setTextColor(RA8875_struct *ra8875, uint16_t fcolor, uint16_t bcolor) //0.69b30
{
	if (fcolor != ra8875->_TXTForeColor) {
		ra8875->_TXTForeColor = fcolor;
		RA8875_setForegroundColor(ra8875, fcolor);
	}
	if (bcolor != ra8875->_TXTBackColor) {
		ra8875->_TXTBackColor = bcolor;
		RA8875_setBackgroundColor(ra8875, bcolor);
	}
	ra8875->_backTransparent = false;
	if (bitRead(ra8875->_TXTparameters, 7) == 0) {
		ra8875->_FNCR1_Reg &= ~(1 << 6);    //clear
		RA8875_writeRegister(RA8875_FNCR1, ra8875->_FNCR1_Reg);
	}
}

/**************************************************************************/
/*!
 set the text color w transparent background
 Parameters:
 fColor: 16bit foreground color (text) RGB565
 NOTE: will set background trasparent ON
 It also works with rendered fonts.
 */
/**************************************************************************/

void RA8875_setTextColorForeground(RA8875_struct *ra8875, uint16_t fcolor) {
	if (fcolor != ra8875->_TXTForeColor) {
		ra8875->_TXTForeColor = fcolor;
		RA8875_setForegroundColor(ra8875, fcolor);
	}
	ra8875->_backTransparent = true;
	if (bitRead(ra8875->_TXTparameters, 7) == 0) {
		ra8875->_FNCR1_Reg |= (1 << 6);    //set
		RA8875_writeRegister(RA8875_FNCR1, ra8875->_FNCR1_Reg);
	}
}

/**************************************************************************/
/*!	PRIVATE
 Main routine that write a single char in render mode, this actually call another subroutine that do the paint job
 but this one take care of all the calculations...
 NOTE: It identify correctly println and /n & /r
 */
/**************************************************************************/
void RA8875_charWriteR(RA8875_struct *ra8875, const char c, uint8_t offset, uint16_t fcolor, uint16_t bcolor) {
	if (c == 13) { //------------------------------- CARRIAGE ----------------------------------
		//ignore
	} else if (c == 10) {        //------------------------- NEW LINE ---------------------------------
		if (!ra8875->_portrait) {
			ra8875->_cursorX = 0;
			ra8875->_cursorY += (ra8875->_FNTheight * ra8875->_scaleY) + ra8875->_FNTinterline + offset;
		} else {
			ra8875->_cursorX += (ra8875->_FNTheight * ra8875->_scaleY) + ra8875->_FNTinterline + offset;
			ra8875->_cursorY = 0;
		}
		RA8875_textPosition(ra8875, ra8875->_cursorX, ra8875->_cursorY, false);
	} else if (c == 32) {        //--------------------------- SPACE ---------------------------------
		if (!ra8875->_portrait) {
			RA8875_fillRect(ra8875, ra8875->_cursorX, ra8875->_cursorY, (ra8875->_spaceCharWidth * ra8875->_scaleX),
					(ra8875->_FNTheight * ra8875->_scaleY), bcolor);	//bColor
			ra8875->_cursorX += (ra8875->_spaceCharWidth * ra8875->_scaleX) + ra8875->_FNTspacing;
		} else {
			RA8875_fillRect(ra8875, ra8875->_cursorY, ra8875->_cursorX, (ra8875->_spaceCharWidth * ra8875->_scaleX),
					(ra8875->_FNTheight * ra8875->_scaleY), bcolor);	//bColor
			ra8875->_cursorY += (ra8875->_spaceCharWidth * ra8875->_scaleX) + ra8875->_FNTspacing;
		}
		// #if defined(FORCE_RA8875_TXTREND_FOLLOW_CURS)
		// _textPosition(_cursorX,_cursorY,false);
		// #endif
	} else {        //-------------------------------------- CHAR ------------------------------------
		int charIndex = RA8875_getCharCode(ra8875, c);		//get char code
		if (charIndex > -1) {		//valid?
			int charW = 0;
			//get charW and glyph
#if defined(_FORCE_PROGMEM__)
			charW = PROGMEM_read(&ra8875->_currentFont->chars[charIndex].image->image_width);
#if !defined(_RA8875_TXTRNDOPTIMIZER)
			const uint8_t * charGlyp = PROGMEM_read(&_currentFont->chars[charIndex].image->data);
#endif
#else
			charW = ra8875->_currentFont->chars[charIndex].image->image_width;
#if !defined(_RA8875_TXTRNDOPTIMIZER)
			const uint8_t * charGlyp = ra8875->_currentFont->chars[charIndex].image->data;
#endif
#endif
			//check if goes out of screen and goes to a new line (if wrap) or just avoid
			if (bitRead(ra8875->_TXTparameters, 2)) {		//wrap?
				if (!ra8875->_portrait && (ra8875->_cursorX + charW * ra8875->_scaleX) >= ra8875->_width) {
					ra8875->_cursorX = 0;
					ra8875->_cursorY += (ra8875->_FNTheight * ra8875->_scaleY) + ra8875->_FNTinterline + offset;
				} else if (ra8875->_portrait && (ra8875->_cursorY + charW * ra8875->_scaleY) >= ra8875->_width) {
					ra8875->_cursorX += (ra8875->_FNTheight * ra8875->_scaleY) + ra8875->_FNTinterline + offset;
					ra8875->_cursorY = 0;
				}
#if defined(FORCE_RA8875_TXTREND_FOLLOW_CURS)
				//_textPosition(_cursorX,_cursorY,false);
#endif
			} else {
				if (ra8875->_portrait) {
					if (ra8875->_cursorY + charW * ra8875->_scaleY >= ra8875->_width)
						return;
				} else {
					if (ra8875->_cursorX + charW * ra8875->_scaleX >= ra8875->_width)
						return;
				}
			}
			//test purposes ----------------------------------------------------------------
			/*
			 if (!_portrait){
			 RA8875_fillRect(_cursorX,_cursorY,(charW * _scaleX),(_FNTheight * _scaleY),RA8875_YELLOW);//bColor
			 } else {
			 RA8875_fillRect(_cursorY,_cursorX,(charW * _scaleX),(_FNTheight * _scaleY),RA8875_YELLOW);//bColor
			 }
			 */
			//-------------------------Actual single char drawing here -----------------------------------
			if (!ra8875->_FNTcompression) {
#if defined(_RA8875_TXTRNDOPTIMIZER)
				if (!ra8875->_portrait) {
					RA8875_drawChar_unc(ra8875, ra8875->_cursorX, ra8875->_cursorY, charW, charIndex, fcolor);
				} else {
					RA8875_drawChar_unc(ra8875, ra8875->_cursorY, ra8875->_cursorX, charW, charIndex, fcolor);
				}
#else
				if (!ra8875->_portrait) {
					_drawChar_unc(ra8875->_cursorX,ra8875->_cursorY,charW,charGlyp,fcolor,bcolor);
				} else {
					_drawChar_unc(ra8875->_cursorY,ra8875->_cursorX,charW,charGlyp,fcolor,bcolor);
				}
#endif
			} else {
				//TODO
				//RLE compressed fonts
			}

			//add charW to total -----------------------------------------------------
			if (!ra8875->_portrait) {
				ra8875->_cursorX += (charW * ra8875->_scaleX) + ra8875->_FNTspacing;
			} else {
				ra8875->_cursorY += (charW * ra8875->_scaleX) + ra8875->_FNTspacing;
			}

			// #if defined(FORCE_RA8875_TXTREND_FOLLOW_CURS)
			// _textPosition(_cursorX,_cursorY,false);
			// #endif
		}			//end valid
	}			//end char
}

/**************************************************************************/
/*!	PRIVATE
 Write a single char, only INT and FONT ROM char (internal RA9975 render)
 NOTE: It identify correctly println and /n & /r
 */
/**************************************************************************/
void RA8875_charWrite(RA8875_struct *ra8875, const char c, uint8_t offset) {
	bool dtacmd = false;
	if (c == 13) {			//'\r'
		//Ignore carriage-return
	} else if (c == 8) {		//'\b'
		//Backspace
		if (!ra8875->_portrait) {
			ra8875->_cursorX -= ra8875->_FNTwidth * (ra8875->_scaleX);
		} else {
			ra8875->_cursorY -= ra8875->_FNTwidth * (ra8875->_scaleX);
		}
		RA8875_textPosition(ra8875, ra8875->_cursorX, ra8875->_cursorY, false);
		RA8875_setCursor(ra8875, ra8875->_cursorX, ra8875->_cursorY, false);
		dtacmd = false;
	} else if (c == 10) {		//'\n'
		if (!ra8875->_portrait) {
			ra8875->_cursorX = 0;
			ra8875->_cursorY += (ra8875->_FNTheight + (ra8875->_FNTheight * (ra8875->_scaleY - 1)))
					+ ra8875->_FNTinterline + offset;
		} else {
			ra8875->_cursorX += (ra8875->_FNTheight + (ra8875->_FNTheight * (ra8875->_scaleY - 1)))
					+ ra8875->_FNTinterline + offset;
			ra8875->_cursorY = 0;
		}
		RA8875_textPosition(ra8875, ra8875->_cursorX, ra8875->_cursorY, false);
		dtacmd = false;
	} else {
		if (!dtacmd) {
			dtacmd = true;

			if (!ra8875->_textMode)
				RA8875_setTextMode(ra8875, true);
			;	//we are in graph mode?
			RA8875_writeCommand(RA8875_MRWC);
		}
		RA8875_writeData(c);
		RA8875_waitBusy(0x80);
		//update cursor
		if (!ra8875->_portrait) {
			ra8875->_cursorX += ra8875->_FNTwidth * (ra8875->_scaleX);
		} else {
			ra8875->_cursorY += ra8875->_FNTwidth * (ra8875->_scaleX);
		}
	}
}

/**************************************************************************/
/*!	PRIVATE
 Search for glyph char code in font array
 It return font index or -1 if not found.
 */
/**************************************************************************/
int RA8875_getCharCode(RA8875_struct *ra8875, uint8_t ch) {
	int i;
	for (i = 0; i < ra8875->_currentFont->length; i++) {	//search for char code
#if defined(_FORCE_PROGMEM__)
			uint8_t ccode =ra8875-> _currentFont->chars[i].char_code;
			if (ccode == ch) return i;
#else
		if (ra8875->_currentFont->chars[i].char_code == ch)
			return i;
#endif
	}		//i
	return -1;
}

/**************************************************************************/
/*!
 draws a FILLED rectangle
 Parameters:
 x: horizontal start
 y: vertical start
 w: width
 h: height
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_fillRect(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
	//RA8875 it's not out-of-range tolerant so this is a workaround
	if (w < 1 || h < 1)
		return;		//it cannot be!
	if (w < 2 && h < 2) { //render as pixel
		RA8875_drawPixel(ra8875, x, y, color);
	} else {		//render as rect
		RA8875_rect_helper(ra8875, x, y, (x + w) - 1, (y + h) - 1, color, true);		//thanks the experimentalist
	}
}

/**************************************************************************/
/*!
 Write a single pixel
 Parameters:
 x: horizontal pos
 y: vertical pos
 color: RGB565 color
 NOTE:
 In 8bit bpp RA8875 needs a 8bit color(332) and NOT a 16bit(565),
 the routine deal with this...
 */
/**************************************************************************/
void RA8875_drawPixel(RA8875_struct *ra8875, int16_t x, int16_t y, uint16_t color) {
	//setXY(x,y);
	if (ra8875->_textMode)
		RA8875_setTextMode(ra8875, false);
	//we are in text mode?
	RA8875_setXY(ra8875, x, y);
	RA8875_writeCommand(RA8875_MRWC);
	if (ra8875->_color_bpp > 8) {
		RA8875_writeData16(color);
	} else {		//TOTEST:layer bug workaround for 8bit color!
		RA8875_writeData(_color16To8bpp(color));
	}
}

/**************************************************************************/
/*!
 Draw a series of pixels
 Parameters:
 p: an array of 16bit colors (pixels)
 count: how many pixels
 x: horizontal pos
 y: vertical pos
 NOTE:
 In 8bit bpp RA8875 needs a 8bit color(332) and NOT a 16bit(565),
 the routine deal with this...
 */
/**************************************************************************/
//void RA8875_drawPixels(RA8875_struct *ra8875, uint16_t p[], uint16_t count, int16_t x, int16_t y) {
////setXY(x,y);
//    uint16_t temp = 0;
//    uint16_t i;
//    if (ra8875->_textMode)
//    RA8875_setTextMode(ra8875, false);;//we are in text mode?
//    setXY(x, y);
//    RA8875_writeCommand(RA8875_MRWC);
//    _startSend();
////set data
//#if defined(__AVR__) && defined(_FASTSSPORT)
//    _spiwrite(RA8875_DATAWRITE);
//#else
//#if defined(SPI_HAS_TRANSACTION) && defined(__MKL26Z64__)
//    if (_altSPI) {
//        SPI1.transfer(RA8875_DATAWRITE);
//    } else {
//        SPI_transfer(RA8875_DATAWRITE);
//    }
//#else
//    SPI_transfer(RA8875_DATAWRITE);
//#endif
//#endif
////the loop
//    for (i = 0; i < count; i++) {
//        if (_color_bpp < 16) {
//            temp = _color16To8bpp(p[i]); //TOTEST:layer bug workaround for 8bit color!
//        } else {
//            temp = p[i];
//        }
//#if !defined(ENERGIA) && !defined(___DUESTUFF) && ((ARDUINO >= 160) || (TEENSYDUINO > 121))
//#if defined(SPI_HAS_TRANSACTION) && defined(__MKL26Z64__)
//        if (_color_bpp > 8) {
//            if (_altSPI) {
//                SPI1.transfer16(temp);
//            } else {
//                SPI_transfer16(temp);
//            }
//        } else {				//TOTEST:layer bug workaround for 8bit color!
//            if (_altSPI) {
//                SPI1.transfer(temp);
//            } else {
//                SPI_transfer(temp & 0xFF);
//            }
//        }
//#else
//        if (_color_bpp > 8) {
//            SPI_transfer16(temp);
//        } else {				//TOTEST:layer bug workaround for 8bit color!
//            SPI_transfer(temp & 0xFF);
//        }
//#endif
//#else
//#if defined(___DUESTUFF) && defined(SPI_DUE_MODE_EXTENDED)
//        if (_color_bpp > 8) {
//            SPI_transfer(_cs, temp >> 8, SPI_CONTINUE);
//            SPI_transfer(_cs, temp & 0xFF, SPI_LAST);
//        } else {				//TOTEST:layer bug workaround for 8bit color!
//            SPI_transfer(_cs, temp & 0xFF, SPI_LAST);
//        }
//#else
//#if defined(__AVR__) && defined(_FASTSSPORT)
//        if (_color_bpp > 8) {
//            _spiwrite16(temp);
//        } else {				//TOTEST:layer bug workaround for 8bit color!
//            _spiwrite(temp >> 8);
//        }
//#else
//        if (_color_bpp > 8) {
//            SPI_transfer(temp >> 8);
//            SPI_transfer(temp & 0xFF);
//        } else {				//TOTEST:layer bug workaround for 8bit color!
//            SPI_transfer(temp & 0xFF);
//        }
//#endif
//#endif
//#endif
//    }
//    _endSend();
//}
//
/**************************************************************************/
/*!
 helper function for rects (filled or not)
 [private]
 */
/**************************************************************************/
void RA8875_rect_helper(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color,
bool filled) {
	if (w < 1 || h < 1)
		return;	//why draw invisible rects?
	if (w >= ra8875->_width)
		return;
	if (h >= ra8875->_height)
		return;

	if (ra8875->_portrait) {
		swapvals_int16_t(x, y);
		swapvals_int16_t(w, h);
	}

	RA8875_checkLimits_helper(ra8875, &x, &y);

	if (ra8875->_textMode)
		RA8875_setTextMode(ra8875, false);
	;	//we are in text mode?
	ra8875->_TXTrecoverColor = true;
	if (color != ra8875->_foreColor)
		RA8875_setForegroundColor(ra8875, color);

	RA8875_line_addressing(ra8875, x, y, w, h);

	RA8875_writeCommand(RA8875_DCR);
	filled == true ? RA8875_writeData(0xB0) : RA8875_writeData(0x90);
	RA8875_waitPoll(RA8875_DCR,
	RA8875_DCR_LINESQUTRI_STATUS);
}
/**************************************************************************/
/*!
 Set the position for Graphic Write
 Parameters:
 x: horizontal position
 y: vertical position
 */
/**************************************************************************/

void RA8875_setXY(RA8875_struct *ra8875, int16_t x, int16_t y) {
	RA8875_setX(ra8875, x);
	RA8875_setY(ra8875, y);
}

/**************************************************************************/
/*!
 Set the x position for Graphic Write
 Parameters:
 x: horizontal position
 */
/**************************************************************************/
void RA8875_setX(RA8875_struct *ra8875, int16_t x) {
	if (x < 0)
		x = 0;
	if (ra8875->_portrait) {  //fix 0.69b21
		if (x >= ra8875->RA8875_HEIGHT)
			x = ra8875->RA8875_HEIGHT - 1;
		RA8875_writeRegister(RA8875_CURV0, x & 0xFF);
		RA8875_writeRegister(RA8875_CURV0 + 1, x >> 8);
	} else {
		if (x >= ra8875->RA8875_WIDTH)
			x = ra8875->RA8875_WIDTH - 1;
		RA8875_writeRegister(RA8875_CURH0, x & 0xFF);
		RA8875_writeRegister(RA8875_CURH0 + 1, (x >> 8));
	}
}

/**************************************************************************/
/*!
 Set the y position for Graphic Write
 Parameters:
 y: vertical position
 */
/**************************************************************************/
void RA8875_setY(RA8875_struct *ra8875, int16_t y) {
	if (y < 0)
		y = 0;
	if (ra8875->_portrait) {  //fix 0.69b21
		if (y >= ra8875->RA8875_WIDTH)
			y = ra8875->RA8875_WIDTH - 1;
		RA8875_writeRegister(RA8875_CURH0, y & 0xFF);
		RA8875_writeRegister(RA8875_CURH0 + 1, (y >> 8));
	} else {
		if (y >= ra8875->RA8875_HEIGHT)
			y = ra8875->RA8875_HEIGHT - 1;
		RA8875_writeRegister(RA8875_CURV0, y & 0xFF);
		RA8875_writeRegister(RA8875_CURV0 + 1, y >> 8);
	}
}

/**************************************************************************/
/*!
 draws a rectangle
 Parameters:
 x: horizontal start
 y: vertical start
 w: width
 h: height
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_drawRect(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
	//RA8875 it's not out-of-range tolerant so this is a workaround
	if (w < 1 || h < 1)
		return;  //it cannot be!
	if (w < 2 && h < 2) { //render as pixel
		RA8875_drawPixel(ra8875, x, y, color);
	} else {		//render as rect
		RA8875_rect_helper(ra8875, x, y, (w + x) - 1, (h + y) - 1, color,
		false);	//thanks the experimentalist
	}
}

/**************************************************************************/
/*!
 Return the max tft width.
 Parameters:
 absolute: if true will return the phisical width
 */
/**************************************************************************/
uint16_t RA8875_width(RA8875_struct *ra8875, bool absolute) {
	if (absolute)
		return ra8875->RA8875_WIDTH;
	return ra8875->_width;
}

/**************************************************************************/
/*!
 Return the max tft height.
 Parameters:
 absolute: if true will return the phisical height
 */
/**************************************************************************/
uint16_t RA8875_height(RA8875_struct *ra8875, bool absolute) {
	if (absolute)
		return ra8875->RA8875_HEIGHT;
	return ra8875->_height;
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +								TEXT STUFF											 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
/**************************************************************************/
/*!		Upload user custom char or symbol to CGRAM, max 255
 Parameters:
 symbol[]: an 8bit x 16 char in an array. Must be exact 16 bytes
 address: 0...255 the address of the CGRAM where to store the char
 */
/**************************************************************************/
void RA8875_uploadUserChar(RA8875_struct *ra8875, const uint8_t symbol[], uint8_t address) {
	uint8_t tempMWCR1;
//	while(1) {
	tempMWCR1 = RA8875_readRegister( RA8875_MWCR1); //thanks MorganSandercock
//	}
	uint8_t i;
	if (ra8875->_textMode)
		RA8875_setTextMode(ra8875, false); //we are in text mode?

	RA8875_writeRegister(RA8875_MWCR0, 0x00); //Graphics Mode
	RA8875_writeRegister(RA8875_CGSR, address); //CGRAM Space Select
	RA8875_writeRegister(RA8875_FNCR0, 0x00); //Select CGRAM
	RA8875_writeRegister(RA8875_MWCR1, 0x04); //Write to CGRAM

	RA8875_writeCommand(0x02); //Begin Write
	for (i = 0; i < 16; i++) {
		RA8875_writeData(symbol[i]);
	}

	RA8875_writeRegister(RA8875_MWCR1, tempMWCR1); //restore register
}

/**************************************************************************/
/*!		Retrieve and print to screen the user custom char or symbol
 User have to store a custom char before use this function
 Parameters:
 address: 0...255 the address of the CGRAM where char it's stored
 wide:0 for single 8x16 char, if you have wider chars that use
 more than a char slot they can be showed combined (see examples)
 */
/**************************************************************************/
void RA8875_showUserChar(RA8875_struct *ra8875, uint8_t symbolAddrs, uint8_t wide) {
	if (!ra8875->_textMode)
		RA8875_setTextMode(ra8875, true);
	; //we are in graph mode?
	uint8_t oldReg1State = ra8875->_FNCR0_Reg;
//	uint8_t oldReg2State = 0;
	uint8_t i;
	oldReg1State |= (1 << 7); //set to CGRAM
	oldReg1State |= (1 << 5); //TODO:check this (page 19)
	RA8875_writeRegister(RA8875_FNCR0, oldReg1State);
//	if (ra8875->_scaling) { //reset scale (not compatible with this!)
//		oldReg2State = ra8875->_FNCR1_Reg;
//		oldReg2State &= ~(0xF); // clear bits from 0 to 3
//		RA8875_writeRegister(RA8875_FNCR1, oldReg2State);
//	}
	//layers?

	if (ra8875->_useMultiLayers) {
		if (ra8875->_currentLayer == 0) {
			RA8875_writeTo(ra8875, L1);
		} else {
			RA8875_writeTo(ra8875, L2);
		}
	} else {
		//writeTo(L1);
	}

	RA8875_writeCommand(RA8875_MRWC);
	RA8875_writeData(symbolAddrs);
	if (wide > 0) {
		for (i = 1; i <= wide; i++) {
			RA8875_writeData(symbolAddrs + i);
		}
	}
//	if (oldReg2State != 0)
//		RA8875_writeRegister(RA8875_FNCR1, ra8875->_FNCR1_Reg); //put back scale as it was
	if (oldReg1State != ra8875->_FNCR0_Reg)
		RA8875_writeRegister(RA8875_FNCR0, ra8875->_FNCR0_Reg); //put back state
}

/**************************************************************************/
/*!	PRIVATE
 draw a string
 Works for all fonts, internal, ROM, external (render)
 */
/**************************************************************************/

void RA8875_textWrite(RA8875_struct *ra8875, const char* buffer, uint16_t len) {
	uint16_t i;
	if (len == 0)
		len = strlen(buffer); //try get the info from the buffer
	if (len == 0)
		return; //better stop here, the string it's really empty!
	bool renderOn = bitRead(ra8875->_TXTparameters, 7); //detect if render fonts active
	uint8_t loVOffset = 0;
	uint8_t hiVOffset = 0;
	uint8_t interlineOffset = 0;
	uint16_t fcolor = ra8875->_foreColor;
	uint16_t bcolor = ra8875->_backColor;
	uint16_t strngWidth = 0;
	uint16_t strngHeight = 0;
	if (!renderOn) {
		loVOffset = ra8875->_FNTbaselineLow * ra8875->_scaleY; //calculate lower baseline
		hiVOffset = ra8875->_FNTbaselineTop * ra8875->_scaleY; //calculate topline
		//now check for offset if using an external fonts rom (RA8875 bug)
		if (bitRead(ra8875->_TXTparameters, 0) == 1)
			interlineOffset = 3 * ra8875->_scaleY;
	}

	//_absoluteCenter or _relativeCenter cases...................
	//plus calculate the real width & height of the entire text in render mode (not trasparent)
	if (ra8875->_absoluteCenter || ra8875->_relativeCenter || (renderOn && !ra8875->_backTransparent)) {
		strngWidth = RA8875_STRlen_helper(ra8875, buffer, len) * ra8875->_scaleX; //this calculates the width of the entire text
		strngHeight = (ra8875->_FNTheight * ra8875->_scaleY) - (loVOffset + hiVOffset);	//the REAL heigh
		if (ra8875->_absoluteCenter && strngWidth > 0) {	//Avoid operations for strngWidth = 0
			ra8875->_absoluteCenter = false;
			ra8875->_cursorX = ra8875->_cursorX - (strngWidth / 2);
			ra8875->_cursorY = ra8875->_cursorY - (strngHeight / 2) - hiVOffset;
			if (ra8875->_portrait)
				swapvals_int16_t(ra8875->_cursorX, ra8875->_cursorY);
		} else if (ra8875->_relativeCenter && strngWidth > 0) {	//Avoid operations for strngWidth = 0
			ra8875->_relativeCenter = false;
			if (bitRead(ra8875->_TXTparameters, 5)) {	//X = center
				if (!ra8875->_portrait) {
					ra8875->_cursorX = (ra8875->_width / 2) - (strngWidth / 2);
				} else {
					ra8875->_cursorX = (ra8875->_height / 2) - (strngHeight / 2) - hiVOffset;
				}
				ra8875->_TXTparameters &= ~(1 << 5);	//reset
			}
			if (bitRead(ra8875->_TXTparameters, 6)) {	//Y = center
				if (!ra8875->_portrait) {
					ra8875->_cursorY = (ra8875->_height / 2) - (strngHeight / 2) - hiVOffset;
				} else {
					ra8875->_cursorY = (ra8875->_width / 2) - (strngWidth / 2);
				}
				ra8875->_TXTparameters &= ~(1 << 6);	//reset
			}
		}
		//if ((_absoluteCenter || _relativeCenter) &&  strngWidth > 0){//Avoid operations for strngWidth = 0
		if (strngWidth > 0) {	//Avoid operations for strngWidth = 0
			RA8875_textPosition(ra8875, ra8875->_cursorX, ra8875->_cursorY,
			false);
		}
	}	//_absoluteCenter,_relativeCenter,(renderOn && !_backTransparent)
//-----------------------------------------------------------------------------------------------
	if (!ra8875->_textMode && !renderOn)
		RA8875_setTextMode(ra8875, true);
	;	//   go to text
	if (ra8875->_textMode && renderOn)
		RA8875_setTextMode(ra8875, false);
	;	//  go to graphic
	//colored text vars
	uint16_t grandientLen = 0;
	uint16_t grandientIndex = 0;
	uint16_t recoverColor = fcolor;
	if (ra8875->_textMode && ra8875->_TXTrecoverColor) {
		if (ra8875->_foreColor != ra8875->_TXTForeColor) {
			ra8875->_TXTrecoverColor = false;
			RA8875_setForegroundColor(ra8875, ra8875->_TXTForeColor);
		}
		if (ra8875->_backColor != ra8875->_TXTBackColor) {
			ra8875->_TXTrecoverColor = false;
			RA8875_setBackgroundColor(ra8875, ra8875->_TXTBackColor);
		}
	} else {
		fcolor = ra8875->_TXTForeColor;
		bcolor = ra8875->_TXTBackColor;
	}
	if (ra8875->_FNTgrandient) {	//coloring text
		recoverColor = ra8875->_TXTForeColor;
		for (i = 0; i < len; i++) {	//avoid non char in color index
			if (buffer[i] != 13 && buffer[i] != 10 && buffer[i] != 32)
				grandientLen++;	//lenght of the interpolation
		}
	}
#if defined(_RA8875_TXTRNDOPTIMIZER)
	//instead write the background by using pixels (trough text rendering) better this trick
	if (renderOn && !ra8875->_backTransparent && strngWidth > 0)
		RA8875_fillRect(ra8875, ra8875->_cursorX, ra8875->_cursorY, strngWidth, strngHeight, ra8875->_backColor);//bColor
#endif
	//Loop trough every char and write them one by one...
	int backspace = 0;
	for (i = 0; i < len; i++) {
		if (ra8875->_FNTgrandient) {
			if (buffer[i] != 13 && buffer[i] != 10 && buffer[i] != 32) {
				if (!renderOn) {
					RA8875_setTextColorForeground(ra8875,
							RA8875_colorInterpolation(ra8875->_FNTgrandientColor1, ra8875->_FNTgrandientColor2,
									grandientIndex++, grandientLen));
				} else {
					fcolor = RA8875_colorInterpolation(ra8875->_FNTgrandientColor1, ra8875->_FNTgrandientColor2,
							grandientIndex++, grandientLen);
				}
			}
		}
		if (buffer[i] == '\b') {
			backspace = 2;
		}
		if (backspace > 0 && !ra8875->_backTransparent) {
			ra8875->_FNCR1_Reg |= (1 << 6);    //set
			RA8875_writeRegister(RA8875_FNCR1, ra8875->_FNCR1_Reg);
			backspace--;
		} else {
			if (ra8875->_backTransparent == false) {
				ra8875->_FNCR1_Reg &= ~(1 << 6);    //clear
				RA8875_writeRegister(RA8875_FNCR1, ra8875->_FNCR1_Reg);
			}
		}

		if (!renderOn) {
			if ((buffer[i] & 0x80) == 0) {
				RA8875_charWrite(ra8875, buffer[i], interlineOffset);
			} else {
				RA8875_showUserChar(ra8875, buffer[i] & 0x7f, 0);
				ra8875->_cursorX += ra8875->_FNTwidth * (ra8875->_scaleX);
			}

		} else {
			RA8875_charWriteR(ra8875, buffer[i], interlineOffset, fcolor, bcolor); // user fonts
		}
	}   //end loop
	if (ra8875->_FNTgrandient) { //recover text color after colored text
		ra8875->_FNTgrandient = false;
//recover original text color
		if (!renderOn) {
			RA8875_setTextColor(ra8875, recoverColor, ra8875->_backColor);
		} else {
			fcolor = recoverColor;
		}
	}
}

/**************************************************************************/
/*!	PRIVATE
 This helper loop trough a text string and return how long is (in pixel)
 NOTE: It identify correctly println and /n & /r and forget non present chars
 */
/**************************************************************************/
int16_t RA8875_STRlen_helper(RA8875_struct *ra8875, const char* buffer, uint16_t len) {
	if (bitRead(ra8875->_TXTparameters, 7) == 0) {		//_renderFont not active
		return (len * ra8875->_FNTwidth);
	} else {		//_renderFont active
		int charIndex = -1;
		uint16_t i;
		if (len == 0)
			len = strlen(buffer);		//try to get data from string
		if (len == 0)
			return 0;		//better stop here
		if (ra8875->_FNTwidth > 0) {		// fixed width font
			return ((len * ra8875->_spaceCharWidth));
		} else {	// variable width, need to loop trough entire string!
			uint16_t totW = 0;
			for (i = 0; i < len; i++) {	//loop trough buffer
				if (buffer[i] == 32) {	//a space
					totW += ra8875->_spaceCharWidth;
				} else if (buffer[i] == 8) {	//a backspace
					totW -= (ra8875->_currentFont->chars[charIndex].image->image_width);
				} else if (buffer[i] != 13 && buffer[i] != 10 && buffer[i] != 32) {	//avoid special char
					charIndex = RA8875_getCharCode(ra8875, buffer[i]);
					if (charIndex > -1) {	//found!
#if defined(_FORCE_PROGMEM__)
							totW += (PROGMEM_read(&ra8875->_currentFont->chars[charIndex].image->image_width));
#else
						totW += (ra8875->_currentFont->chars[charIndex].image->image_width);
#endif
					}
				}	//inside permitted chars
			}		//buffer loop
			return totW;		//return data
		}		//end variable w font
	}
}

/**************************************************************************/
/*!	PRIVATE
 Here's the char render engine for uncompressed fonts, it actually render a single char.
 It's actually 2 functions, this one take care of every glyph line
 and perform some optimization second one paint concurrent pixels in chunks.
 To show how optimizations works try uncomment RA8875_VISPIXDEBUG in settings.
 Please do not steal this part of code!
 */
/**************************************************************************/
void RA8875_drawChar_unc(RA8875_struct *ra8875, int16_t x, int16_t y, int charW, int index, uint16_t fcolor) {
//start by getting some glyph data...
#if defined(_FORCE_PROGMEM__)
	const uint8_t * charGlyp = PROGMEM_read(&ra8875->_currentFont->chars[index].image->data); //char data
	int totalBytes = PROGMEM_read(&ra8875->_currentFont->chars[index].image->image_datalen);
#else
	const uint8_t * charGlyp = ra8875->_currentFont->chars[index].image->data;
	int totalBytes = ra8875->_currentFont->chars[index].image->image_datalen;
#endif
	int i;
	uint8_t temp = 0;
//some basic variable...
	uint8_t currentXposition = 0; //the current position of the writing cursor in the x axis, from 0 to charW
	uint8_t currentYposition = 1; //the current position of the writing cursor in the y axis, from 1 to _FNTheight
	int currentByte = 0; //the current byte in reading (from 0 to totalBytes)
	bool lineBuffer[charW]; //the temporary line buffer (will be _FNTheight each char)
	int lineChecksum = 0; //part of the optimizer
	/*
	 uint8_t bytesInLine = 0;
	 //try to understand how many bytes in a line
	 if (charW % 8 == 0) {	// divisible by 8
	 bytesInLine = charW / 8;
	 } else {						// when it's divisible by 8?
	 bytesInLine = charW;
	 while (bytesInLine % 8) { bytesInLine--;}
	 bytesInLine = bytesInLine / 8;
	 }
	 */
//the main loop that will read all bytes of the glyph
	while (currentByte < totalBytes) {
		//read n byte
#if defined(_FORCE_PROGMEM__)
		temp = PROGMEM_read(&charGlyp[currentByte]);
#else
		temp = charGlyp[currentByte];
#endif
		for (i = 7; i >= 0; i--) {
//----------------------------------- exception
			if (currentXposition >= charW) {
				//line buffer has been filled!
				currentXposition = 0; //reset the line x position
				if (lineChecksum < 1) {	//empty line
#if defined(RA8875_VISPIXDEBUG)
						drawRect(x,y + (currentYposition * _scaleY),charW * _scaleX,_scaleY,RA8875_BLUE);
#endif
				} else if (lineChecksum == charW) {	//full line
#if !defined(RA8875_VISPIXDEBUG)
					RA8875_fillRect(ra8875,
#else
							RA8875_drawRect(ra8875,
#endif
							x, y + (currentYposition * ra8875->_scaleY), charW * ra8875->_scaleX, ra8875->_scaleY,
							fcolor);
				} else { //line render
					RA8875_charLineRender(ra8875, lineBuffer, charW, x, y, currentYposition, fcolor);
				}
				currentYposition++; //next line
				lineChecksum = 0; //reset checksum
			} //end exception
//-------------------------------------------------------
			lineBuffer[currentXposition] = bitRead(temp, i);              //continue fill line buffer
			lineChecksum += lineBuffer[currentXposition];
			currentXposition++;
		}
		currentByte++;
	}
}

/**************************************************************************/
/*!	PRIVATE
 Font Line render optimized routine
 This will render ONLY a single font line by grouping chunks of same pixels
 Version 3.0 (fixed a bug that cause xlinePos to jump of 1 pixel
 */
/**************************************************************************/
void RA8875_charLineRender(RA8875_struct *ra8875,
bool lineBuffer[], int charW, int16_t x, int16_t y, int16_t currentYposition, uint16_t fcolor) {
	int xlinePos = 0;
	int px;
	uint8_t endPix = 0;
	bool refPixel = false;
	while (xlinePos < charW) {
		refPixel = lineBuffer[xlinePos]; //xlinePos pix as reference value for next pixels
		//detect and render concurrent pixels
		for (px = xlinePos; px <= charW; px++) {
			if (lineBuffer[px] == lineBuffer[xlinePos] && px < charW) {	//grouping pixels with same val
				endPix++;
			} else {
				if (refPixel) {
#if defined(RA8875_VISPIXDEBUG)
					drawRect(
#else
					RA8875_fillRect(ra8875,
#endif
							x, y + (currentYposition * ra8875->_scaleY), endPix * ra8875->_scaleX, ra8875->_scaleY,
							fcolor);
				} else {
#if defined(RA8875_VISPIXDEBUG)
					drawRect(x,y + (currentYposition *ra8875-> _scaleY),endPix *ra8875-> _scaleX,ra8875->_scaleY,RA8875_BLUE);
#endif
				}
				//reset and update some vals
				xlinePos += endPix;
				x += endPix * ra8875->_scaleX;
				endPix = 0;
				break;	//exit cycle for...
			}
		}
	}
}

/**************************************************************************/
/*!
 interpolate 2 16bit colors
 return a 16bit mixed color between the two
 Parameters:
 color1:
 color2:
 pos:0...div (mix percentage) (0:color1, div:color2)
 div:divisions between color1 and color 2
 */
/**************************************************************************/
uint16_t RA8875_colorInterpolation(uint16_t color1, uint16_t color2, uint16_t pos, uint16_t div) {
	if (pos == 0)
		return color1;
	if (pos >= div)
		return color2;
	uint8_t r1, g1, b1;
	Color565ToRGB(color1, &r1, &g1, &b1);	//split in r,g,b
	uint8_t r2, g2, b2;
	Color565ToRGB(color2, &r2, &g2, &b2);	//split in r,g,b
	return RA8875_colorInterpolationRGB(r1, g1, b1, r2, g2, b2, pos, div);
}

/**************************************************************************/
/*!
 interpolate 2 r,g,b colors
 return a 16bit mixed color between the two
 Parameters:
 r1.
 g1:
 b1:
 r2:
 g2:
 b2:
 pos:0...div (mix percentage) (0:color1, div:color2)
 div:divisions between color1 and color 2
 */
/**************************************************************************/
uint16_t RA8875_colorInterpolationRGB(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2,
		uint16_t pos, uint16_t div) {
	if (pos == 0)
		return Color565(r1, g1, b1);
	if (pos >= div)
		return Color565(r2, g2, b2);
	float pos2 = (float) pos / (float) div;
	return Color565((uint8_t) (((1.0 - pos2) * r1) + (pos2 * r2)), (uint8_t) ((1.0 - pos2) * g1 + (pos2 * g2)),
			(uint8_t) (((1.0 - pos2) * b1) + (pos2 * b2)));
}

/**************************************************************************/
/*!
 choose an external font that will be rendered
 Of course slower that internal fonts!
 Parameters:
 *font: &myfont
 */
/**************************************************************************/
void RA8875_setFontExt(RA8875_struct *ra8875, const tFont *font) {
	ra8875->_currentFont = font;
	ra8875->_FNTheight = ra8875->_currentFont->font_height;
	ra8875->_FNTwidth = ra8875->_currentFont->font_width; //if 0 it's variable width font
	ra8875->_FNTcompression = ra8875->_currentFont->rle;
//get all needed infos
	if (ra8875->_FNTwidth > 0) {
		ra8875->_spaceCharWidth = ra8875->_FNTwidth;
	} else {
		//_FNTwidth will be 0 to inform other functions that this it's a variable w font
		// We just get the space width now...
		int temp = RA8875_getCharCode(ra8875, 0x20);
		if (temp > -1) {
#if defined(_FORCE_PROGMEM__)
			ra8875-> _spaceCharWidth = PROGMEM_read(&_currentFont->chars[temp].image->image_width);
#else
			ra8875->_spaceCharWidth = (ra8875->_currentFont->chars[temp].image->image_width);
#endif
		} else {
//font malformed, doesn't have needed space parameter
//will return to system font
			RA8875_setFont(ra8875, INT);
			return;
		}
	}
	ra8875->_scaleX = 1;
	ra8875->_scaleY = 1;			//reset font scale
//setFontScale(0);
	ra8875->_TXTparameters |= (1 << 7);	//render ON
}

/**************************************************************************/
/*!
 Enable/Disable the Font Full Alignemet feature (default off)
 Parameters:
 align: true,false
 Note: not active with rendered fonts
 */
/**************************************************************************/
void RA8875_setFontFullAlign(RA8875_struct *ra8875, boolean align) {
	if (bitRead(ra8875->_TXTparameters, 7) == 0) {
		if (align)
			ra8875->_FNCR1_Reg |= (1 << 7);
		else
			ra8875->_FNCR1_Reg &= ~(1 << 7);
		RA8875_writeRegister(RA8875_FNCR1, ra8875->_FNCR1_Reg);
	}
}

/**************************************************************************/
/*!
 Set the Text size by it's multiple. normal should=0, max is 3 (x4) for internal fonts
 With Rendered fonts the max scale it's not limited
 Parameters:
 scale: 0..3  -> 0:normal, 1:x2, 2:x3, 3:x4
 */
/**************************************************************************/
void RA8875_setFontScale(RA8875_struct *ra8875, uint8_t scale) {
	RA8875_setFontScaleXY(ra8875, scale, scale);
}

/**************************************************************************/
/*!
 Set the Text size by it's multiple. normal should=0, max is 3 (x4) for internal fonts
 With Rendered fonts the max scale it's not limited
 This time you can specify different values for vertical and horizontal
 Parameters:
 xscale: 0..3  -> 0:normal, 1:x2, 2:x3, 3:x4 for internal fonts - 0...xxx for Rendered Fonts
 yscale: 0..3  -> 0:normal, 1:x2, 2:x3, 3:x4 for internal fonts - 0...xxx for Rendered Fonts
 */
/**************************************************************************/
void RA8875_setFontScaleXY(RA8875_struct *ra8875, uint8_t xscale, uint8_t yscale) {
	ra8875->_scaling = false;
	if (bitRead(ra8875->_TXTparameters, 7) == 0) {
		xscale = xscale % 4; //limit to the range 0-3
		yscale = yscale % 4; //limit to the range 0-3
		ra8875->_FNCR1_Reg &= ~(0xF); // clear bits from 0 to 3
		ra8875->_FNCR1_Reg |= xscale << 2;
		ra8875->_FNCR1_Reg |= yscale;
		RA8875_writeRegister(RA8875_FNCR1, ra8875->_FNCR1_Reg);
	}
	ra8875->_scaleX = xscale + 1;
	ra8875->_scaleY = yscale + 1;
	if (ra8875->_scaleX > 1 || ra8875->_scaleY > 1)
		ra8875->_scaling = true;
}

/**************************************************************************/
/*!
 select the font family for the external Font Rom Chip
 Parameters:
 erf: STANDARD, ARIAL, ROMAN, BOLD
 setReg:
 true(send phisically the register, useful when you change
 family after set setExternalFontRom)
 false:(change only the register container, useful during config)
 NOTE: works only when external font rom it's active
 */
/**************************************************************************/
void RA8875_setExtFontFamily(RA8875_struct *ra8875, enum RA8875extRomFamily erf, boolean setReg) {
	if (ra8875->_FNTsource == EXT) {		//only on EXT ROM fonts!
		ra8875->_EXTFNTfamily = erf;
		ra8875->_SFRSET_Reg &= ~(0x03);		// clear bits from 0 to 1
		switch (erf) {	//check rom font family
		case STANDARD:
			ra8875->_SFRSET_Reg &= 0xFC;
			break;
		case ARIAL:
			ra8875->_SFRSET_Reg &= 0xFC;
			ra8875->_SFRSET_Reg |= 0x01;
			break;
		case ROMAN:
			ra8875->_SFRSET_Reg &= 0xFC;
			ra8875->_SFRSET_Reg |= 0x02;
			break;
		case BOLD:
			ra8875->_SFRSET_Reg |= ((1 << 1) | (1 << 0));	// set bits 1 and 0
			break;
		default:
			ra8875->_EXTFNTfamily = STANDARD;
			ra8875->_SFRSET_Reg &= 0xFC;
			return;
		}
		if (setReg)
			RA8875_writeRegister(RA8875_SFRSET, ra8875->_SFRSET_Reg);
	}
}
/**************************************************************************/
/*!
 External Font Rom setup
 This will not phisically change the register but should be called before setFont(EXT)!
 You should use this values accordly Font ROM datasheet!
 Parameters:
 ert: ROM Type          (GT21L16T1W, GT21H16T1W, GT23L16U2W, GT30H24T3Y, GT23L24T3Y, GT23L24M1Z, GT23L32S4W, GT30H32S4W)
 erc: ROM Font Encoding (GB2312, GB12345, BIG5, UNICODE, ASCII, UNIJIS, JIS0208, LATIN)
 erf: ROM Font Family   (STANDARD, ARIAL, ROMAN, BOLD)
 */
/**************************************************************************/

void RA8875_setExternalFontRom(RA8875_struct *ra8875, enum RA8875extRomType ert, enum RA8875extRomCoding erc,
		enum RA8875extRomFamily erf) {
	if (!ra8875->_textMode)
		RA8875_setTextMode(ra8875, true);
	;
	ra8875->_SFRSET_Reg = RA8875_readRegister(RA8875_FNCR0);
	; //just to preserve the reg in case something wrong
	uint8_t temp = 0b00000000;
	switch (ert) { //type of rom
	case GT21L16T1W:
	case GT21H16T1W:
		temp &= 0x1F;
		break;
	case GT23L16U2W:
	case GT30L16U2W:
	case ER3301_1:
		temp &= 0x1F;
		temp |= 0x20;
		break;
	case GT23L24T3Y:
	case GT30H24T3Y:
	case ER3303_1: //encoding GB12345
		temp &= 0x1F;
		temp |= 0x40;
		break;
	case GT23L24M1Z:
		temp &= 0x1F;
		temp |= 0x60;
		break;
	case GT23L32S4W:
	case GT30H32S4W:
	case GT30L32S4W:
	case ER3304_1: //encoding GB2312
		temp &= 0x1F;
		temp |= 0x80;
		break;
	default:
		ra8875->_TXTparameters &= ~(1 << 0); //wrong type, better avoid for future
		return; //cannot continue, exit
	}
	ra8875->_EXTFNTrom = ert;
	switch (erc) {	//check rom font coding
	case GB2312:
		temp &= 0xE3;
		break;
	case GB12345:
		temp &= 0xE3;
		temp |= 0x04;
		break;
	case BIG5:
		temp &= 0xE3;
		temp |= 0x08;
		break;
	case UNICODE:
		temp &= 0xE3;
		temp |= 0x0C;
		break;
	case ASCII:
		temp &= 0xE3;
		temp |= 0x10;
		break;
	case UNIJIS:
		temp &= 0xE3;
		temp |= 0x14;
		break;
	case JIS0208:
		temp &= 0xE3;
		temp |= 0x18;
		break;
	case LATIN:
		temp &= 0xE3;
		temp |= 0x1C;
		break;
	default:
		ra8875->_TXTparameters &= ~(1 << 0);	//wrong coding, better avoid for future
		return;	//cannot continue, exit
	}
	ra8875->_EXTFNTcoding = erc;
	ra8875->_SFRSET_Reg = temp;
	RA8875_setExtFontFamily(ra8875, erf, false);
	ra8875->_TXTparameters |= (1 << 0); //bit set 0
	RA8875_writeRegister(RA8875_SFRSET, ra8875->_SFRSET_Reg); //0x2F
//// vTaskDelay(4);
}

/**************************************************************************/
/*!
 helper function for rounded Rects
 PARAMETERS
 x:
 y:
 w:
 h:
 r:
 color:
 filled:
 [private]
 */
/**************************************************************************/
void RA8875_roundRect_helper(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
		uint16_t color, bool filled) {

	if (ra8875->_portrait) {
		swapvals_int16_t(x, y);
		swapvals_int16_t(w, h);
	}

	if (ra8875->_textMode)
		RA8875_setTextMode(ra8875, false);
//we are in text mode?

	ra8875->_TXTrecoverColor = true;
	if (color != ra8875->_foreColor)
		RA8875_setForegroundColor(ra8875, color); //0.69b30 avoid several SPI calls

	RA8875_line_addressing(ra8875, x, y, w, h);

	RA8875_writeRegister(RA8875_ELL_A0, r & 0xFF);
	RA8875_writeRegister(
	RA8875_ELL_A0 + 1, r >> 8);
	RA8875_writeRegister(RA8875_ELL_B0, r & 0xFF);
	RA8875_writeRegister(
	RA8875_ELL_B0 + 1, r >> 8);

	RA8875_writeCommand(RA8875_ELLIPSE);
	if (filled)
		RA8875_writeData(0xE0);
	else
		RA8875_writeData(0xA0);
	RA8875_waitPoll(RA8875_ELLIPSE, RA8875_DCR_LINESQUTRI_STATUS);
}

/**************************************************************************/
/*!
 Draw a rounded rectangle
 Parameters:
 x:   x location of the rectangle
 y:   y location of the rectangle
 w:  the width in pix
 h:  the height in pix
 r:  the radius of the rounded corner
 color: RGB565 color
 _roundRect_helper it's not tolerant to improper values
 so there's some value check here
 */
/**************************************************************************/
void RA8875_drawRoundRect(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
	if (r == 0)
		RA8875_drawRect(ra8875, x, y, w, h, color);
	if (w < 1 || h < 1)
		return; //it cannot be!
	if (w < 2 && h < 2) { //render as pixel
		RA8875_drawPixel(ra8875, x, y, color);
	} else {		//render as rect
		if (w < h && (r * 2) >= w)
			r = (w / 2) - 1;
		if (w > h && (r * 2) >= h)
			r = (h / 2) - 1;
		if (r == w || r == h)
			RA8875_drawRect(ra8875, x, y, w, h, color);
		RA8875_roundRect_helper(ra8875, x, y, (x + w) - 1, (y + h) - 1, r, color,
		false);
	}
}

/**************************************************************************/
/*!
 Draw a filled rounded rectangle
 Parameters:
 x:   x location of the rectangle
 y:   y location of the rectangle
 w:  the width in pix
 h:  the height in pix
 r:  the radius of the rounded corner
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_fillRoundRect(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
	if (r == 0)
		RA8875_fillRect(ra8875, x, y, w, h, color);
	if (w < 1 || h < 1)
		return;		//it cannot be!
	if (w < 2 && h < 2) { //render as pixel
		RA8875_drawPixel(ra8875, x, y, color);
	} else {		//render as rect
		if (w < h && (r * 2) >= w)
			r = (w / 2) - 1;
		if (w > h && (r * 2) >= h)
			r = (h / 2) - 1;
		if (r == w || r == h)
			RA8875_drawRect(ra8875, x, y, w, h, color);
		RA8875_roundRect_helper(ra8875, x, y, (x + w) - 1, (y + h) - 1, r, color, true);
	}
}

/**************************************************************************/
/*!
 Draw filled circle
 Parameters:
 x0: The 0-based x location of the center of the circle
 y0: The 0-based y location of the center of the circle
 r: radius
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_fillCircle(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t r, uint16_t color) {
//	RA8875_center_helper(ra8875, b x0, y0);
	if (r <= 0)
		return;
	RA8875_circle_helper(ra8875, x0, y0, r, color, true);
}

/**************************************************************************/
/*!
 Draw an ellipse
 Parameters:
 xCenter:   x location of the center of the ellipse
 yCenter:   y location of the center of the ellipse
 longAxis:  Size in pixels of the long axis
 shortAxis: Size in pixels of the short axis
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_drawEllipse(RA8875_struct *ra8875, int16_t xCenter, int16_t yCenter, int16_t longAxis, int16_t shortAxis,
		uint16_t color) {
	RA8875_ellipseCurve_helper(ra8875, xCenter, yCenter, longAxis, shortAxis, 255, color,
	false);
}

/**************************************************************************/
/*!
 Draw a filled ellipse
 Parameters:
 xCenter:   x location of the center of the ellipse
 yCenter:   y location of the center of the ellipse
 longAxis:  Size in pixels of the long axis
 shortAxis: Size in pixels of the short axis
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_fillEllipse(RA8875_struct *ra8875, int16_t xCenter, int16_t yCenter, int16_t longAxis, int16_t shortAxis,
		uint16_t color) {
	RA8875_ellipseCurve_helper(ra8875, xCenter, yCenter, longAxis, shortAxis, 255, color,
	true);
}

/**************************************************************************/
/*!
 helper function for ellipse and curve
 [private]
 */
/**************************************************************************/
void RA8875_ellipseCurve_helper(RA8875_struct *ra8875, int16_t xCenter, int16_t yCenter, int16_t longAxis,
		int16_t shortAxis, uint8_t curvePart, uint16_t color, bool filled) {

//_center_helper(xCenter, yCenter);	//use CENTER?

	if (ra8875->_portrait) {
		swapvals_int16_t(xCenter, yCenter);
		swapvals_int16_t(longAxis, shortAxis);
		if (longAxis > ra8875->RA8875_HEIGHT / 2)
			longAxis = (ra8875->RA8875_HEIGHT / 2) - 1;
		if (shortAxis > ra8875->RA8875_WIDTH / 2)
			shortAxis = (ra8875->RA8875_WIDTH / 2) - 1;
	} else {
		if (longAxis > ra8875->RA8875_WIDTH / 2)
			longAxis = (ra8875->RA8875_WIDTH / 2) - 1;
		if (shortAxis > ra8875->RA8875_HEIGHT / 2)
			shortAxis = (ra8875->RA8875_HEIGHT / 2) - 1;
	}
	if (longAxis == 1 && shortAxis == 1) {
		RA8875_drawPixel(ra8875, xCenter, yCenter, color);
		return;
	}
	RA8875_checkLimits_helper(ra8875, &xCenter, &yCenter);

	if (ra8875->_textMode)
		RA8875_setTextMode(ra8875, false);
	;	//we are in text mode?

#if defined(USE_RA8875_SEPARATE_TEXT_COLOR)
	ra8875->_TXTrecoverColor = true;
#endif
	if (color != ra8875->_foreColor)
		RA8875_setForegroundColor(ra8875, color);

	RA8875_curve_addressing(xCenter, yCenter, longAxis, shortAxis);
	RA8875_writeCommand(RA8875_ELLIPSE);

	if (curvePart != 255) {
		curvePart = curvePart % 4; //limit to the range 0-3
		filled == true ? RA8875_writeData(0xD0 | (curvePart & 0x03)) : RA8875_writeData(0x90 | (curvePart & 0x03));
	} else {
		filled == true ? RA8875_writeData(0xC0) : RA8875_writeData(0x80);
	}
	RA8875_waitPoll(RA8875_ELLIPSE, RA8875_ELLIPSE_STATUS);
}

/**************************************************************************/
/*!
 curve addressing helper
 [private]
 */
/**************************************************************************/
void RA8875_curve_addressing(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
//center
	RA8875_writeRegister(RA8875_DEHR0, x0 & 0xFF);
	RA8875_writeRegister(RA8875_DEHR0 + 1, x0 >> 8);
	RA8875_writeRegister(RA8875_DEVR0, y0 & 0xFF);
	RA8875_writeRegister(RA8875_DEVR0 + 1, y0 >> 8);
//long,short ax
	RA8875_writeRegister(RA8875_ELL_A0, x1 & 0xFF);
	RA8875_writeRegister(RA8875_ELL_A0 + 1, x1 >> 8);
	RA8875_writeRegister(RA8875_ELL_B0, y1 & 0xFF);
	RA8875_writeRegister(RA8875_ELL_B0 + 1, y1 >> 8);
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +					GEOMETRIC PRIMITIVE HELPERS STUFF								 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
/**************************************************************************/
/*!
 helper function for circles
 [private]
 */
/**************************************************************************/
void RA8875_circle_helper(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t r, uint16_t color,
bool filled)		//0.69b32 fixed an undocumented hardware limit
{
	if (ra8875->_portrait)
		swapvals_int16_t(x0, y0);		//0.69b21

	if (r < 1)
		r = 1;
	if (r < 2) {			 //NEW
		RA8875_drawPixel(ra8875, x0, y0, color);
		return;
	}
	if (r > ra8875->RA8875_HEIGHT / 2)
		r = (ra8875->RA8875_HEIGHT / 2) - 1;			 //this is the (undocumented) hardware limit of RA8875

	if (ra8875->_textMode)
		RA8875_setTextMode(ra8875, false);
	;	//we are in text mode?
	ra8875->_TXTrecoverColor = true;
	if (color != ra8875->_foreColor)
		RA8875_setForegroundColor(ra8875, color);	//0.69b30 avoid several SPI calls

	RA8875_writeRegister(RA8875_DCHR0, x0 & 0xFF);
	RA8875_writeRegister(RA8875_DCHR0 + 1, x0 >> 8);

	RA8875_writeRegister(RA8875_DCVR0, y0 & 0xFF);
	RA8875_writeRegister(RA8875_DCVR0 + 1, y0 >> 8);
	RA8875_writeRegister(RA8875_DCRR, r);

	RA8875_writeCommand(RA8875_DCR);

	if (filled) {
		RA8875_writeData(
		RA8875_DCR_CIRCLE_START | RA8875_DCR_FILL);
	} else {
		RA8875_writeData(
		RA8875_DCR_CIRCLE_START | RA8875_DCR_NOFILL);
	}
	RA8875_waitPoll(RA8875_DCR,
	RA8875_DCR_CIRCLE_STATUS);		//ZzZzz

}
/***************************************************************************/
/* !
 *  Drw arc funtion
 */
void RA8875_drawArc(RA8875_struct *ra8875, uint16_t cx, uint16_t cy, uint16_t radius, uint16_t thickness, float start,
		float end, uint16_t color) {
	if (start == 0 && end == ra8875->_arcAngle_max) {
		RA8875_drawArc_helper(ra8875, cx, cy, radius, thickness, 0, ra8875->_arcAngle_max, color);
	} else {
		RA8875_drawArc_helper(ra8875, cx, cy, radius, thickness,
				start + (ra8875->_arcAngle_offset / (float) 360) * ra8875->_arcAngle_max,
				end + (ra8875->_arcAngle_offset / (float) 360) * ra8875->_arcAngle_max, color);
	}
}

/**************************************************************************/
/*!
 helper function for draw arcs in degrees
 DrawArc function thanks to Jnmattern and his Arc_2.0 (https://github.com/Jnmattern)
 Adapted for DUE by Marek Buriak https://github.com/marekburiak/ILI9341_Due
 Re-Adapted for this library by sumotoy
 PARAMETERS
 cx: center x
 cy: center y
 radius: the radius of the arc
 thickness:
 start: where arc start in degrees
 end:	 where arc end in degrees
 color:
 [private]
 */
/**************************************************************************/
void RA8875_drawArc_helper(RA8875_struct *ra8875, uint16_t cx, uint16_t cy, uint16_t radius, uint16_t thickness,
		float start, float end, uint16_t color) {

//_center_helper(cx,cy);//use CENTER?
	int16_t xmin = 65535, xmax = -32767, ymin = 32767, ymax = -32767;
	float cosStart, sinStart, cosEnd, sinEnd;
	float r, t;
	float startAngle, endAngle;

	startAngle = (start / ra8875->_arcAngle_max) * 360;		// 252
	endAngle = (end / ra8875->_arcAngle_max) * 360;		// 807

	while (startAngle < 0)
		startAngle += 360;
	while (endAngle < 0)
		endAngle += 360;
	while (startAngle > 360)
		startAngle -= 360;
	while (endAngle > 360)
		endAngle -= 360;

	if (startAngle > endAngle) {
		RA8875_drawArc_helper(ra8875, cx, cy, radius, thickness, ((startAngle) / (float) 360) * ra8875->_arcAngle_max,
				ra8875->_arcAngle_max, color);
		RA8875_drawArc_helper(ra8875, cx, cy, radius, thickness, 0, ((endAngle) / (float) 360) * ra8875->_arcAngle_max,
				color);
	} else {
//if (_textMode) RA8875_setTextMode(ra8875, false);;//we are in text mode?
// Calculate bounding box for the arc to be drawn
		cosStart = RA8875_cosDeg_helper(startAngle);
		sinStart = RA8875_sinDeg_helper(startAngle);
		cosEnd = RA8875_cosDeg_helper(endAngle);
		sinEnd = RA8875_sinDeg_helper(endAngle);

		r = radius;
// Point 1: radius & startAngle
		t = r * cosStart;
		if (t < xmin)
			xmin = t;
		if (t > xmax)
			xmax = t;
		t = r * sinStart;
		if (t < ymin)
			ymin = t;
		if (t > ymax)
			ymax = t;

// Point 2: radius & endAngle
		t = r * cosEnd;
		if (t < xmin)
			xmin = t;
		if (t > xmax)
			xmax = t;
		t = r * sinEnd;
		if (t < ymin)
			ymin = t;
		if (t > ymax)
			ymax = t;

		r = radius - thickness;
// Point 3: radius-thickness & startAngle
		t = r * cosStart;
		if (t < xmin)
			xmin = t;
		if (t > xmax)
			xmax = t;
		t = r * sinStart;
		if (t < ymin)
			ymin = t;
		if (t > ymax)
			ymax = t;

// Point 4: radius-thickness & endAngle
		t = r * cosEnd;
		if (t < xmin)
			xmin = t;
		if (t > xmax)
			xmax = t;
		t = r * sinEnd;
		if (t < ymin)
			ymin = t;
		if (t > ymax)
			ymax = t;
// Corrections if arc crosses X or Y axis
		if ((startAngle < 90) && (endAngle > 90))
			ymax = radius;
		if ((startAngle < 180) && (endAngle > 180))
			xmin = -radius;
		if ((startAngle < 270) && (endAngle > 270))
			ymin = -radius;

// Slopes for the two sides of the arc
		float sslope = (float) cosStart / (float) sinStart;
		float eslope = (float) cosEnd / (float) sinEnd;
		if (endAngle == 360)
			eslope = -1000000;
		int ir2 = (radius - thickness) * (radius - thickness);
		int or2 = radius * radius;
		for (int x = xmin; x <= xmax; x++) {
			bool y1StartFound = false, y2StartFound = false;
			bool y1EndFound = false, y2EndSearching = false;
			int y1s = 0, y1e = 0, y2s = 0;	//, y2e = 0;
			for (int y = ymin; y <= ymax; y++) {
				int x2 = x * x;
				int y2 = y * y;

				if ((x2 + y2 < or2 && x2 + y2 >= ir2)
						&& ((y > 0 && startAngle < 180 && x <= y * sslope)
								|| (y < 0 && startAngle > 180 && x >= y * sslope) || (y < 0 && startAngle <= 180)
								|| (y == 0 && startAngle <= 180 && x < 0) || (y == 0 && startAngle == 0 && x > 0))
						&& ((y > 0 && endAngle < 180 && x >= y * eslope) || (y < 0 && endAngle > 180 && x <= y * eslope)
								|| (y > 0 && endAngle >= 180) || (y == 0 && endAngle >= 180 && x < 0)
								|| (y == 0 && startAngle == 0 && x > 0))) {
					if (!y1StartFound) {	//start of the higher line found
						y1StartFound = true;
						y1s = y;
					} else if (y1EndFound && !y2StartFound) {	//start of the lower line found
						y2StartFound = true;
						y2s = y;
						y += y1e - y1s - 1;	// calculate the most probable end of the lower line (in most cases the length of lower line is equal to length of upper line), in the next loop we will validate if the end of line is really there
						if (y > ymax - 1) {	// the most probable end of line 2 is beyond ymax so line 2 must be shorter, thus continue with pixel by pixel search
							y = y2s;	// reset y and continue with pixel by pixel search
							y2EndSearching = true;
						}
					} else if (y2StartFound && !y2EndSearching) {
						// we validated that the probable end of the lower line has a pixel, continue with pixel by pixel search, in most cases next loop with confirm the end of lower line as it will not find a valid pixel
						y2EndSearching = true;
					}
				} else {
					if (y1StartFound && !y1EndFound) {	//higher line end found
						y1EndFound = true;
						y1e = y - 1;
						RA8875_drawFastVLine(ra8875, cx + x, cy + y1s, y - y1s, color);
						if (y < 0) {
							y = y * -1; // skip the empty middle
						} else
							break;
					} else if (y2StartFound) {
						if (y2EndSearching) {
							// we found the end of the lower line after pixel by pixel search
							RA8875_drawFastVLine(ra8875, cx + x, cy + y2s, y - y2s, color);
							y2EndSearching = false;
							break;
						} else {
							// the expected end of the lower line is not there so the lower line must be shorter
							y = y2s; // put the y back to the lower line start and go pixel by pixel to find the end
							y2EndSearching = true;
						}
					}
				}
			}
			if (y1StartFound && !y1EndFound) {
				y1e = ymax;
				RA8875_drawFastVLine(ra8875, cx + x, cy + y1s, y1e - y1s + 1, color);
			} else if (y2StartFound && y2EndSearching) { // we found start of lower line but we are still searching for the end
														 // which we haven't found in the loop so the last pixel in a column must be the end
				RA8875_drawFastVLine(ra8875, cx + x, cy + y2s, ymax - y2s + 1, color);
			}
		}
	}
}

/**************************************************************************/
/*!
 for compatibility with popular Adafruit_GFX
 draws a single vertical line
 Parameters:
 x: horizontal start
 y: vertical start
 h: height
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_drawFastVLine(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t h, uint16_t color) {
	if (h < 1)
		h = 1;
	if (h < 2) {
		RA8875_drawPixel(ra8875, x, y, color);
	} else {
		RA8875_drawLine(ra8875, x, y, x, (y + h) - 1, color);
	}
}

/**************************************************************************/
/*!
 for compatibility with popular Adafruit_GFX
 draws a single orizontal line
 Parameters:
 x: horizontal start
 y: vertical start
 w: width
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_drawFastHLine(RA8875_struct *ra8875, int16_t x, int16_t y, int16_t w, uint16_t color) {
	if (w < 1)
		w = 1;
	if (w < 2) {
		RA8875_drawPixel(ra8875, x, y, color);
	} else {
		RA8875_drawLine(ra8875, x, y, (w + x) - 1, y, color);
	}
}

/**************************************************************************/
/*!
 Basic line draw
 Parameters:
 x0: horizontal start pos
 y0: vertical start
 x1: horizontal end pos
 y1: vertical end pos
 color: RGB565 color
 NOTE:
 Remember that this write from->to so: drawLine(0,0,2,0,RA8875_RED);
 result a 3 pixel long! (0..1..2)
 */
/**************************************************************************/
void RA8875_drawLine(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
	if ((x0 == x1 && y0 == y1) || ((x1 - x0 == 1) && (y1 - y0 == 1))) {	//NEW
		RA8875_drawPixel(ra8875, x0, y0, color);
		return;
	}
//if ((x1 - x0 == 1) && (y1 - y0 == 1)) RA8875_drawPixel(x0,y0,color);
	if (ra8875->_portrait) {
		swapvals_int16_t(x0, y0);
		swapvals_int16_t(x1, y1);
	}
	if (ra8875->_textMode)
		RA8875_setTextMode(ra8875, false);
	;	//we are in text mode?
	ra8875->_TXTrecoverColor = true;
	if (color != ra8875->_foreColor)
		RA8875_setForegroundColor(ra8875, color);	//0.69b30 avoid 3 useless SPI calls

	RA8875_line_addressing(ra8875, x0, y0, x1, y1);

	RA8875_writeRegister(RA8875_DCR, 0x80);
	RA8875_waitPoll(RA8875_DCR,
	RA8875_DCR_LINESQUTRI_STATUS);
}

/**************************************************************************/
/*! This is the most important function to write on:
 LAYERS
 CGRAM
 PATTERN
 CURSOR
 Parameter:
 d (L1, L2, CGRAM, PATTERN, CURSOR)
 When writing on layers 0 or 1, if the layers are not enable it will enable automatically
 If the display doesn't support layers, it will automatically switch to 8bit color
 Remember that when layers are ON you need to disable manually, once that only Layer 1 will be visible

 */
/**************************************************************************/
void RA8875_writeTo(RA8875_struct *ra8875, enum RA8875writes d) {
	uint8_t temp = RA8875_readRegister( RA8875_MWCR1);
//bool trigMultilayer = false;
	switch (d) {
	case L1:
		temp &= ~((1 << 3) | (1 << 2));	// Clear bits 3 and 2
		temp &= ~(1 << 0);	//clear bit 0
		ra8875->_currentLayer = 0;
//trigMultilayer = true;
		RA8875_writeData(temp);
		if (!ra8875->_useMultiLayers)
			RA8875_useLayers(ra8875, true);
		break;
	case L2:
		temp &= ~((1 << 3) | (1 << 2));	// Clear bits 3 and 2
		temp |= (1 << 0);	//bit set 0
		ra8875->_currentLayer = 1;
//trigMultilayer = true;
		RA8875_writeData(temp);
		if (!ra8875->_useMultiLayers)
			RA8875_useLayers(ra8875, true);
		break;
	case CGRAM:
		temp &= ~(1 << 3);	//clear bit 3
		temp |= (1 << 2);	//bit set 2
		if (bitRead(ra8875->_FNCR0_Reg, 7)) { //REG[0x21] bit7 must be 0
			ra8875->_FNCR0_Reg &= ~(1 << 7); //clear bit 7
			RA8875_writeRegister(RA8875_FNCR0, ra8875->_FNCR0_Reg);
			RA8875_writeRegister(RA8875_MWCR1, temp);
		} else {
			RA8875_writeData(temp);
		}
		temp = RA8875_readRegister( RA8875_MWCR1);
		break;
	case PATTERN:
		temp |= (1 << 3); //bit set 3
		temp |= (1 << 2); //bit set 2
		RA8875_writeData(temp);
		break;
	case CURSOR:
		temp |= (1 << 3); //bit set 3
		temp &= ~(1 << 2); //clear bit 2
		RA8875_writeData(temp);
		break;
	default:
//break;
		return;
	}
//if (trigMultilayer && !_useMultiLayers) useLayers(true);//turn on multiple layers if it's off
//RA8875_writeRegister(RA8875_MWCR1,temp);
}

/**************************************************************************/
/*!
 Instruct the RA8875 chip to use 2 layers
 If resolution bring to restrictions it will switch to 8 bit
 so you can always use layers.
 Parameters:
 on: true (enable multiple layers), false (disable)

 */
/**************************************************************************/
void RA8875_useLayers(RA8875_struct *ra8875, boolean on) {
	if (ra8875->_useMultiLayers == on)
		return; //no reason to do change that it's already as desidered.
//bool clearBuffer = false;
	if (ra8875->_hasLayerLimits && ra8875->_color_bpp > 8) { //try to set up 8bit color space
		RA8875_setColorBpp(ra8875, 8);
		//	RA8875_waitBusy();
		ra8875->_maxLayers = 2;
	}
	if (on) {
		ra8875->_useMultiLayers = true;
		ra8875->_DPCR_Reg |= (1 << 7);
//clearBuffer = true;
		RA8875_clearActiveWindow(ra8875, true);
	} else {
		ra8875->_useMultiLayers = false;
		ra8875->_DPCR_Reg &= ~(1 << 7);
		RA8875_clearActiveWindow(ra8875, false);

	}

	RA8875_writeRegister(RA8875_DPCR, ra8875->_DPCR_Reg);
	if (!ra8875->_useMultiLayers && ra8875->_color_bpp < 16)
		RA8875_setColorBpp(ra8875, 16); //bring color back to 16
				/*
				 if (clearBuffer) {
				 clearWindow(true);
				 //for some reason if you switch to multilayer the layer 2 has garbage better clear
				 //writeTo(L2);//switch to layer 2
				 //clearMemory(false);//clear memory of layer 2
				 //clearWindow(false);
				 //writeTo(L1);//switch to layer 1
				 }
				 */
}
/**************************************************************************/
/*!
 Set the display 'Color Space'
 Parameters:
 Bit per Pixel color (colors): 8 or 16 bit
 NOTE:
 For display over 272*480 give the ability to use
 Layers since at 16 bit it's not possible.
 */
/**************************************************************************/
void RA8875_setColorBpp(RA8875_struct *ra8875, uint8_t colors) {
	if (colors != ra8875->_color_bpp) { //only if necessary
		if (colors < 16) {
			ra8875->_color_bpp = 8;
			ra8875->_colorIndex = 3;
			RA8875_writeRegister(RA8875_SYSR, 0x00);
			if (ra8875->_hasLayerLimits)
				ra8875->_maxLayers = 2;
		} else if (colors > 8) {  //65K
			ra8875->_color_bpp = 16;
			ra8875->_colorIndex = 0;
			RA8875_writeRegister(RA8875_SYSR, 0x0C);
			if (ra8875->_hasLayerLimits)
				ra8875->_maxLayers = 1;
			ra8875->_currentLayer = 0;
		}
	}
}

/**************************************************************************/
/*!
 Return current Color Space (8 or 16)
 */
/**************************************************************************/
uint8_t RA8875_getColorBpp(RA8875_struct *ra8875) {
	return ra8875->_color_bpp;
}

/**************************************************************************/
/*!
 Clear the active window
 Parameters:
 full: false(clear current window), true clear full window
 */
/**************************************************************************/
void RA8875_clearActiveWindow(RA8875_struct *ra8875, bool full) {
	uint8_t temp;
	temp = RA8875_readRegister(RA8875_MCLR);
	if (full)
		temp &= ~(1 << 6);
	else
		temp |= (1 << 6);
	RA8875_writeData(temp);
//_waitBusy(0x80);
}

/**************************************************************************/
/*!
 Draw Triangle
 Parameters:
 x0: The 0-based x location of the point 0 of the triangle bottom LEFT
 y0: The 0-based y location of the point 0 of the triangle bottom LEFT
 x1: The 0-based x location of the point 1 of the triangle middle TOP
 y1: The 0-based y location of the point 1 of the triangle middle TOP
 x2: The 0-based x location of the point 2 of the triangle bottom RIGHT
 y2: The 0-based y location of the point 2 of the triangle bottom RIGHT
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_drawTriangle(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
		uint16_t color) {
	RA8875_triangle_helper(ra8875, x0, y0, x1, y1, x2, y2, color, false);
}
/**************************************************************************/
/*!
 Draw Triangle
 Parameters:
 x0: The 0-based x location of the point 0 of the triangle bottom LEFT
 y0: The 0-based y location of the point 0 of the triangle bottom LEFT
 x1: The 0-based x location of the point 1 of the triangle middle TOP
 y1: The 0-based y location of the point 1 of the triangle middle TOP
 x2: The 0-based x location of the point 2 of the triangle bottom RIGHT
 y2: The 0-based y location of the point 2 of the triangle bottom RIGHT
 color: RGB565 color
 */
/**************************************************************************/
void RA8875_fillTriangle(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
		uint16_t color) {
	RA8875_triangle_helper(ra8875, x0, y0, x1, y1, x2, y2, color, true);
}

/**************************************************************************/
/*!
 Draw a filled quadrilater by connecting 4 points
 Parameters:
 x0:
 y0:
 x1:
 y1:
 x2:
 y2:
 x3:
 y3:
 color: RGB565 color
 triangled: if true a full quad will be generated, false generate a low res quad (faster)
 *NOTE: a bug in _triangle_helper create some problem, still fixing....
 */
/**************************************************************************/
void RA8875_fillQuad(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
		int16_t x3, int16_t y3, uint16_t color,
		bool triangled) {
	RA8875_triangle_helper(ra8875, x0, y0, x1, y1, x2, y2, color, true);
	if (triangled)
		RA8875_triangle_helper(ra8875, x2, y2, x3, y3, x0, y0, color, true);
	RA8875_triangle_helper(ra8875, x1, y1, x2, y2, x3, y3, color, true);
}

/**************************************************************************/
/*!
 helper function for triangles
 [private]
 */
/**************************************************************************/
void RA8875_triangle_helper(RA8875_struct *ra8875, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
		int16_t y2, uint16_t color,
		bool filled) {
	if (x0 >= ra8875->_width || x1 >= ra8875->_width || x2 >= ra8875->_width)
		return;
	if (y0 >= ra8875->_height || y1 >= ra8875->_height || y2 >= ra8875->_height)
		return;

	if (ra8875->_portrait) {
		swapvals_int16_t(x0, y0);
		swapvals_int16_t(x1, y1);
		swapvals_int16_t(x2, y2);
	}

	if (x0 == x1 && y0 == y1) {
		RA8875_drawLine(ra8875, x0, y0, x2, y2, color);
		return;
	} else if (x0 == x2 && y0 == y2) {
		RA8875_drawLine(ra8875, x0, y0, x1, y1, color);
		return;
	} else if (x0 == x1 && y0 == y1 && x0 == x2 && y0 == y2) {      //new
		RA8875_drawPixel(ra8875, x0, y0, color);
		return;
	}

	if (y0 > y1) {
		swapvals_int16_t(y0, y1);
		swapvals_int16_t(x0, x1);
	}

	if (y1 > y2) {
		swapvals_int16_t(y2, y1);
		swapvals_int16_t(x2, x1);
	}

	if (y0 > y1) {
		swapvals_int16_t(y0, y1);
		swapvals_int16_t(x0, x1);
	}

	if (y0 == y2) { // Handle awkward all-on-same-line case as its own thing
		int16_t a, b;
		a = b = x0;
		if (x1 < a) {
			a = x1;
		} else if (x1 > b) {
			b = x1;
		}
		if (x2 < a) {
			a = x2;
		} else if (x2 > b) {
			b = x2;
		}
		RA8875_drawFastHLine(ra8875, a, y0, b - a + 1, color);
		return;
	}

	if (ra8875->_textMode)
		RA8875_setTextMode(ra8875, false);
	; //we are in text mode?

#if defined(USE_RA8875_SEPARATE_TEXT_COLOR)
	ra8875->_TXTrecoverColor = true;
#endif
	if (color != ra8875->_foreColor)
		RA8875_setForegroundColor(ra8875, color); //0.69b30 avoid several SPI calls

//RA8875_checkLimits_helper(RA8875_struct *ra8875,&x0,&y0);
//RA8875_checkLimits_helper(RA8875_struct *ra8875,&x1,&y1);

	RA8875_line_addressing(ra8875, x0, y0, x1, y1);
//p2

	RA8875_writeRegister(RA8875_DTPH0, x2 & 0xFF);
	RA8875_writeRegister(RA8875_DTPH0 + 1, x2 >> 8);
	RA8875_writeRegister(RA8875_DTPV0, y2 & 0xFF);
	RA8875_writeRegister(RA8875_DTPV0 + 1, y2 >> 8);

	RA8875_writeCommand(RA8875_DCR);
	filled == true ? RA8875_writeData(0xA1) : RA8875_writeData(0x81);

	RA8875_waitPoll(RA8875_DCR,
	RA8875_DCR_LINESQUTRI_STATUS);
}

#ifdef bla
RA8875_struct *ra8875,
ra8875->

/**************************************************************************/
/*!		
 Clear width BG color
 Parameters:
 bte: false(clear width BTE BG color), true(clear width font BG color)
 */
/**************************************************************************/
void RA8875::clearWidthColor(bool bte) {
	uint8_t temp;
	temp = _readRegister(RA8875_MCLR);
	bte == true ? temp &= ~(1 << 0) : temp |= (1 << 0);
	RA8875_writeData(temp);
//_waitBusy(0x80);
}

/**************************************************************************/
/*!
 turn display on/off
 */
/**************************************************************************/
void RA8875::displayOn(boolean on) {
	on == true ? RA8875_writeRegister(RA8875_PWRR,
			RA8875_PWRR_NORMAL | RA8875_PWRR_DISPON) :
	RA8875_writeRegister(RA8875_PWRR,
			RA8875_PWRR_NORMAL | RA8875_PWRR_DISPOFF);
}

/**************************************************************************/
/*!		
 Set the Active Window
 Parameters:
 XL: Horizontal Left
 XR: Horizontal Right
 YT: Vertical TOP
 YB: Vertical Bottom
 */
/**************************************************************************/
void RA8875::getActiveWindow(int16_t &XL, int16_t &XR, int16_t &YT, int16_t &YB) //0.69b24
{
	XL = _activeWindowXL;
	XR = _activeWindowXR;
	YT = _activeWindowYT;
	YB = _activeWindowYB;
}

/**************************************************************************/

/**************************************************************************/
/*!
 Get rotation setting
 */
/**************************************************************************/
uint8_t RA8875::getRotation() {
	return _rotation;
}

/**************************************************************************/
/*!
 true if rotation 1 or 3
 */
/**************************************************************************/
boolean RA8875::isPortrait(void) {
	return _portrait;
}

/**************************************************************************/
/*!  
 External Font Rom setup
 This will not phisically change the register but should be called before setFont(EXT)!
 You should use this values accordly Font ROM datasheet!
 Parameters:
 ert: ROM Type          (GT21L16T1W, GT21H16T1W, GT23L16U2W, GT30H24T3Y, GT23L24T3Y, GT23L24M1Z, GT23L32S4W, GT30H32S4W)
 erc: ROM Font Encoding (GB2312, GB12345, BIG5, UNICODE, ASCII, UNIJIS, JIS0208, LATIN)
 erf: ROM Font Family   (STANDARD, ARIAL, ROMAN, BOLD)
 */
/**************************************************************************/
void RA8875::setExternalFontRom(enum RA8875extRomType ert,
		enum RA8875extRomCoding erc, enum RA8875extRomFamily erf) {
	if (!_textMode)
	RA8875_setTextMode(ra8875, true);;
	_SFRSET_Reg = _readRegister(RA8875_FNCR0);
	; //just to preserve the reg in case something wrong
	uint8_t temp = 0b00000000;
	switch (ert) { //type of rom
		case GT21L16T1W:
		case GT21H16T1W:
		temp &= 0x1F;
		break;
		case GT23L16U2W:
		case GT30L16U2W:
		case ER3301_1:
		temp &= 0x1F;
		temp |= 0x20;
		break;
		case GT23L24T3Y:
		case GT30H24T3Y:
		case ER3303_1://encoding GB12345
		temp &= 0x1F;
		temp |= 0x40;
		break;
		case GT23L24M1Z:
		temp &= 0x1F;
		temp |= 0x60;
		break;
		case GT23L32S4W:
		case GT30H32S4W:
		case GT30L32S4W:
		case ER3304_1://encoding GB2312
		temp &= 0x1F;
		temp |= 0x80;
		break;
		default:
		_TXTparameters &= ~(1 << 0);//wrong type, better avoid for future
		return;//cannot continue, exit
	}
	ra8875->_EXTFNTrom = ert;
	switch (erc) {	//check rom font coding
		case GB2312:
		temp &= 0xE3;
		break;
		case GB12345:
		temp &= 0xE3;
		temp |= 0x04;
		break;
		case BIG5:
		temp &= 0xE3;
		temp |= 0x08;
		break;
		case UNICODE:
		temp &= 0xE3;
		temp |= 0x0C;
		break;
		case ASCII:
		temp &= 0xE3;
		temp |= 0x10;
		break;
		case UNIJIS:
		temp &= 0xE3;
		temp |= 0x14;
		break;
		case JIS0208:
		temp &= 0xE3;
		temp |= 0x18;
		break;
		case LATIN:
		temp &= 0xE3;
		temp |= 0x1C;
		break;
		default:
		_TXTparameters &= ~(1 << 0);//wrong coding, better avoid for future
		return;//cannot continue, exit
	}
	_EXTFNTcoding = erc;
	_SFRSET_Reg = temp;
	setExtFontFamily(erf, false);
	_TXTparameters |= (1 << 0); //bit set 0
//RA8875_writeRegister(RA8875_SFRSET,_SFRSET_Reg);//0x2F
//// vTaskDelay(4);
}

void RA8875::fontRomSpeed(uint8_t sp) {
	RA8875_writeRegister(0x28, sp);
}

/**************************************************************************/
/*!  
 select the font family for the external Font Rom Chip
 Parameters:
 erf: STANDARD, ARIAL, ROMAN, BOLD
 setReg:
 true(send phisically the register, useful when you change
 family after set setExternalFontRom)
 false:(change only the register container, useful during config)
 NOTE: works only when external font rom it's active
 */
/**************************************************************************/
void RA8875::setExtFontFamily(enum RA8875extRomFamily erf, boolean setReg) {
	if (_FNTsource == EXT) {		//only on EXT ROM fonts!
		_EXTFNTfamily = erf;
		_SFRSET_Reg &= ~(0x03);// clear bits from 0 to 1
		switch (erf) {	//check rom font family
			case STANDARD:
			_SFRSET_Reg &= 0xFC;
			break;
			case ARIAL:
			_SFRSET_Reg &= 0xFC;
			_SFRSET_Reg |= 0x01;
			break;
			case ROMAN:
			_SFRSET_Reg &= 0xFC;
			_SFRSET_Reg |= 0x02;
			break;
			case BOLD:
			_SFRSET_Reg |= ((1 << 1) | (1 << 0));// set bits 1 and 0
			break;
			default:
			_EXTFNTfamily = STANDARD;
			_SFRSET_Reg &= 0xFC;
			return;
		}
		if (setReg)
		RA8875_writeRegister(RA8875_SFRSET, _SFRSET_Reg);
	}
}

/**************************************************************************/
/*!
 Set distance between text lines (default off)
 Parameters:
 pix: 0...63 pixels
 Note: active with rendered fonts
 */
/**************************************************************************/
void RA8875::setFontInterline(uint8_t pix) {
	if (bitRead(_TXTparameters, 7) == 1) {
		_FNTinterline = pix;
	} else {
		if (pix > 0x3F)
		pix = 0x3F;
		_FNTinterline = pix;
//_FWTSET_Reg &= 0xC0;
//_FWTSET_Reg |= spc & 0x3F;
		RA8875_writeRegister(RA8875_FLDR, _FNTinterline);
	}
}

/**************************************************************************/

/**************************************************************************/
/*!
 Give you back the current text cursor position by reading inside RA8875
 Parameters:
 x: horizontal pos in pixels
 y: vertical pos in pixels
 note: works also with rendered fonts
 USE: xxx.getCursor(myX,myY);
 */
/**************************************************************************/
void RA8875::getCursor(int16_t &x, int16_t &y) {
	if (bitRead(_TXTparameters, 7) == 1) {
		getCursorFast(x, y);
	} else {
		uint8_t t1, t2, t3, t4;
		t1 = _readRegister(RA8875_F_CURXL);
		t2 = _readRegister(RA8875_F_CURXH);
		t3 = _readRegister(RA8875_F_CURYL);
		t4 = _readRegister(RA8875_F_CURYH);
		x = (t2 << 8) | (t1 & 0xFF);
		y = (t4 << 8) | (t3 & 0xFF);
		if (_portrait)
		swapvals(x, y);
	}
}

/**************************************************************************/
/*!
 Give you back the current text cursor position as tracked by library (fast)
 Parameters:
 x: horizontal pos in pixels
 y: vertical pos in pixels
 note: works also with rendered fonts
 USE: xxx.getCursor(myX,myY);
 */
/**************************************************************************/
void RA8875::getCursorFast(int16_t &x, int16_t &y) {
	x = _cursorX;
	y = _cursorY;
	if (_portrait)
	swapvals(x, y);
}

int16_t RA8875::getCursorX(void) {
	if (_portrait)
	return _cursorY;
	return _cursorX;
}

int16_t RA8875::getCursorY(void) {
	if (_portrait)
	return _cursorX;
	return _cursorY;
}
/**************************************************************************/
/*!     Show/Hide text cursor
 Parameters:
 c: cursor type (NOCURSOR,IBEAM,UNDER,BLOCK)
 note: not active with rendered fonts
 blink: true=blink cursor
 */
/**************************************************************************/
void RA8875::showCursor(enum RA8875tcursor c, bool blink) {
//uint8_t MWCR1Reg = _readRegister(RA8875_MWCR1) & 0x01;(needed?)
	uint8_t cW = 0;
	uint8_t cH = 0;
	_FNTcursorType = c;
	c == NOCURSOR ? _MWCR0_Reg &= ~(1 << 6) : _MWCR0_Reg |= (1 << 6);
	if (blink)
	_MWCR0_Reg |= 0x20;//blink or not?
	RA8875_writeRegister(RA8875_MWCR0, _MWCR0_Reg);//set cursor
//RA8875_writeRegister(RA8875_MWCR1, MWCR1Reg);//close graphic cursor(needed?)
	switch (c) {
		case IBEAM:
		cW = 0x01;
		cH = 0x1F;
		break;
		case UNDER:
		cW = 0x07;
		cH = 0x01;
		break;
		case BLOCK:
		cW = 0x07;
		cH = 0x1F;
		break;
		case NOCURSOR:
		default:
		break;
	}
//set cursor size
	RA8875_writeRegister(RA8875_CURHS, cW);
	RA8875_writeRegister(RA8875_CURVS, cH);
}

void RA8875::setTextGrandient(uint16_t fcolor1, uint16_t fcolor2) {
	_FNTgrandient = true;
	_FNTgrandientColor1 = fcolor1;
	_FNTgrandientColor2 = fcolor2;
}

/**************************************************************************/
/*!
 Normally at every char the cursor advance by one
 You can stop/enable this by using this function
 Parameters:
 on: true(auto advance - default), false:(stop auto advance)
 Note: Inactive with rendered fonts
 */
/**************************************************************************/
void RA8875::cursorIncrement(bool on) {
	if (bitRead(_TXTparameters, 7) == 0) {
		on == true ? _MWCR0_Reg &= ~(1 << 1) : _MWCR0_Reg |= (1 << 1);
		bitWrite(_TXTparameters, 1, on);
		RA8875_writeRegister(RA8875_MWCR0, _MWCR0_Reg);
	}
}

/**************************************************************************/
/*!
 return the current width of the font in pixel
 If font it's scaled, it will multiply.
 It's a fast business since the register it's internally tracked
 It can also return the usable rows based on the actual fontWidth
 Parameters: inColums (true:returns max colums)
 TODO: modded for Rendered Fonts
 */
/**************************************************************************/
uint8_t RA8875::getFontWidth(boolean inColums) {
	uint8_t temp;
	if (bitRead(_TXTparameters, 7) == 1) {
		temp = _FNTwidth;
		if (temp < 1)
		return 0; //variable with
	} else {
		temp = (((_FNCR0_Reg >> 2) & 0x3) + 1) * _FNTwidth;
	}
	if (inColums) {
		if (_scaleX < 2)
		return (_width / temp);
		temp = temp * _scaleX;
		return (_width / temp);
	} else {
		if (_scaleX < 2)
		return temp;
		temp = temp * _scaleX;
		return temp;
	}
}

/**************************************************************************/
/*!
 return the current heigh of the font in pixel
 If font it's scaled, it will multiply.
 It's a fast business since the register it's internally tracked
 It can also return the usable rows based on the actual fontHeight
 Parameters: inRows (true:returns max rows)
 TODO: modded for Rendered Fonts
 */
/**************************************************************************/
uint8_t RA8875::getFontHeight(boolean inRows) {
	uint8_t temp;
	if (bitRead(_TXTparameters, 7) == 1) {
		temp = _FNTheight;
	} else {
		temp = (((_FNCR0_Reg >> 0) & 0x3) + 1) * _FNTheight;
	}
	if (inRows) {
		if (_scaleY < 2)
		return (_height / temp);
		temp = temp * _scaleY;
		return (_height / temp);
	} else {
		if (_scaleY < 2)
		return temp;
		temp = temp * _scaleY;
		return temp;
	}
}

/**************************************************************************/
/*!
 Choose space in pixels between chars
 Parameters:
 spc: 0...63pix (default 0=off)
 TODO: modded for Rendered Fonts
 */
/**************************************************************************/
void RA8875::setFontSpacing(uint8_t spc) {
	if (spc > 0x3F)
	spc = 0x3F;
	_FNTspacing = spc;
	if (bitRead(_TXTparameters, 7) == 0) {
		_FWTSET_Reg &= 0xC0;
		_FWTSET_Reg |= spc & 0x3F;
		RA8875_writeRegister(RA8875_FWTSET, _FWTSET_Reg);
	}

}

#if defined(_RA8875_TXTRNDOPTIMIZER)

/*
 void RA8875::_drawChar_com(int16_t x,int16_t y,int16_t w,const uint8_t *data)
 {
 }
 */

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +								COLOR STUFF											 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

/**************************************************************************/
/*!
 Sets set the trasparent background color using 16bit RGB565 color
 It handles automatically color conversion when in 8 bit!
 Parameters:
 color: 16bit color RGB565
 Note: will set background Trasparency ON
 */
/**************************************************************************/
void RA8875::setTransparentColor(uint16_t color) {
	_backColor = color;
	RA8875_writeRegister(RA8875_BGTR0,
			((color & 0xF800) >> _RA8875colorMask[_colorIndex]));
	RA8875_writeRegister(RA8875_BGTR0 + 1,
			((color & 0x07E0) >> _RA8875colorMask[_colorIndex + 1]));
	RA8875_writeRegister(RA8875_BGTR0 + 2,
			((color & 0x001F) >> _RA8875colorMask[_colorIndex + 2]));
}
/**************************************************************************/
/*!
 Sets set the Trasparent background color using 8bit R,G,B
 Parameters:
 R: 8bit RED
 G: 8bit GREEN
 B: 8bit BLUE
 Note: will set background Trasparency ON
 */
/**************************************************************************/
void RA8875::setTransparentColor(uint8_t R, uint8_t G, uint8_t B) {
	_backColor = Color565(R, G, B); //keep track
	RA8875_writeRegister(RA8875_BGTR0, R);
	RA8875_writeRegister(RA8875_BGTR0 + 1, G);
	RA8875_writeRegister(RA8875_BGTR0 + 2, B);
}

/**************************************************************************/
/*!
 set foreground,background color (plus transparent background)
 Parameters:
 fColor: 16bit foreground color (text) RGB565
 bColor: 16bit background color RGB565
 backTransp:if true the bColor will be transparent
 */
/**************************************************************************/
void RA8875::setColor(uint16_t fcolor, uint16_t bcolor,
		bool bcolorTraspFlag) //0.69b30
{
	if (fcolor != _foreColor)
	setForegroundColor(fcolor);
	if (bcolorTraspFlag) {
		setTransparentColor(bcolor);
	} else {
		if (bcolor != _backColor)
		setBackgroundColor(bcolor);
	}
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +								DRAW STUFF											 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

/**************************************************************************/
/*!		Set graphic cursor beween 8 different ones.
 Graphic cursors has to be inserted before use!
 Parameters:
 cur: 0...7
 */
/**************************************************************************/
void RA8875::setGraphicCursor(uint8_t cur) {
	if (cur > 7)
	cur = 7;
	uint8_t temp = _readRegister(
			RA8875_MWCR1);
	temp &= ~(0x70);  //clear bit 6,5,4
	temp |= cur << 4;
	temp |= cur;
	if (_useMultiLayers) {
		_currentLayer == 1 ? temp |= (1 << 0) : temp &= ~(1 << 0);
	} else {
		temp &= ~(1 << 0);
	}
	RA8875_writeData(temp);
}

/**************************************************************************/
/*!		Show the graphic cursor
 Graphic cursors has to be inserted before use!
 Parameters:
 cur: true,false
 */
/**************************************************************************/
void RA8875::showGraphicCursor(
		boolean cur) {
	uint8_t temp = _readRegister(
			RA8875_MWCR1);
	cur == true ? temp |= (1 << 7) : temp &= ~(1 << 7);
	if (_useMultiLayers) {
		_currentLayer == 1 ? temp |= (1 << 0) : temp &= ~(1 << 0);
	} else {
		temp &= ~(1 << 0);
	}
	RA8875_writeData(temp);
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +								SCROLL STUFF											 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
/**************************************************************************/
/*!
 Sets the scroll mode. This is controlled by bits 6 and 7 of
 REG[52h] Layer Transparency Register0 (LTPR0)
 Author: The Experimentalist
 */
/**************************************************************************/
void RA8875::setScrollMode(enum RA8875scrollMode mode) {
	uint8_t temp = _readRegister(
			RA8875_LTPR0);
	temp &= 0x3F; // Clear bits 6 and 7 to zero
	switch (mode) {  // bit 7,6 of LTPR0
		case SIMULTANEOUS:// 00b : Layer 1/2 scroll simultaneously.
// Do nothing
		break;
		case LAYER1ONLY:// 01b : Only Layer 1 scroll.
		temp |= 0x40;
		break;
		case LAYER2ONLY:// 10b : Only Layer 2 scroll.
		temp |= 0x80;
		break;
		case BUFFERED:// 11b: Buffer scroll (using Layer 2 as scroll buffer)
		temp |= 0xC0;
		break;
		default:
		return;//do nothing
	}
//TODO: Should this be conditional on multi layer?
//if (_useMultiLayers) RA8875_writeRegister(RA8875_LTPR0,temp);
//RA8875_writeRegister(RA8875_LTPR0,temp);
	RA8875_writeData(temp);
}

/**************************************************************************/
/*!
 Define a window for perform scroll
 Parameters:
 XL: x window start left
 XR: x window end right
 YT: y window start top
 YB: y window end bottom

 */
/**************************************************************************/
void RA8875::setScrollWindow(int16_t XL, int16_t XR, int16_t YT, int16_t YB) {
	if (_portrait) {   //0.69b22 (fixed)
		swapvals(XL, YT);
		swapvals(XR, YB);
	}

	RA8875_checkLimits_helper(RA8875_struct *ra8875,&XL,& YT);
	RA8875_checkLimits_helper(RA8875_struct *ra8875,&XR, &YB);

	_scrollXL = XL;
	_scrollXR = XR;
	_scrollYT = YT;
	_scrollYB = YB;
	RA8875_writeRegister(RA8875_HSSW0, (_scrollXL & 0xFF));
	RA8875_writeRegister(RA8875_HSSW0 + 1, (_scrollXL >> 8));

	RA8875_writeRegister(RA8875_HESW0, (_scrollXR & 0xFF));
	RA8875_writeRegister(RA8875_HESW0 + 1, (_scrollXR >> 8));

	RA8875_writeRegister(RA8875_VSSW0, (_scrollYT & 0xFF));
	RA8875_writeRegister(RA8875_VSSW0 + 1, (_scrollYT >> 8));

	RA8875_writeRegister(RA8875_VESW0, (_scrollYB & 0xFF));
	RA8875_writeRegister(RA8875_VESW0 + 1, (_scrollYB >> 8));
	// vTaskDelay(1);
}

/**************************************************************************/
/*!
 Perform the scroll

 */
/**************************************************************************/
void RA8875::scroll(int16_t x, int16_t y) {
	if (_portrait)
	swapvals(x, y);
//if (y > _scrollYB) y = _scrollYB;//??? mmmm... not sure
	if (_scrollXL == 0 && _scrollXR == 0 && _scrollYT == 0 && _scrollYB == 0) {
//do nothing, scroll window inactive
	} else {
		RA8875_writeRegister(RA8875_HOFS0, (x & 0xFF));
		RA8875_writeRegister(RA8875_HOFS1, (x >> 8));

		RA8875_writeRegister(RA8875_VOFS0, (y & 0xFF));
		RA8875_writeRegister(RA8875_VOFS1, (y >> 8));
	}
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +								DMA STUFF											 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
/**************************************************************************/
/*!

 */
/**************************************************************************/
void RA8875::DMA_blockModeSize(int16_t BWR, int16_t BHR, int16_t SPWR) {
	RA8875_writeRegister(RA8875_DTNR0, BWR & 0xFF);
	RA8875_writeRegister(RA8875_BWR1, BWR >> 8);

	RA8875_writeRegister(RA8875_DTNR1, BHR & 0xFF);
	RA8875_writeRegister(RA8875_BHR1, BHR >> 8);

	RA8875_writeRegister(RA8875_DTNR2, SPWR & 0xFF);
	RA8875_writeRegister(RA8875_SPWR1, SPWR >> 8);
}

/**************************************************************************/
/*!

 */
/**************************************************************************/
void RA8875::DMA_startAddress(unsigned long adrs) {
	RA8875_writeRegister(RA8875_SSAR0, adrs & 0xFF);
	RA8875_writeRegister(RA8875_SSAR0 + 1, adrs >> 8);
	RA8875_writeRegister(RA8875_SSAR0 + 2, adrs >> 16);
//RA8875_writeRegister(0xB3,adrs >> 24);// not more in datasheet!
}

/**************************************************************************/
/*!

 */
/**************************************************************************/
void RA8875::DMA_enable(void) {
	uint8_t temp = _readRegister(
			RA8875_DMACR);
	temp |= 0x01;
	RA8875_writeData(temp);
	_waitBusy(0x01);
}
/**************************************************************************/
/*! (STILL IN DEVELOP, please do not complain)
 Display an image stored in Flash RAM
 Note: you should have the optional FLASH Chip connected to RA8875!
 Note: You should store some image in that chip!
 Note: Never tried!!!!!!!

 */
/**************************************************************************/
void RA8875::drawFlashImage(int16_t x, int16_t y, int16_t w, int16_t h,
		uint8_t picnum) {
	if (_portrait) {
		swapvals(x, y);
		swapvals(w, h);
	} //0.69b21 -have to check this, not verified
	if (_textMode)
	RA8875_setTextMode(ra8875, false);;//we are in text mode?
	RA8875_writeRegister(RA8875_SFCLR, 0x00);
	RA8875_writeRegister(RA8875_SROC, 0x87);
	RA8875_writeRegister(RA8875_DMACR, 0x02);
//setActiveWindow(0,_width-1,0,_height-1);
	RA8875_checkLimits_helper(RA8875_struct *ra8875,&x, &y);
	RA8875_checkLimits_helper(RA8875_struct *ra8875,&w, &h);
	_portrait == true ? setXY(y, x) : setXY(x, y);

	DMA_startAddress(261120 * (picnum - 1));
	DMA_blockModeSize(w, h, w);
	RA8875_writeRegister(RA8875_DMACR, 0x03);
	_waitBusy(0x01);
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +								BTE STUFF											 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

/**************************************************************************/
/*
 Block Transfer Move
 Can move a rectangular block from any area of memory (eg. layer 1) to any other (eg layer 2)
 Can move with transparency - note THE TRANSPARENT COLOUR IS THE TEXT FOREGROUND COLOUR
 ReverseDir is for moving overlapping areas - may need to use reverse to prevent it double-copying the overlapping area (this option not available with transparency or monochrome)
 ROP is Raster Operation. Usually use RA8875_ROP_SOURCE but a few others are defined
 Defaults to current layer if not given or layer is zero.
 Monochrome uses the colour-expansion mode: the input is a bit map which is then converted to the current foreground and background colours, transparent background is optional
 Monochrome data is assumed to be linear, originally written to the screen memory in 16-bit chunks with RA8875_drawPixels().
 Monochrome mode uses the ROP to define the offset of the first image bit within the first byte. This also depends on the width of the block you are trying to display.
 Monochrome skips 16-bit words in the input pattern - see the example for more explanation and a trick to interleave 2 characters in the space of one.

 This function returns immediately but the actual transfer can take some time
 Caller should check the busy status before issuing any more RS8875 commands.

 Basic usage:
 BTE_Move(SourceX, SourceY, Width, Height, DestX, DestY) = copy something visible on the current layer
 BTE_Move(SourceX, SourceY, Width, Height, DestX, DestY, 2) = copy something from layer 2 to the current layer
 BTE_Move(SourceX, SourceY, Width, Height, DestX, DestY, 2, 1, true) = copy from layer 2 to layer 1, with the transparency option
 BTE_Move(SourceX, SourceY, Width, Height, DestX, DestY, 0, 0, true, RA8875_BTEROP_ADD) = copy on the current layer, using transparency and the ADD/brighter operation
 BTE_Move(SourceX, SourceY, Width, Height, DestX, DestY, 0, 0, false, RA8875_BTEROP_SOURCE, false, true) = copy on the current layer using the reverse direction option for overlapping areas
 */

void RA8875::BTE_move(int16_t SourceX, int16_t SourceY, int16_t Width,
		int16_t Height, int16_t DestX, int16_t DestY, uint8_t SourceLayer,
		uint8_t DestLayer,
		bool Transparent, uint8_t ROP,
		bool Monochrome,
		bool ReverseDir) {

	if (SourceLayer == 0)
	SourceLayer = _currentLayer;
	if (DestLayer == 0)
	DestLayer = _currentLayer;
	if (SourceLayer == 2)
	SourceY |= 0x8000; //set the high bit of the vertical coordinate to indicate layer 2
	if (DestLayer == 2)
	DestY |= 0x8000;//set the high bit of the vertical coordinate to indicate layer 2
	ROP &= 0xF0;//Ensure the lower bits of ROP are zero
	if (Transparent) {
		if (Monochrome) {
			ROP |= 0x0A; //colour-expand transparent
		} else {
			ROP |= 0x05; //set the transparency option
		}
	} else {
		if (Monochrome) {
			ROP |= 0x0B; //colour-expand normal
		} else {
			if (ReverseDir) {
				ROP |= 0x03; //set the reverse option
			} else {
				ROP |= 0x02; //standard block-move operation
			}
		}
	}

	_waitBusy(0x40); //Check that another BTE operation is not still in progress
	if (_textMode)
	RA8875_setTextMode(ra8875, false);;//we are in text mode?
	BTE_moveFrom(SourceX, SourceY);
	BTE_size(Width, Height);
	BTE_moveTo(ra8875, DestX, DestY);
	BTE_ropcode(ROP);
//Execute BTE! (This selects linear addressing mode for the monochrome source data)
	if (Monochrome)
	RA8875_writeRegister(RA8875_BECR0, 0xC0);
	else
	RA8875_writeRegister(RA8875_BECR0, 0x80);
	_waitBusy(0x40);
//we are supposed to wait for the thing to become unbusy
//caller can call _waitBusy(0x40) to check the BTE busy status (except it's private)
}

/**************************************************************************/
/*! TESTING

 */
/**************************************************************************/
void RA8875::BTE_size(int16_t w, int16_t h) {
//0.69b21 -have to check this, not verified
	if (_portrait)
	swapvals(w, h);
	RA8875_writeRegister(RA8875_BEWR0, w & 0xFF);//BET area width literacy
	RA8875_writeRegister(RA8875_BEWR0 + 1, w >> 8);//BET area width literacy
	RA8875_writeRegister(RA8875_BEHR0, h & 0xFF);//BET area height literacy
	RA8875_writeRegister(RA8875_BEHR0 + 1, h >> 8);//BET area height literacy
}

/**************************************************************************/
/*!

 */
/**************************************************************************/

void RA8875::BTE_moveFrom(int16_t SX, int16_t SY) {
	if (_portrait)
	swapvals(SX, SY);
	RA8875_writeRegister(RA8875_HSBE0, SX & 0xFF);
	RA8875_writeRegister(RA8875_HSBE0 + 1, SX >> 8);
	RA8875_writeRegister(RA8875_VSBE0, SY & 0xFF);
	RA8875_writeRegister(RA8875_VSBE0 + 1, SY >> 8);
}

/**************************************************************************/
/*!

 */
/**************************************************************************/

void RA8875::BTE_moveTo(ra8875, int16_t DX, int16_t DY) {
	if (_portrait)
	swapvals(DX, DY);
	RA8875_writeRegister(RA8875_HDBE0, DX & 0xFF);
	RA8875_writeRegister(RA8875_HDBE0 + 1, DX >> 8);
	RA8875_writeRegister(RA8875_VDBE0, DY & 0xFF);
	RA8875_writeRegister(RA8875_VDBE0 + 1, DY >> 8);
}

/**************************************************************************/
/*! TESTING
 Use a ROP code EFX
 */
/**************************************************************************/
void RA8875::BTE_ropcode(unsigned char setx) {
	RA8875_writeRegister(RA8875_BECR1, setx);	//BECR1
}

/**************************************************************************/
/*! TESTING
 Enable BTE transfer
 */
/**************************************************************************/
void RA8875::BTE_enable(bool on) {
	uint8_t temp = _readRegister(
			RA8875_BECR0);
	on == true ? temp &= ~(1 << 7) : temp |= (1 << 7);
	RA8875_writeData(temp);
//RA8875_writeRegister(RA8875_BECR0,temp);
	_waitBusy(0x40);
}

/**************************************************************************/
/*! TESTING
 Select BTE mode (CONT (continuous) or RECT)
 */
/**************************************************************************/
void RA8875::BTE_dataMode(enum RA8875btedatam m) {
	uint8_t temp = _readRegister(
			RA8875_BECR0);
	m == CONT ? temp &= ~(1 << 6) : temp |= (1 << 6);
	RA8875_writeData(temp);
//RA8875_writeRegister(RA8875_BECR0,temp);
}

/**************************************************************************/
/*! TESTING
 Select the BTE SOURCE or DEST layer (1 or 2)
 */
/**************************************************************************/

void RA8875::BTE_layer(enum RA8875btelayer sd, uint8_t l) {
	uint8_t temp;
	sd == SOURCE ? temp = _readRegister(
			RA8875_VSBE0 + 1) :
	temp = _readRegister(
			RA8875_VDBE0 + 1);
	l == 1 ? temp &= ~(1 << 7) : temp |= (1 << 7);
	RA8875_writeData(temp);
//RA8875_writeRegister(RA8875_VSBE1,temp);
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +								LAYER STUFF											 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

/**************************************************************************/
/*!


 */
/**************************************************************************/
void RA8875::layerEffect(enum RA8875boolean efx) {
	uint8_t reg = 0b00000000;
//reg &= ~(0x07);//clear bit 2,1,0
	if (!_useMultiLayers)
	useLayers(true);//turn on multiple layers if it's off
	switch (efx) { //                       bit 2,1,0 of LTPR0
		case LAYER1://only layer 1 visible  [000]
//do nothing
		break;
		case LAYER2://only layer 2 visible  [001]
		reg |= (1 << 0);
		break;
		case TRANSPARENT://transparent mode [011]
		reg |= (1 << 0);
		reg |= (1 << 1);
		break;
		case LIGHTEN://lighten-overlay mode [010]
		reg |= (1 << 1);
		break;
		case OR://boolean OR mode           [100]
		reg |= (1 << 2);
		break;
		case AND://boolean AND mode         [101]
		reg |= (1 << 0);
		reg |= (1 << 2);
		break;
		case FLOATING://floating windows    [110]
		reg |= (1 << 1);
		reg |= (1 << 2);
		break;
		default:
//do nothing
		break;
	}
	RA8875_writeRegister(RA8875_LTPR0, reg);
}

/**************************************************************************/
/*!


 */
/**************************************************************************/
void RA8875::layerTransparency(uint8_t layer1, uint8_t layer2) {
	if (layer1 > 8)
	layer1 = 8;
	if (layer2 > 8)
	layer2 = 8;
	if (!_useMultiLayers)
	useLayers(true); //turn on multiple layers if it's off
//if (_useMultiLayers) RA8875_writeRegister(RA8875_LTPR1, ((layer2 & 0x0F) << 4) | (layer1 & 0x0F));
//uint8_t res = 0b00000000;//RA8875_LTPR1
//reg &= ~(0x07);//clear bit 2,1,0
	RA8875_writeRegister(RA8875_LTPR1, ((layer2 & 0xF) << 4) | (layer1 & 0xF));
}

/**************************************************************************/
/*! return the current drawing layer. If layers are OFF, return 255

 */
/**************************************************************************/
uint8_t RA8875::getCurrentLayer(void) {
	if (!_useMultiLayers)
	return 255;
	return _currentLayer;
}

/**************************************************************************/
/*! select pattern

 */
/**************************************************************************/
void RA8875::setPattern(uint8_t num, enum RA8875pattern p) {
	uint8_t maxLoc;
	uint8_t temp = 0b00000000;
	if (p != P16X16) {
		maxLoc = 16;	//at 8x8 max 16 locations
	} else {
		maxLoc = 4;	//at 16x16 max 4 locations
		temp |= (1 << 7);
	}
	if (num > (maxLoc - 1))
	num = maxLoc - 1;
	temp = temp | num;
	writeTo(PATTERN);
	RA8875_writeRegister(RA8875_PTNO, temp);
}

/**************************************************************************/
/*! write pattern

 */
/**************************************************************************/
void RA8875::writePattern(int16_t x, int16_t y, const uint8_t *data,
		uint8_t size, bool setAW) {
	int16_t i;
	int16_t a, b, c, d;
	if (size < 8 || size > 16)
	return;
	if (setAW)
	getActiveWindow(a, b, c, d);
	setActiveWindow(x, x + size - 1, y, y + size - 1);
	setXY(x, y);

	if (_textMode)
	RA8875_setTextMode(ra8875, false);;	//we are in text mode?
	RA8875_writeCommand(RA8875_MRWC);
	for (i = 0; i < (size * size); i++) {
		RA8875_writeData(data[i * 2]);
		RA8875_writeData(data[i * 2 + 1]);
		_waitBusy(0x80);
	}
	if (setAW)
	setActiveWindow(a, b, c, d);	//set as it was before
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +						GEOMETRIC PRIMITIVE  STUFF									 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
/**************************************************************************/
/*!

 */
/**************************************************************************/
/* void RA8875::RA8875_fillRect(void) {
 RA8875_writeCommand(RA8875_DCR);
 RA8875_writeData(RA8875_DCR_LINESQUTRI_STOP | RA8875_DCR_DRAWSQUARE);
 RA8875_writeData(RA8875_DCR_LINESQUTRI_START | RA8875_DCR_FILL | RA8875_DCR_DRAWSQUARE);
 }
 */

/**************************************************************************/
/*!
 Draw a series of pixels
 Parameters:
 p: an array of 16bit colors (pixels)
 count: how many pixels
 x: horizontal pos
 y: vertical pos
 NOTE:
 In 8bit bpp RA8875 needs a 8bit color(332) and NOT a 16bit(565),
 the routine deal with this...
 */
/**************************************************************************/
void RA8875::RA8875_drawPixels(uint16_t p[], uint16_t count, int16_t x, int16_t y) {
//setXY(x,y);
	uint16_t temp = 0;
	uint16_t i;
	if (_textMode)
	RA8875_setTextMode(ra8875, false);;//we are in text mode?
	setXY(x, y);
	RA8875_writeCommand(RA8875_MRWC);
	_startSend();
//set data
#if defined(__AVR__) && defined(_FASTSSPORT)
	_spiwrite(RA8875_DATAWRITE);
#else
#if defined(SPI_HAS_TRANSACTION) && defined(__MKL26Z64__)
	if (_altSPI) {
		SPI1.transfer(RA8875_DATAWRITE);
	} else {
		SPI_transfer(RA8875_DATAWRITE);
	}
#else
	SPI_transfer(RA8875_DATAWRITE);
#endif
#endif
//the loop
	for (i = 0; i < count; i++) {
		if (_color_bpp < 16) {
			temp = _color16To8bpp(p[i]); //TOTEST:layer bug workaround for 8bit color!
		} else {
			temp = p[i];
		}
#if !defined(ENERGIA) && !defined(___DUESTUFF) && ((ARDUINO >= 160) || (TEENSYDUINO > 121))
#if defined(SPI_HAS_TRANSACTION) && defined(__MKL26Z64__)
		if (_color_bpp > 8) {
			if (_altSPI) {
				SPI1.transfer16(temp);
			} else {
				SPI_transfer16(temp);
			}
		} else {				//TOTEST:layer bug workaround for 8bit color!
			if (_altSPI) {
				SPI1.transfer(temp);
			} else {
				SPI_transfer(temp & 0xFF);
			}
		}
#else
		if (_color_bpp > 8) {
			SPI_transfer16(temp);
		} else {				//TOTEST:layer bug workaround for 8bit color!
			SPI_transfer(temp & 0xFF);
		}
#endif
#else
#if defined(___DUESTUFF) && defined(SPI_DUE_MODE_EXTENDED)
		if (_color_bpp > 8) {
			SPI_transfer(_cs, temp >> 8, SPI_CONTINUE);
			SPI_transfer(_cs, temp & 0xFF, SPI_LAST);
		} else {				//TOTEST:layer bug workaround for 8bit color!
			SPI_transfer(_cs, temp & 0xFF, SPI_LAST);
		}
#else
#if defined(__AVR__) && defined(_FASTSSPORT)
		if (_color_bpp > 8) {
			_spiwrite16(temp);
		} else {				//TOTEST:layer bug workaround for 8bit color!
			_spiwrite(temp >> 8);
		}
#else
		if (_color_bpp > 8) {
			SPI_transfer(temp >> 8);
			SPI_transfer(temp & 0xFF);
		} else {				//TOTEST:layer bug workaround for 8bit color!
			SPI_transfer(temp & 0xFF);
		}
#endif
#endif
#endif
	}
	_endSend();
}

/**************************************************************************/
/*!
 Get a pixel color from screen
 Parameters:
 x: horizontal pos
 y: vertical pos
 */
/**************************************************************************/
uint16_t RA8875::getPixel(int16_t x, int16_t y) {
	uint16_t color;
	setXY(x, y);
	if (_textMode)
	RA8875_setTextMode(ra8875, false);;	//we are in text mode?
	RA8875_writeCommand(RA8875_MRWC);
#if defined(_FASTCPU)
	_slowDownSPI(true);
#endif
	_startSend();
#if defined(__AVR__) && defined(_FASTSSPORT)
	_spiwrite(RA8875_DATAREAD);
	_spiwrite(0x00);
#else
#if defined(SPI_HAS_TRANSACTION) && defined(__MKL26Z64__)
	if (_altSPI) {
		SPI1.transfer(RA8875_DATAREAD);
		SPI1.transfer(0x00);	//first byte it's dummy
	} else {
		SPI_transfer(RA8875_DATAREAD);
		SPI_transfer(0x00);	//first byte it's dummy
	}
#else
	SPI_transfer(RA8875_DATAREAD);
	SPI_transfer(0x00);	//first byte it's dummy
#endif
#endif
#if !defined(___DUESTUFF) && ((ARDUINO >= 160) || (TEENSYDUINO > 121))
#if defined(SPI_HAS_TRANSACTION) && defined(__MKL26Z64__)
	if (_altSPI) {
		color = SPI1.transfer16(0x0);
	} else {
		color = SPI_transfer16(0x0);
	}
#else
	color = SPI_transfer16(0x0);
#endif
#else
#if defined(___DUESTUFF) && defined(SPI_DUE_MODE_EXTENDED)
	color = SPI_transfer(_cs, 0x0, SPI_CONTINUE);
	color |= (SPI_transfer(_cs, 0x0, SPI_LAST) << 8);
#else
#if defined(__AVR__) && defined(_FASTSSPORT)
	color = _spiread();
	color |= (_spiread() << 8);
#else
	color = SPI_transfer(0x0);
	color |= (SPI_transfer(0x0) << 8);
#endif
#endif
#endif
#if defined(_FASTCPU)
	_slowDownSPI(false);
#endif
	_endSend();
	return color;
}

/*
 void RA8875::getPixels(uint16_t * p, uint32_t count, int16_t x, int16_t y)
 {
 uint16_t color;
 if (_textMode) RA8875_setTextMode(ra8875, false);;//we are in text mode?
 setXY(x,y);
 RA8875_writeCommand(RA8875_MRWC);
 #if defined(_FASTCPU)
 _slowDownSPI(true);
 #endif
 _startSend();
 SPI_transfer(RA8875_DATAREAD);
 #if !defined(ENERGIA) && !defined(__SAM3X8E__) && ((ARDUINO >= 160) || (TEENSYDUINO > 121))
 SPI_transfer16(0x0);//dummy
 #else
 SPI_transfer(0x0);//dummy
 SPI_transfer(0x0);//dummy
 #endif
 while (count--) {
 #if !defined(__SAM3X8E__) && ((ARDUINO >= 160) || (TEENSYDUINO > 121))
 color  = SPI_transfer16(0x0);
 #else
 color  = SPI_transfer(0x0);
 color |= (SPI_transfer(0x0) << 8);
 #endif
 *p++ = color;
 }
 #if defined(_FASTCPU)
 _slowDownSPI(false);
 #endif
 _endSend();
 }
 */
/**************************************************************************/
/*!
 Basic line draw
 Parameters:
 x0: horizontal start pos
 y0: vertical start
 x1: horizontal end pos
 y1: vertical end pos
 color: RGB565 color
 NOTE:
 Remember that this write from->to so: drawLine(0,0,2,0,RA8875_RED);
 result a 3 pixel long! (0..1..2)
 */
/**************************************************************************/
void RA8875::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
		uint16_t color) {
	if ((x0 == x1 && y0 == y1) || ((x1 - x0 == 1) && (y1 - y0 == 1))) {	//NEW
		RA8875_drawPixel(x0, y0, color);
		return;
	}
//if ((x1 - x0 == 1) && (y1 - y0 == 1)) RA8875_drawPixel(x0,y0,color);
	if (_portrait) {
		swapvals(x0, y0);
		swapvals(x1, y1);
	}
	if (_textMode)
	RA8875_setTextMode(ra8875, false);;	//we are in text mode?
#if defined(USE_RA8875_SEPARATE_TEXT_COLOR)
	_TXTrecoverColor = true;
#endif
	if (color != _foreColor)
	setForegroundColor(color);	//0.69b30 avoid 3 useless SPI calls

	RA8875_line_addressing(ra8875,x0, y0, x1, y1);

	RA8875_writeRegister(RA8875_DCR, 0x80);
	_waitPoll(RA8875_DCR,
			RA8875_DCR_LINESQUTRI_STATUS);
}

/**************************************************************************/
/*!
 Basic line by using Angle as parameter
 Parameters:
 x: horizontal start pos
 y: vertical start
 angle: the angle of the line
 length: lenght of the line
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawLineAngle(int16_t x, int16_t y, int16_t angle, uint16_t length,
		uint16_t color, int offset) {
	if (length < 2) {	//NEW
		RA8875_drawPixel(x, y, color);
	} else {
		drawLine(x, y, x + (length * _cosDeg_helper(angle + offset)),	//_angle_offset
				y + (length * _sinDeg_helper(angle + offset)), color);
	}
}

/**************************************************************************/
/*!
 Basic line by using Angle as parameter
 Parameters:
 x: horizontal start pos
 y: vertical start
 angle: the angle of the line
 start: where line start
 length: lenght of the line
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawLineAngle(int16_t x, int16_t y, int16_t angle, uint16_t start,
		uint16_t length, uint16_t color, int offset) {
	if (start - length < 2) {	//NEW
		RA8875_drawPixel(x, y, color);
	} else {
		drawLine(x + start * _cosDeg_helper(angle + offset),	//_angle_offset
				y + start * _sinDeg_helper(angle + offset),
				x + (start + length) * _cosDeg_helper(angle + offset),
				y + (start + length) * _sinDeg_helper(angle + offset), color);
	}
}
#ifdef NO_SIN_COS
void RA8875::roundGaugeTicker(uint16_t x, uint16_t y, uint16_t r, int from,
		int to, float dev, uint16_t color) {
	float dsec;
	int i;
	for (i = from; i <= to; i += 30) {
		dsec = i * (PI / 180);
		drawLine(x + (cos(dsec) * (r / dev)) + 1,
				y + (sin(dsec) * (r / dev)) + 1, x + (cos(dsec) * r) + 1,
				y + (sin(dsec) * r) + 1, color);
	}
}
#endif
/**************************************************************************/
/*!
 for compatibility with popular Adafruit_GFX
 draws a single vertical line
 Parameters:
 x: horizontal start
 y: vertical start
 h: height
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
	if (h < 1)
	h = 1;
	h < 2 ? RA8875_drawPixel(x, y, color) : drawLine(x, y, x, (y + h) - 1, color);
}

/**************************************************************************/
/*!
 for compatibility with popular Adafruit_GFX
 draws a single orizontal line
 Parameters:
 x: horizontal start
 y: vertical start
 w: width
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
	if (w < 1)
	w = 1;
	w < 2 ? RA8875_drawPixel(x, y, color) : drawLine(x, y, (w + x) - 1, y, color);
}

/**************************************************************************/
/*!
 calculate a grandient color
 return a spectrum starting at blue to red (0...127)
 */
/**************************************************************************/
uint16_t RA8875::grandient(uint8_t val) {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t q = val / 32;
	switch (q) {
		case 0:
		r = 0;
		g = 2 * (val % 32);
		b = 31;
		break;
		case 1:
		r = 0;
		g = 63;
		b = 31 - (val % 32);
		break;
		case 2:
		r = val % 32;
		g = 63;
		b = 0;
		break;
		case 3:
		r = 31;
		g = 63 - 2 * (val % 32);
		b = 0;
		break;
	}
	return (r << 11) + (g << 5) + b;
}

/**************************************************************************/
/*!
 draws a dots filled area
 Parameters:
 x: horizontal origin
 y: vertical origin
 w: width
 h: height
 spacing: space between dots in pixels (min 2pix)
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawMesh(int16_t x, int16_t y, int16_t w, int16_t h,
		uint8_t spacing, uint16_t color) {
	if (spacing < 2)
	spacing = 2;
	if (((x + w) - 1) >= _width)
	w = _width - x;
	if (((y + h) - 1) >= _height)
	h = _height - y;

	int16_t n, m;

	if (w < x) {
		n = w;
		w = x;
		x = n;
	}
	if (h < y) {
		n = h;
		h = y;
		y = n;
	}
	for (m = y; m <= h; m += spacing) {
		for (n = x; n <= w; n += spacing) {
			RA8875_drawPixel(n, m, color);
		}
	}
}

/**************************************************************************/
/*!
 clearScreen it's different from fillWindow because it doesn't depends
 from the active window settings so it will clear all the screen.
 It should be used only when needed since it's slower than fillWindow.
 parameter:
 color: 16bit color (default=BLACK)
 */
/**************************************************************************/
void RA8875::clearScreen(uint16_t color)	//0.69b24
{
	setActiveWindow();
	fillWindow(color);
}

/**************************************************************************/
/*!
 Draw circle
 Parameters:
 x0: The 0-based x location of the center of the circle
 y0: The 0-based y location of the center of the circle
 r: radius
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
	_center_helper(x0, y0);
	if (r < 1)
	return;
	if (r < 2) {
		RA8875_drawPixel(x0, y0, color);
		return;
	}
	_circle_helper(x0, y0, r, color, false);
}

/**************************************************************************/
/*!
 Draw a quadrilater by connecting 4 points
 Parameters:
 x0:
 y0:
 x1:
 y1:
 x2:
 y2:
 x3:
 y3:
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawQuad(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
		int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint16_t color) {
	drawLine(x0, y0, x1, y1, color);	//low 1
	drawLine(x1, y1, x2, y2, color);//high 1
	drawLine(x2, y2, x3, y3, color);//high 2
	drawLine(x3, y3, x0, y0, color);//low 2
}

/**************************************************************************/
/*!
 Draw a filled quadrilater by connecting 4 points
 Parameters:
 x0:
 y0:
 x1:
 y1:
 x2:
 y2:
 x3:
 y3:
 color: RGB565 color
 triangled: if true a full quad will be generated, false generate a low res quad (faster)
 *NOTE: a bug in _triangle_helper create some problem, still fixing....
 */
/**************************************************************************/
void RA8875::fillQuad(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
		int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint16_t color,
		bool triangled) {
	_triangle_helper(x0, y0, x1, y1, x2, y2, color, true);
	if (triangled)
	_triangle_helper(x2, y2, x3, y3, x0, y0, color, true);
	_triangle_helper(x1, y1, x2, y2, x3, y3, color, true);
}

/**************************************************************************/
/*!
 Draw a polygon from a center
 Parameters:
 cx: x center of the polygon
 cy: y center of the polygon
 sides: how many sides (min 3)
 diameter: diameter of the polygon
 rot: angle rotation of the polygon
 color: RGB565 color
 */
#ifdef NO_SIN_COS
/**************************************************************************/
void RA8875::drawPolygon(int16_t cx, int16_t cy, uint8_t sides,
		int16_t diameter, float rot, uint16_t color) {
	_center_helper(cx, cy);
	sides = (sides > 2 ? sides : 3);
	float dtr = (PI / 180.0) + PI;
	float rads = 360.0 / sides;	//points spacd equally
	uint8_t i;
	for (i = 0; i < sides; i++) {
		drawLine(cx + (sin((i * rads + rot) * dtr) * diameter),
				cy + (cos((i * rads + rot) * dtr) * diameter),
				cx + (sin(((i + 1) * rads + rot) * dtr) * diameter),
				cy + (cos(((i + 1) * rads + rot) * dtr) * diameter), color);
	}
}
#endif

/**************************************************************************/
/*!
 ringMeter
 (adapted from Alan Senior (thanks man!))
 it create a ring meter with a lot of personalizations,
 it return the width of the gauge so you can use this value
 for positioning other gauges near the one just created easily
 Parameters:
 val:  your value
 minV: the minimum value possible
 maxV: the max value possible
 x:    the position on x axis
 y:    the position on y axis
 r:    the radius of the gauge (minimum 50)
 units: a text that shows the units, if "none" all text will be avoided
 scheme:0...7 or 16 bit color (not BLACK or WHITE)
 0:red
 1:green
 2:blue
 3:blue->red
 4:green->red
 5:red->green
 6:red->green->blue
 7:cyan->green->red
 8:black->white linear interpolation
 9:violet->yellow linear interpolation
 or
 RGB565 color (not BLACK or WHITE)
 backSegColor: the color of the segments not active (default BLACK)
 angle:		90 -> 180 (the shape of the meter, 90:halfway, 180:full round, 150:default)
 inc: 			5...20 (5:solid, 20:sparse divisions, default:10)
 */
/**************************************************************************/
#ifdef NO_SIN_COS

void RA8875::ringMeter(int val, int minV, int maxV, int16_t x, int16_t y,
		uint16_t r, const char* units, uint16_t colorScheme,
		uint16_t backSegColor, int16_t angle, uint8_t inc) {
	if (inc < 5)
	inc = 5;
	if (inc > 20)
	inc = 20;
	if (r < 50)
	r = 50;
	if (angle < 90)
	angle = 90;
	if (angle > 180)
	angle = 180;
	int curAngle = map(val, minV, maxV, -angle, angle);
	uint16_t colour;
	x += r;
	y += r; // Calculate coords of centre of ring
	uint16_t w = r / 4;// Width of outer ring is 1/4 of radius
	const uint8_t seg = 5;// Segments are 5 degrees wide = 60 segments for 300 degrees
// Draw colour blocks every inc degrees
	for (int16_t i = -angle; i < angle; i += inc) {
		colour = RA8875_BLACK;
		switch (colorScheme) {
			case 0:
			colour = RA8875_RED;
			break; // Fixed colour
			case 1:
			colour = RA8875_GREEN;
			break;// Fixed colour
			case 2:
			colour = RA8875_BLUE;
			break;// Fixed colour
			case 3:
			colour = grandient(map(i, -angle, angle, 0, 127));
			break;// Full spectrum blue to red
			case 4:
			colour = grandient(map(i, -angle, angle, 63, 127));
			break;// Green to red (high temperature etc)
			case 5:
			colour = grandient(map(i, -angle, angle, 127, 63));
			break;// Red to green (low battery etc)
			case 6:
			colour = grandient(map(i, -angle, angle, 127, 0));
			break;// Red to blue (air cond reverse)
			case 7:
			colour = grandient(map(i, -angle, angle, 35, 127));
			break;// cyan to red
			case 8:
			colour = colorInterpolation(0, 0, 0, 255, 255, 255,
					map(i, -angle, angle, 0, w), w);
			break;// black to white
			case 9:
			colour = colorInterpolation(0x80, 0, 0xC0, 0xFF, 0xFF, 0,
					map(i, -angle, angle, 0, w), w);
			break;// violet to yellow
			default:
			if (colorScheme > 9) {
				colour = colorScheme;
			} else {
				colour = RA8875_BLUE;
			}
			break; // Fixed colour
		}
// Calculate pair of coordinates for segment start
		float xStart = cos((i - 90) * 0.0174532925);
		float yStart = sin((i - 90) * 0.0174532925);
		uint16_t x0 = xStart * (r - w) + x;
		uint16_t y0 = yStart * (r - w) + y;
		uint16_t x1 = xStart * r + x;
		uint16_t y1 = yStart * r + y;

// Calculate pair of coordinates for segment end
		float xEnd = cos((i + seg - 90) * 0.0174532925);
		float yEnd = sin((i + seg - 90) * 0.0174532925);
		int16_t x2 = xEnd * (r - w) + x;
		int16_t y2 = yEnd * (r - w) + y;
		int16_t x3 = xEnd * r + x;
		int16_t y3 = yEnd * r + y;

		if (i < curAngle) { // Fill in coloured segments with 2 triangles
			fillQuad(x0, y0, x1, y1, x2, y2, x3, y3, colour, false);
		} else { // Fill in blank segments
			fillQuad(x0, y0, x1, y1, x2, y2, x3, y3, backSegColor, false);
		}
	}

// text
	if (strcmp(units, "none") != 0) {
//erase internal background
		if (angle > 90) {
			fillCircle(x, y, r - w, _backColor);
		} else {
			fillCurve(x, y + getFontHeight() / 2, r - w, r - w, 1, _backColor);
			fillCurve(x, y + getFontHeight() / 2, r - w, r - w, 2, _backColor);
		}
//prepare for write text
		if (r > 84) {
			setFontScale(1);
		} else {
			setFontScale(0);
		}
		if (_portrait) {
			setCursor(y, x - 15, true);
		} else {
			setCursor(x - 15, y, true);
		}
	}

// Calculate and return right hand side x coordinate
//return x + r;

}
#endif
/**************************************************************************/
/*!
 Draw Triangle
 Parameters:
 x0: The 0-based x location of the point 0 of the triangle bottom LEFT
 y0: The 0-based y location of the point 0 of the triangle bottom LEFT
 x1: The 0-based x location of the point 1 of the triangle middle TOP
 y1: The 0-based y location of the point 1 of the triangle middle TOP
 x2: The 0-based x location of the point 2 of the triangle bottom RIGHT
 y2: The 0-based y location of the point 2 of the triangle bottom RIGHT
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
		int16_t x2, int16_t y2, uint16_t color) {
	_triangle_helper(x0, y0, x1, y1, x2, y2, color, false);
}

/**************************************************************************/
/*!
 Draw filled Triangle
 Parameters:
 x0: The 0-based x location of the point 0 of the triangle
 y0: The 0-based y location of the point 0 of the triangle
 x1: The 0-based x location of the point 1 of the triangle
 y1: The 0-based y location of the point 1 of the triangle
 x2: The 0-based x location of the point 2 of the triangle
 y2: The 0-based y location of the point 2 of the triangle
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
		int16_t x2, int16_t y2, uint16_t color) {
	_triangle_helper(x0, y0, x1, y1, x2, y2, color, true);
}

/**************************************************************************/
/*!
 Draw an ellipse
 Parameters:
 xCenter:   x location of the center of the ellipse
 yCenter:   y location of the center of the ellipse
 longAxis:  Size in pixels of the long axis
 shortAxis: Size in pixels of the short axis
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawEllipse(int16_t xCenter, int16_t yCenter, int16_t longAxis,
		int16_t shortAxis, uint16_t color) {
	_ellipseCurve_helper(xCenter, yCenter, longAxis, shortAxis, 255, color,
			false);
}

/**************************************************************************/
/*!
 Draw a filled ellipse
 Parameters:
 xCenter:   x location of the center of the ellipse
 yCenter:   y location of the center of the ellipse
 longAxis:  Size in pixels of the long axis
 shortAxis: Size in pixels of the short axis
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::fillEllipse(int16_t xCenter, int16_t yCenter, int16_t longAxis,
		int16_t shortAxis, uint16_t color) {
	_ellipseCurve_helper(xCenter, yCenter, longAxis, shortAxis, 255, color,
			true);
}

/**************************************************************************/
/*!
 Draw a curve
 Parameters:
 xCenter:]   x location of the ellipse center
 yCenter:   y location of the ellipse center
 longAxis:  Size in pixels of the long axis
 shortAxis: Size in pixels of the short axis
 curvePart: Curve to draw in clock-wise dir: 0[180-270�],1[270-0�],2[0-90�],3[90-180�]
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::drawCurve(int16_t xCenter, int16_t yCenter, int16_t longAxis,
		int16_t shortAxis, uint8_t curvePart, uint16_t color) {
	curvePart = curvePart % 4; //limit to the range 0-3
	if (_portrait) { //fix a problem with rotation
		if (curvePart == 0) {
			curvePart = 2;
		} else if (curvePart == 2) {
			curvePart = 0;
		}
	}
	_ellipseCurve_helper(xCenter, yCenter, longAxis, shortAxis, curvePart,
			color, false);
}

/**************************************************************************/
/*!
 Draw a filled curve
 Parameters:
 xCenter:]   x location of the ellipse center
 yCenter:   y location of the ellipse center
 longAxis:  Size in pixels of the long axis
 shortAxis: Size in pixels of the short axis
 curvePart: Curve to draw in clock-wise dir: 0[180-270�],1[270-0�],2[0-90�],3[90-180�]
 color: RGB565 color
 */
/**************************************************************************/
void RA8875::fillCurve(int16_t xCenter, int16_t yCenter, int16_t longAxis,
		int16_t shortAxis, uint8_t curvePart, uint16_t color) {
	curvePart = curvePart % 4; //limit to the range 0-3
	if (_portrait) { //fix a problem with rotation
		if (curvePart == 0) {
			curvePart = 2;
		} else if (curvePart == 2) {
			curvePart = 0;
		}
	}
	_ellipseCurve_helper(xCenter, yCenter, longAxis, shortAxis, curvePart,
			color, true);
}

/**************************************************************************/
/*!
 helper function for triangles
 [private]
 */
/**************************************************************************/
void RA8875::_triangle_helper(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
		int16_t x2, int16_t y2, uint16_t color,
		bool filled) {
	if (x0 >= _width || x1 >= _width || x2 >= _width)
	return;
	if (y0 >= _height || y1 >= _height || y2 >= _height)
	return;

	if (_portrait) {
		swapvals(x0, y0);
		swapvals(x1, y1);
		swapvals(x2, y2);
	}

	if (x0 == x1 && y0 == y1) {
		drawLine(x0, y0, x2, y2, color);
		return;
	} else if (x0 == x2 && y0 == y2) {
		drawLine(x0, y0, x1, y1, color);
		return;
	} else if (x0 == x1 && y0 == y1 && x0 == x2 && y0 == y2) {		//new
		RA8875_drawPixel(x0, y0, color);
		return;
	}

	if (y0 > y1) {
		swapvals(y0, y1);
		swapvals(x0, x1);
	}

	if (y1 > y2) {
		swapvals(y2, y1);
		swapvals(x2, x1);
	}

	if (y0 > y1) {
		swapvals(y0, y1);
		swapvals(x0, x1);
	}

	if (y0 == y2) { // Handle awkward all-on-same-line case as its own thing
		int16_t a, b;
		a = b = x0;
		if (x1 < a) {
			a = x1;
		} else if (x1 > b) {
			b = x1;
		}
		if (x2 < a) {
			a = x2;
		} else if (x2 > b) {
			b = x2;
		}
		drawFastHLine(a, y0, b - a + 1, color);
		return;
	}

	if (_textMode)
	RA8875_setTextMode(ra8875, false);; //we are in text mode?

#if defined(USE_RA8875_SEPARATE_TEXT_COLOR)
	_TXTrecoverColor = true;
#endif
	if (color != _foreColor)
	setForegroundColor(color); //0.69b30 avoid several SPI calls

//RA8875_checkLimits_helper(RA8875_struct *ra8875,&x0,&y0);
//RA8875_checkLimits_helper(RA8875_struct *ra8875,&x1,&y1);

	RA8875_line_addressing(ra8875,x0, y0, x1, y1);
//p2

	RA8875_writeRegister(RA8875_DTPH0, x2 & 0xFF);
	RA8875_writeRegister(RA8875_DTPH0 + 1, x2 >> 8);
	RA8875_writeRegister(RA8875_DTPV0, y2 & 0xFF);
	RA8875_writeRegister(RA8875_DTPV0 + 1, y2 >> 8);

	RA8875_writeCommand(RA8875_DCR);
	filled == true ? RA8875_writeData(0xA1) : RA8875_writeData(0x81);

	_waitPoll(RA8875_DCR,
			RA8875_DCR_LINESQUTRI_STATUS);
}

/**************************************************************************/
/*!
 helper function for ellipse and curve
 [private]
 */
/**************************************************************************/
void RA8875::_ellipseCurve_helper(int16_t xCenter, int16_t yCenter,
		int16_t longAxis, int16_t shortAxis, uint8_t curvePart, uint16_t color,
		bool filled) {
	_center_helper(xCenter, yCenter);	//use CENTER?

	if (_portrait) {
		swapvals(xCenter, yCenter);
		swapvals(longAxis, shortAxis);
		if (longAxis > _height / 2)
		longAxis = (_height / 2) - 1;
		if (shortAxis > _width / 2)
		shortAxis = (_width / 2) - 1;
	} else {
		if (longAxis > _width / 2)
		longAxis = (_width / 2) - 1;
		if (shortAxis > _height / 2)
		shortAxis = (_height / 2) - 1;
	}
	if (longAxis == 1 && shortAxis == 1) {
		RA8875_drawPixel(xCenter, yCenter, color);
		return;
	}
	RA8875_checkLimits_helper(RA8875_struct *ra8875,&xCenter, &yCenter);

	if (_textMode)
	RA8875_setTextMode(ra8875, false);;	//we are in text mode?

#if defined(USE_RA8875_SEPARATE_TEXT_COLOR)
	_TXTrecoverColor = true;
#endif
	if (color != _foreColor)
	setForegroundColor(color);

	_curve_addressing(xCenter, yCenter, longAxis, shortAxis);
	RA8875_writeCommand(RA8875_ELLIPSE);

	if (curvePart != 255) {
		curvePart = curvePart % 4; //limit to the range 0-3
		filled == true ?
		RA8875_writeData(0xD0 | (curvePart & 0x03)) :
		RA8875_writeData(0x90 | (curvePart & 0x03));
	} else {
		filled == true ? RA8875_writeData(0xC0) : RA8875_writeData(0x80);
	}
	_waitPoll(RA8875_ELLIPSE,
			RA8875_ELLIPSE_STATUS);
}

/**************************************************************************/
/*!
 curve addressing helper
 [private]
 */
/**************************************************************************/
void RA8875::_curve_addressing(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
//center
	RA8875_writeRegister(RA8875_DEHR0, x0 & 0xFF);
	RA8875_writeRegister(RA8875_DEHR0 + 1, x0 >> 8);
	RA8875_writeRegister(RA8875_DEVR0, y0 & 0xFF);
	RA8875_writeRegister(RA8875_DEVR0 + 1, y0 >> 8);
//long,short ax
	RA8875_writeRegister(RA8875_ELL_A0, x1 & 0xFF);
	RA8875_writeRegister(
			RA8875_ELL_A0 + 1, x1 >> 8);
	RA8875_writeRegister(RA8875_ELL_B0, y1 & 0xFF);
	RA8875_writeRegister(
			RA8875_ELL_B0 + 1, y1 >> 8);
}
#ifdef NO_SIN_COS

#endif
/**************************************************************************/
/*!
 change the arc default parameters
 */
/**************************************************************************/
void RA8875::setArcParams(float arcAngleMax, int arcAngleOffset) {
	_arcAngle_max = arcAngleMax;
	_arcAngle_offset = arcAngleOffset;
}

/**************************************************************************/
/*!
 change the angle offset parameter from default one
 */
/**************************************************************************/
void RA8875::setAngleOffset(int16_t angleOffset) {
	_angle_offset = ANGLE_OFFSET + angleOffset;
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +								PWM STUFF											 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

/**************************************************************************/
/*!
 on/off GPIO (basic for Adafruit module
 */
/**************************************************************************/
void RA8875::GPIOX(boolean on) {
	RA8875_writeRegister(RA8875_GPIOX, on);
}

/**************************************************************************/
/*!
 Set the brightness of the backlight (if connected to pwm)
 (basic controls pwm 1)
 Parameters:
 val: 0...255
 */
/**************************************************************************/
void RA8875::brightness(uint8_t val) {
	_brightness = val;
	PWMout(1, _brightness);
}

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +								         ISR										 +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

void RA8875::setInternalInt(enum RA8875intlist b) {
	_enabledInterrups |= (1 << b); //set
}

void RA8875::clearInternalInt(enum RA8875intlist b) {
	_enabledInterrups &= ~(1 << b); //clear
}

/**************************************************************************/
/*!
 RA8875 can use the INT pin for many purposes.
 This function just tell the CPU to use a pin for listen to RA8875 pin,
 no matter if we will use ISR or DigitalRead
 ------------------------------
 Bit:	Called by:		In use:
 --------------------------------
 0: 		isr triggered	[*]
 1: 		Resistive TS	[*]
 2: 		KeyScan			[*]
 3: 		DMA
 4: 		BTE
 5: 		-na-
 6: 		-na-
 7: 		-na-
 --------------------------------
 Parameters:
 INTpin: it's the pin where we listen to ISR
 INTnum: it's the INT related to pin. On some processor it's not needed
 This last parameter it's used only when decide to use an ISR.
 */
/**************************************************************************/

void RA8875::useINT(const uint8_t INTpin, const uint8_t INTnum) {
	_intPin = INTpin;
	_intNum = INTnum;
//#if defined(___TEENSYES)//all of them (32 bit only)
//	pinMode(_intPin ,INPUT_PULLUP);
//#else
//	pinMode(_intPin, INPUT);
//#endif
}

/**************************************************************************/
/*!
 the generic ISR routine, will set to 1 bit 0 of _RA8875_INTS
 [private]
 */
/**************************************************************************/
void RA8875::_isr(void) {
	_RA8875_INTS |= (1 << 0); //set
}

/**************************************************************************/
/*!
 Enable the ISR, after this any falling edge on
 _intPin pin will trigger ISR
 Parameters:
 force: if true will force attach interrupt
 NOTE:
 if parameter _needISRrearm = true will rearm interrupt
 */
/**************************************************************************/
void RA8875::enableISR(bool force) {
	if (force || _needISRrearm) {
		_needISRrearm = false;
//#ifdef digitalPinToInterrupt
//		attachInterrupt(digitalPinToInterrupt(_intPin),_isr,FALLING);
//#else
//		attachInterrupt(_intNum, _isr, FALLING);
//#endif
		_RA8875_INTS = 0b00000000;//reset all INT bits flags
		_useISR = true;
	}
#if defined(USE_RA8875_TOUCH)
	if (_touchEnabled) _checkInterrupt(2); //clear internal RA int to engage
#endif
}

/**************************************************************************/
/*!
 Disable ISR
 Works only if previously enabled or do nothing.
 */
/**************************************************************************/
void RA8875::_disableISR(void) {
	if (_useISR) {
#if defined(USE_RA8875_TOUCH)
		if (_touchEnabled) _checkInterrupt(2); //clear internal RA int to engage
#endif
//#ifdef digitalPinToInterrupt
//		detachInterrupt(digitalPinToInterrupt(_intPin));
//#else
//		detachInterrupt(_intNum);
//#endif
		_RA8875_INTS = 0b00000000;//reset all bits
		_useISR = false;
	}
}

/**************************************************************************/
/*!
 Check the [interrupt register] for an interrupt,
 if found it will reset it.
 bit
 0: complicated....
 1: BTE INT
 2: TOUCH INT
 3: DMA INT
 4: Keyscan INT
 */
/**************************************************************************/
bool RA8875::_checkInterrupt(uint8_t _bit, bool _clear) {
	if (_bit > 4)
	_bit = 4;
	uint8_t temp = _readRegister(
			RA8875_INTC2);
	if (bitRead(temp, _bit) == 1) {
		if (_clear) {
			temp |= (1 << _bit); //bitSet(temp,_bit);
//if (bitRead(temp,0)) bitSet(temp,0);//Mmmmm...
			RA8875_writeRegister(RA8875_INTC2, temp);//clear int
		}
		return true;
	}
	return false;
}

#if defined(USE_FT5206_TOUCH)
/**************************************************************************/
/*!
 The FT5206 Capacitive Touch driver uses a different INT pin than RA8875
 and it's not controlled by RA chip of course, so we need a separate code
 for that purpose, no matter we decide to use an ISR or digitalRead.
 no matter if we will use ISR or DigitalRead
 Parameters:
 INTpin: it's the pin where we listen to ISR
 INTnum: it's the INT related to pin. On some processor it's not needed
 This last parameter it's used only when decide to use an ISR.
 */
/**************************************************************************/
void RA8875::useCapINT(const uint8_t INTpin,const uint8_t INTnum)
{
	_intCTSPin = INTpin;
	_intCTSNum = INTnum;
#if defined(___TEENSYES)//all of them (32 bit only)
	pinMode(_intCTSPin ,INPUT_PULLUP);
#else
	pinMode(_intCTSPin ,INPUT);
#endif
}

/**************************************************************************/
/*!
 Since FT5206 uses a different INT pin, we need a separate isr routine.
 [private]
 */
/**************************************************************************/
void RA8875::cts_isr(void)
{
	_FT5206_INT = true;
}

/**************************************************************************/
/*!
 Enable the ISR, after this any falling edge on
 _intCTSPin pin will trigger ISR
 Parameters:
 force: if true will force attach interrup
 NOTE:
 if parameter _needCTS_ISRrearm = true will rearm interrupt
 */
/**************************************************************************/
void RA8875::enableCapISR(bool force)
{
	if (force || _needCTS_ISRrearm) {
		_needCTS_ISRrearm = false;
#ifdef digitalPinToInterrupt
		attachInterrupt(digitalPinToInterrupt(_intCTSPin),cts_isr,FALLING);
#else
		attachInterrupt(_intCTSNum,cts_isr,FALLING);
#endif
		_FT5206_INT = false;
		_useISR = true;
	}
}

/**************************************************************************/
/*!
 Disable ISR [FT5206 version]
 Works only if previously enabled or do nothing.
 */
/**************************************************************************/
void RA8875::_disableCapISR(void)
{
	if (_useISR) {
#ifdef digitalPinToInterrupt
		detachInterrupt(digitalPinToInterrupt(_intCTSPin));
#else
		detachInterrupt(_intCTSNum);
#endif
		_FT5206_INT = false;
		_useISR = false;
	}
}
#endif

/*
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +							     TOUCH SCREEN COMMONS						         +
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
#if !defined(_AVOID_TOUCHSCREEN)//Common touch screend methods (for capacitive and resistive)

/**************************************************************************/
/*!
 Checks an interrupt has occurred. return true if yes.
 Designed to run in loop.
 It works with ISR or DigitalRead methods
 Parameters:
 safe: true  (detach interrupt routine, has to be re-engaged manually!)
 false (
 */
/**************************************************************************/
bool RA8875::touched(bool safe)
{
	if (_useISR) {				//using interrupts
#if defined(USE_FT5206_TOUCH)
		_needCTS_ISRrearm = safe;
		if (_FT5206_INT) {
#elif defined(USE_RA8875_TOUCH)
			_needISRrearm = safe;
			if (bitRead(_RA8875_INTS,0)) {
#endif
				//there was an interrupt
#if defined(USE_FT5206_TOUCH)
				if (_needCTS_ISRrearm) {		//safe was true, detach int
					_disableCapISR();
#else
					if (_needISRrearm) {		//safe was true, detach int
						_disableISR();
#endif
					} else {		//safe was false, do not detatch int and clear INT flag
#if defined(USE_FT5206_TOUCH)
						_FT5206_INT = false;
#elif defined(USE_RA8875_TOUCH)
						_RA8875_INTS &= ~(1 << 0);	//clear
						_checkInterrupt(2);//clear internal RA int to re-engage
#endif
					}
					return true;
				}
				return false;
			} else {	//not use ISR, digitalRead method
#if defined(USE_FT5206_TOUCH)
				//TODO

#elif defined(USE_RA8875_TOUCH)
				if (_touchEnabled) {
#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MKL26Z64__)
					if (!digitalReadFast(_intPin)) {
#else
						if (!digitalRead(_intPin)) {
#endif
							_clearTInt = true;
							if (_checkInterrupt(2)) {
								return true;
							} else {
								return false;
							}
						}				//digitalRead

						if (_clearTInt) {
							_clearTInt = false;
							_checkInterrupt(2);	//clear internal RA int
							// vTaskDelay(1);
						}

						return false;

					} else {	//_touchEnabled
						return false;
					}
#endif
					return false;
				}
			}

			void RA8875::setTouchLimit(uint8_t limit)
			{
#if defined(USE_FT5206_TOUCH)
				if (limit > 5) limit = 5;	//max 5 allowed
#else
				limit = 1;	//always 1
#endif
				_maxTouch = limit;
			}

			uint8_t RA8875::getTouchLimit(void)
			{
				return _maxTouch;
			}
			/*
			 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			 +				CAPACITIVE TOUCH SCREEN CONTROLLER	FT5206						     +
			 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			 */
#if defined(USE_FT5206_TOUCH)

			/**************************************************************************/
			/*!
			 This is the function that update the current state of the Touch Screen
			 It's developed for use in loop
			 */
			/**************************************************************************/
			void RA8875::updateTS(void)
			{
				Wire.requestFrom((uint8_t)_ctpAdrs,(uint8_t)28); //get 28 registers
				uint8_t index = 0;
				while(Wire.available()) {
					_cptRegisters[index++] = Wire.read(); //fill registers
				}
				_currentTouches = _cptRegisters[0x02] & 0xF;
				if (_currentTouches > _maxTouch) _currentTouches = _maxTouch;
				_gesture = _cptRegisters[0x01];
				if (_maxTouch < 2) _gesture = 0;
				uint8_t temp = _cptRegisters[0x03];
				_currentTouchState = 0;
				if (!bitRead(temp,7) && bitRead(temp,6)) _currentTouchState = 1; //finger up
				if (bitRead(temp,7) && !bitRead(temp,6)) _currentTouchState = 2;//finger down
			}

			/**************************************************************************/
			/*!
			 It gets coordinates out from data collected by updateTS function
			 Actually it will not communicate with FT5206, just reorder collected data
			 so it MUST be used after updateTS!
			 */
			/**************************************************************************/
			uint8_t RA8875::getTScoordinates(uint16_t (*touch_coordinates)[2])
			{
				uint8_t i;
				if (_currentTouches < 1) return 0;
				for (i=1;i<=_currentTouches;i++) {
					switch(_rotation) {
						case 0: //ok
								//touch_coordinates[i-1][0] = _width - (((_cptRegisters[coordRegStart[i-1]] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 1]) / (4096/_width);
								//touch_coordinates[i-1][1] = (((_cptRegisters[coordRegStart[i-1]] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 1]) / (4096/_height);
						touch_coordinates[i-1][0] = ((_cptRegisters[coordRegStart[i-1]] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 1];
						touch_coordinates[i-1][1] = ((_cptRegisters[coordRegStart[i-1] + 2] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 3];
						break;
						case 1://ok
						touch_coordinates[i-1][0] = (((_cptRegisters[coordRegStart[i-1] + 2] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 3]);
						touch_coordinates[i-1][1] = (RA8875_WIDTH - 1) - (((_cptRegisters[coordRegStart[i-1]] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 1]);
						break;
						case 2://ok
						touch_coordinates[i-1][0] = (RA8875_WIDTH - 1) - (((_cptRegisters[coordRegStart[i-1]] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 1]);
						touch_coordinates[i-1][1] = (RA8875_HEIGHT - 1) -(((_cptRegisters[coordRegStart[i-1] + 2] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 3]);
						break;
						case 3://ok
						touch_coordinates[i-1][0] = (RA8875_HEIGHT - 1) - (((_cptRegisters[coordRegStart[i-1] + 2] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 3]);
						touch_coordinates[i-1][1] = (((_cptRegisters[coordRegStart[i-1]] & 0x0f) << 8) | _cptRegisters[coordRegStart[i-1] + 1]);
						break;
					}
					if (i == _maxTouch) return i;
				}
				return _currentTouches;
			}

			/**************************************************************************/
			/*!
			 Gets the current Touch State, must be used AFTER updateTS!
			 */
			/**************************************************************************/
			uint8_t RA8875::getTouchState(void)
			{
				return _currentTouchState;
			}

			/**************************************************************************/
			/*!
			 Gets the number of touches, must be used AFTER updateTS!
			 Return 0..5
			 0: no touch
			 */
			/**************************************************************************/
			uint8_t RA8875::getTouches(void)
			{
				return _currentTouches;
			}

			/**************************************************************************/
			/*!
			 Gets the gesture, if identified, must be used AFTER updateTS!
			 */
			/**************************************************************************/
			uint8_t RA8875::getGesture(void)
			{
				return _gesture;
			}

#elif defined(USE_RA8875_TOUCH)
			/*
			 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			 +						RESISTIVE TOUCH STUFF										 +
			 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			 */

			/**************************************************************************/
			/*!
			 Initialize support for on-chip resistive Touch Screen controller
			 It also enable the Touch Screen
			 NOTE:
			 You need to use useINT(pin) [before] to define an INT pin!
			 You need to use enableISR() [after] to enable ISR on MCU and RA8875!
			 */
			/**************************************************************************/
			void RA8875::touchBegin(void)
			{
				if (_intPin < 255) {
					/*		Touch Panel Control Register 0  TPCR0  [0x70]
					 7: 0(disable, 1:(enable)
					 6,5,4:TP Sample Time Adjusting (000...111)
					 3:Touch Panel Wakeup Enable 0(disable),1(enable)
					 2,1,0:ADC Clock Setting (000...111) set fixed to 010: (System CLK) / 4, 10Mhz Max! */
#if defined(___TEENSYES) ||  defined(___DUESTUFF)//fast 32 bit processors
					RA8875_writeRegister(RA8875_TPCR0, TP_ENABLE | TP_ADC_SAMPLE_16384_CLKS | TP_ADC_CLKDIV_32);
#else
					RA8875_writeRegister(RA8875_TPCR0, TP_ENABLE | TP_ADC_SAMPLE_4096_CLKS | TP_ADC_CLKDIV_16);
#endif
					RA8875_writeRegister(RA8875_TPCR1, TP_MODE_AUTO | TP_DEBOUNCE_ON);
					setInternalInt(TOUCH);
					_INTC1_Reg |= (1 << 2);	//set
					RA8875_writeRegister(RA8875_INTC1, _INTC1_Reg);
					_touchEnabled = true;
				} else {
					_touchEnabled = false;
				}
			}

			/**************************************************************************/
			/*!
			 Enables or disables the on-chip touch screen controller
			 You must use touchBegin at list once to instruct the RA8875
			 Parameters:
			 enabled: true(enable),false(disable)
			 */
			/**************************************************************************/
			void RA8875::touchEnable(boolean enabled) {
				if (_intPin < 255) {
					/* another grrrr bug of the RA8875!
					 if we are in text mode the RA chip cannot get back the
					 INT mode!
					 */
					if (_textMode) RA8875_setTextMode(ra8875, false);;
					if (!_touchEnabled && enabled) {	//Enable
						//enableISR(true);
						// bitSet(_INTC1_Reg,2);
						// RA8875_writeRegister(RA8875_INTC1, _INTC1_Reg);
						_touchEnabled = true;
						_checkInterrupt(2);
					} else if (_touchEnabled && !enabled) {	//disable
						//_disableISR();
						// bitClear(_INTC1_Reg,2);
						// RA8875_writeRegister(RA8875_INTC1, _INTC1_Reg);
						_checkInterrupt(2);
						_touchEnabled = false;
					}
				} else {
					_touchEnabled = false;
				}
			}

			/**************************************************************************/
			/*!
			 Read 10bit internal ADC of RA8875 registers and perform corrections
			 It will return always RAW data
			 Parameters:
			 x: out 0...1023
			 Y: out 0...1023

			 */
			/**************************************************************************/
			void RA8875::readTouchADC(uint16_t *x, uint16_t *y)
			{
#if defined(_FASTCPU)
				_slowDownSPI(true);
#endif
				uint16_t tx = _readRegister(RA8875_TPXH);
				uint16_t ty = _readRegister(RA8875_TPYH);
				uint8_t remain = _readRegister(RA8875_TPXYL);
#if defined(_FASTCPU)
				_slowDownSPI(false);
#endif
				tx <<= 2;
				ty <<= 2;
				tx |= remain & 0x03; // get the bottom x bits
				ty |= (remain >> 2) & 0x03;// get the bottom y bits
				if (_portrait) {
					*x = ty;
					*y = tx;
				} else {
					tx = 1024 - tx;
					ty = 1024 - ty;
					*x = tx;
					*y = ty;
				}
			}

			/**************************************************************************/
			/*!
			 Returns 10bit x,y data with TRUE scale (0...1023)
			 Parameters:
			 x: out 0...1023
			 Y: out 0...1023
			 */
			/**************************************************************************/
			void RA8875::touchReadAdc(uint16_t *x, uint16_t *y)
			{
				uint16_t tx,ty;
				readTouchADC(&tx,&ty);
#if (defined(TOUCSRCAL_XLOW) && (TOUCSRCAL_XLOW != 0)) || (defined(TOUCSRCAL_XHIGH) && (TOUCSRCAL_XHIGH != 0))
				*x = map(tx,_tsAdcMinX,_tsAdcMaxX,0,1024);
#else
				*x = tx;
#endif
#if (defined(TOUCSRCAL_YLOW) && (TOUCSRCAL_YLOW != 0)) || (defined(TOUCSRCAL_YHIGH) && (TOUCSRCAL_YHIGH != 0))
				*y = map(ty,_tsAdcMinY,_tsAdcMaxY,0,1024);
#else
				*y = ty;
#endif
				_checkInterrupt(2);
			}

			/**************************************************************************/
			/*!
			 Returns pixel x,y data with SCREEN scale (screen width, screen Height)
			 Parameters:
			 x: out 0...screen width  (pixels)
			 Y: out 0...screen Height (pixels)
			 Check for out-of-bounds here as touches near the edge of the screen
			 can be safely mapped to the nearest point of the screen.
			 If the screen is rotated, then the min and max will be modified elsewhere
			 so that this always corresponds to screen pixel coordinates.
			 /M.SANDERSCROCK added constrain
			 */
			/**************************************************************************/
			void RA8875::touchReadPixel(uint16_t *x, uint16_t *y)
			{
				uint16_t tx,ty;
				readTouchADC(&tx,&ty);
				//*x = map(tx,_tsAdcMinX,_tsAdcMaxX,0,_width-1);
				*x = constrain(map(tx,_tsAdcMinX,_tsAdcMaxX,0,_width-1),0,_width-1);
				//*y = map(ty,_tsAdcMinY,_tsAdcMaxY,0,_height-1);
				*y = constrain(map(ty,_tsAdcMinY,_tsAdcMaxY,0,_height-1),0,_height-1);
				_checkInterrupt(2);
			}

			boolean RA8875::touchCalibrated(void)
			{
				return _calibrated;
			}

			/**************************************************************************/
			/*!   A service utility that detects if system has been calibrated in the past
			 Return true if an old calibration exists
			 [private]
			 */
			/**************************************************************************/
			boolean RA8875::_isCalibrated(void)
			{
				uint8_t uncaltetection = 4;
#if defined(TOUCSRCAL_XLOW) && (TOUCSRCAL_XLOW != 0)
				uncaltetection--;
#endif
#if defined(TOUCSRCAL_YLOW) && (TOUCSRCAL_YLOW != 0)
				uncaltetection--;
#endif
#if defined(TOUCSRCAL_XHIGH) && (TOUCSRCAL_XHIGH != 0)
				uncaltetection--;
#endif
#if defined(TOUCSRCAL_YHIGH) && (TOUCSRCAL_YHIGH != 0)
				uncaltetection--;
#endif
				if (uncaltetection < 4) return true;
				return false;
			}

#endif
#endif

			/*
			 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			 +								  SLEEP STUFF										 +
			 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			 */

			/**************************************************************************/
			/*!
			 Sleep mode on/off (complete sequence)
			 The sleep on/off sequence it's quite tricky on RA8875 when in SPI mode!
			 */
			/**************************************************************************/
			void RA8875::sleep(
					boolean sleep) {
				if (_sleep != sleep) { //only if it's needed
					_sleep = sleep;
					if (_sleep == true) {
						//1)turn off backlight
						if (_displaySize == Adafruit_480x272
								|| _displaySize
								== Adafruit_800x480/* || _displaySize == Adafruit_640x480*/)
						GPIOX(false);
						//2)decelerate SPI clock
#if defined(_FASTCPU)
						_slowDownSPI(true);
#else
#if defined(SPI_HAS_TRANSACTION)
						_SPImaxSpeed = 4000000UL;
#else
#if defined(___DUESTUFF) && defined(SPI_DUE_MODE_EXTENDED)
						SPI_setClockDivider(_cs,SPI_SPEED_READ);
#else
						SPI_setClockDivider(SPI_SPEED_READ);
#endif
#endif
#endif
						//3)set PLL to default
						RA8875__setSysClock(0x07, 0x03, 0x02);
						//4)display off & sleep
						RA8875_writeRegister(
								RA8875_PWRR,
								RA8875_PWRR_DISPOFF | RA8875_PWRR_SLEEP);
						// vTaskDelay(100);
					} else {
						//1)wake up with display off(100ms)
						RA8875_writeRegister(
								RA8875_PWRR,
								RA8875_PWRR_DISPOFF);
						// vTaskDelay(100);
						//2)bring back the pll
						RA8875__setSysClock(initStrings[_initIndex][0], initStrings[_initIndex][1],
								initStrings[_initIndex][2]);
						//RA8875_writeRegister(RA8875_PCSR,initStrings[_initIndex][2]);//Pixel Clock Setting Register
						// vTaskDelay(20);
						RA8875_writeRegister(
								RA8875_PWRR,
								RA8875_PWRR_NORMAL | RA8875_PWRR_DISPON);//disp on
						// vTaskDelay(20);
						//4)resume SPI speed
#if defined(_FASTCPU)
						_slowDownSPI(false);
#else
#if defined(SPI_HAS_TRANSACTION)
#if defined(__MKL26Z64__)
						if (_altSPI) {
							_SPImaxSpeed = MAXSPISPEED2;
						} else {
							_SPImaxSpeed = MAXSPISPEED;
						}
#else
						_SPImaxSpeed = MAXSPISPEED;
#endif
#else
#if defined(___DUESTUFF) && defined(SPI_DUE_MODE_EXTENDED)
						SPI_setClockDivider(_cs,SPI_SPEED_WRITE);
#else
						SPI_setClockDivider(SPI_SPEED_WRITE);
#endif
#endif
#endif
						//5)PLL afterburn!
						RA8875__setSysClock(sysClockPar[_initIndex][0], sysClockPar[_initIndex][1],
								initStrings[_initIndex][2]);
						//5)turn on backlight
						if (_displaySize == Adafruit_480x272
								|| _displaySize
								== Adafruit_800x480/* || _displaySize == Adafruit_640x480*/)
						GPIOX(true);
						//RA8875_writeRegister(RA8875_PWRR, RA8875_PWRR_NORMAL);
					}
				}
			}

			/*
			 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			 +							SPI & LOW LEVEL STUFF									 +
			 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			 */

			/**************************************************************************/
			/*!

			 */
			/**************************************************************************/
			uint8_t RA8875::readStatus(void) {
				return _readData(true);
			}

			int osDelay(uint32_t millisec);

			void RA8875::// vTaskDelay(uint32_t length) {

				osDelay(length);
			}

			/*
			 void RA8875::debugData(uint16_t data,uint8_t len)
			 {
			 int i;
			 for (i=len-1; i>=0; i--){
			 if (bitRead(data,i)==1){
			 Serial.print("1");
			 } else {
			 Serial.print("0");
			 }
			 }
			 Serial.print(" -> 0x");
			 Serial.print(data,HEX);
			 Serial.print("\n");
			 }
			 */

			/*
			 void RA8875::showLineBuffer(uint8_t data[],int len)
			 {
			 int i;
			 for (i=0; i<len; i++){
			 if (data[i] == 1){
			 Serial.print("1");
			 } else {
			 Serial.print("0");
			 }
			 }
			 Serial.print("\n");
			 }
			 */
#endif
#endif
/** @} */