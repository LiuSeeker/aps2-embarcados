#include <asf.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef struct {
	const uint8_t *data;
	uint16_t width;
	uint16_t height;
	uint8_t dataSize;
} tImage;

#include "img/imgcancel.h"
#include "img/imglocked.h"
#include "img/imgok.h"
#include "img/imgpause.h"
#include "img/imgplay.h"
#include "img/imgunlocked.h"
#include "img/imgret.h"
#include "img/imgstop.h"

#include "img/imgcentrifuga.h"
#include "img/imgdiario.h"
#include "img/imgenxague.h"
#include "img/imgfast.h"
#include "img/imgheavy.h"

#include "conf_board.h"
#include "conf_example.h"
#include "conf_uart_serial.h"
#include "ioport.h"
#include "fonts/tfont.h"
#include "fonts/calibri_36.h"
#include "fonts/calibri_24.h"
#include "maquina1.h"

/************************************************************************/
/* DEFINES                                                              */
/************************************************************************/

t_ciclo *g_modo;

#define MAX_ENTRIES        3
#define STRING_LENGTH     70

#define USART_TX_MAX_LENGTH     0xff

#define STRING_EOL    "\r\n"
#define STRING_HEADER "-- SAME70 LCD DEMO --"STRING_EOL	\
"-- "BOARD_NAME " --"STRING_EOL	\
"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

#define LED_PIO_ID	   ID_PIOC
#define LED_PIO        PIOC
#define LED_PIN		   8
#define LED_PIN_MASK   (1<<LED_PIN)

#define BUT_PIO      PIOA
#define BUT_PIO_ID   ID_PIOA
#define BUT_IDX  11
#define BUT_IDX_MASK (1 << BUT_IDX)





/************************************************************************/
/* VAR globais                                                          */
/************************************************************************/

struct ili9488_opt_t g_ili9488_display_opt;

const uint32_t BUTTON_W = 120;
const uint32_t BUTTON_H = 150;
const uint32_t BUTTON_BORDER = 2;
const uint32_t BUTTON_X = ILI9488_LCD_WIDTH/2;
const uint32_t BUTTON_Y = ILI9488_LCD_HEIGHT/2;

const uint32_t BNEXT_R = 25;
const uint32_t BNEXT_X = ILI9488_LCD_WIDTH-30;
const uint32_t BNEXT_Y = ILI9488_LCD_HEIGHT-60;

const uint32_t BPREV_R = 25;
const uint32_t BPREV_X = 30;
const uint32_t BPREV_Y = ILI9488_LCD_HEIGHT-60;

const uint32_t BOK_R = 25;
const uint32_t BOK_X = ILI9488_LCD_WIDTH/2;
const uint32_t BOK_Y = ILI9488_LCD_HEIGHT-60;

const uint32_t B1_R = 30;
const uint32_t B1_X = 60;
const uint32_t B1_Y = ILI9488_LCD_HEIGHT/2-20;
const uint32_t B2_R = 30;
const uint32_t B2_X = 140;
const uint32_t B2_Y = ILI9488_LCD_HEIGHT/2-20;

const uint32_t B3_R = 30;
const uint32_t B3_X = 60;
const uint32_t B3_Y = ILI9488_LCD_HEIGHT/2+60;
const uint32_t B4_R = 30;
const uint32_t B4_X = 140;
const uint32_t B4_Y = ILI9488_LCD_HEIGHT/2+60;

const uint32_t BD_R = 30;
const uint32_t BD_X = 220;
const uint32_t BD_Y = ILI9488_LCD_HEIGHT/2-20;
const uint32_t BC_R = 30;
const uint32_t BC_X = 220;
const uint32_t BC_Y = ILI9488_LCD_HEIGHT/2+60;

volatile int year = 2018;
volatile int month = 3;
volatile int day = 19;
volatile int week = 12;
volatile int hour = 15;
volatile int minute = 30;
volatile int second = 0;

volatile int counter = 0;
volatile int n_counter = 0;

typedef struct{
	int seq[4];
	int counter;
} stack;

volatile stack lock = {{-1,-1,-1,-1}, 0};
volatile stack unlock = {{-1,-1,-1,-1}, 0};

volatile Bool flag_lavagem = false;
volatile Bool flag_concluido = false;
volatile Bool flag_menu = true;

volatile Bool flag_stop_alarm = false;

volatile Bool lock_menu = false;
volatile Bool lock_concluido = true;
volatile Bool lock_lavagem = true;
volatile Bool lock_alerta = true;

volatile Bool porta_aberta = false;
volatile Bool alerta = false;
volatile Bool senha = false;
volatile Bool pause = false;
volatile Bool bsenha = false;

volatile uint32_t last_status = 0;


/************************************************************************/
/* PROTOTYPES                                                           */
/************************************************************************/

void mxt_handler(struct mxt_device *device);
void RTC_Handler();
void BUT_handler();

static void configure_lcd(void);
static void mxt_init(struct mxt_device *device);
void draw_screen(void);
void draw_button(uint32_t clicked);
void draw_next_button();
void draw_prev_button();
void font_draw_text(tFont *font, const char *text, int x, int y, int spacing);
void pin_toggle(Pio *pio, uint32_t mask);
uint32_t convert_axis_system_x(uint32_t touch_y);
uint32_t convert_axis_system_y(uint32_t touch_x);
void update_screen(uint32_t tx, uint32_t ty);
void RTC_init();
void LED_init(int estado);
void BUT_init();
int in_circle(uint32_t tx, uint32_t ty, uint32_t r);
t_ciclo *initMenuOrder();
void draw_lavagem(t_ciclo *mod);
void set_alarm();
void draw_concluido(t_ciclo *mod);
void draw_senha();
void stack_reset(stack *stak);
void stack_push(int n, stack *stak);
void stack_pop(stack *stak);
void reset_alarm();

/************************************************************************/
/* Handlers                                                             */
/************************************************************************/

void mxt_handler(struct mxt_device *device)
{
	/* USART tx buffer initialized to 0 */
	char tx_buf[STRING_LENGTH * MAX_ENTRIES] = {0};
	uint8_t i = 0; /* Iterator */

	/* Temporary touch event data struct */
	struct mxt_touch_event touch_event;

	/* Collect touch events and put the data in a string,
	* maximum 2 events at the time */
	do {
		/* Temporary buffer for each new touch event line */
		char buf[STRING_LENGTH];
		
		/* Read next next touch event in the queue, discard if read fails */
		if (mxt_read_touch_event(device, &touch_event) != STATUS_OK) {
			continue;
		}
		
		// eixos trocados (quando na vertical LCD)
		uint32_t conv_x = convert_axis_system_x(touch_event.y);
		uint32_t conv_y = convert_axis_system_y(touch_event.x);
		
		
		/* Format a new entry in the data string that will be sent over USART */
		sprintf(buf, "Nr: %1d, X:%4d, Y:%4d, Status: %d conv X:%3d Y:%3d\n\r\n", touch_event.id, touch_event.x, touch_event.y, touch_event.status, conv_x, conv_y);
		
		if(last_status <= 60){
			update_screen(conv_x, conv_y);
		}
		last_status = touch_event.status;

		/* Add the new string to the string buffer */
		strcat(tx_buf, buf);
		i++;

		/* Check if there is still messages in the queue and
		* if we have reached the maximum numbers of events */
	} while ((mxt_is_message_pending(device)) & (i < MAX_ENTRIES));

	/* If there is any entries in the buffer, send them over USART */
	if (i > 0) {
		usart_serial_write_packet(USART_SERIAL_EXAMPLE, (uint8_t *)tx_buf, strlen(tx_buf));
	}
}

void RTC_Handler(void)
{
	uint32_t ul_status = rtc_get_status(RTC);

	/*
	*  Verifica por qual motivo entrou
	*  na interrupcao, se foi por segundo
	*  ou Alarm
	*/
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		if(!flag_stop_alarm){
			set_alarm();
		}
	}
	
	flag_lavagem = true;
	if(!pause){
		counter += 1;
	}
	
	rtc_clear_status(RTC, RTC_SR_ALARM);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

void BUT_handler(void){
	if(lock_lavagem){
		pin_toggle(LED_PIO, LED_PIN_MASK);
		if(!porta_aberta){
			porta_aberta = true;
		}
		else{
			porta_aberta = false;
		}
	}
}

/************************************************************************/
/* Funcoes                                                              */
/************************************************************************/

void stack_reset(stack *stak){
	stak->seq[0] = -1;
	stak->seq[1] = -1;
	stak->seq[2] = -1;
	stak->seq[3] = -1;
	stak->counter = 0;
}

void stack_push(int n, stack *stak){
	if(stak->counter >= 4){
		return;
	}
	stak->seq[stak->counter] = n;
	stak->counter += 1; 
}

void stack_pop(stack *stak){
	if(stak->counter <= 0){
		return;
	}
	stak->counter -= 1;
	stak->seq[stak->counter] = -1;
}

static void configure_lcd(void){
	/* Initialize display parameter */
	g_ili9488_display_opt.ul_width = ILI9488_LCD_WIDTH;
	g_ili9488_display_opt.ul_height = ILI9488_LCD_HEIGHT;
	g_ili9488_display_opt.foreground_color = COLOR_CONVERT(COLOR_WHITE);
	g_ili9488_display_opt.background_color = COLOR_CONVERT(COLOR_WHITE);

	/* Initialize LCD */
	ili9488_init(&g_ili9488_display_opt);
}

static void mxt_init(struct mxt_device *device)
{
	enum status_code status;

	/* T8 configuration object data */
	uint8_t t8_object[] = {
		0x0d, 0x00, 0x05, 0x0a, 0x4b, 0x00, 0x00,
		0x00, 0x32, 0x19
	};

	/* T9 configuration object data */
	uint8_t t9_object[] = {
		0x8B, 0x00, 0x00, 0x0E, 0x08, 0x00, 0x80,
		0x32, 0x05, 0x02, 0x0A, 0x03, 0x03, 0x20,
		0x02, 0x0F, 0x0F, 0x0A, 0x00, 0x00, 0x00,
		0x00, 0x18, 0x18, 0x20, 0x20, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x02,
		0x02
	};

	/* T46 configuration object data */
	uint8_t t46_object[] = {
		0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x03,
		0x00, 0x00
	};
	
	/* T56 configuration object data */
	uint8_t t56_object[] = {
		0x02, 0x00, 0x01, 0x18, 0x1E, 0x1E, 0x1E,
		0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
		0x1E, 0x1E, 0x1E, 0x1E, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00
	};

	/* TWI configuration */
	twihs_master_options_t twi_opt = {
		.speed = MXT_TWI_SPEED,
		.chip  = MAXTOUCH_TWI_ADDRESS,
	};

	status = (enum status_code)twihs_master_setup(MAXTOUCH_TWI_INTERFACE, &twi_opt);
	Assert(status == STATUS_OK);

	/* Initialize the maXTouch device */
	status = mxt_init_device(device, MAXTOUCH_TWI_INTERFACE,
	MAXTOUCH_TWI_ADDRESS, MAXTOUCH_XPRO_CHG_PIO);
	Assert(status == STATUS_OK);

	/* Issue soft reset of maXTouch device by writing a non-zero value to
	* the reset register */
	mxt_write_config_reg(device, mxt_get_object_address(device,
	MXT_GEN_COMMANDPROCESSOR_T6, 0)
	+ MXT_GEN_COMMANDPROCESSOR_RESET, 0x01);

	/* Wait for the reset of the device to complete */
	delay_ms(MXT_RESET_TIME);

	/* Write data to configuration registers in T7 configuration object */
	mxt_write_config_reg(device, mxt_get_object_address(device,
	MXT_GEN_POWERCONFIG_T7, 0) + 0, 0x20);
	mxt_write_config_reg(device, mxt_get_object_address(device,
	MXT_GEN_POWERCONFIG_T7, 0) + 1, 0x10);
	mxt_write_config_reg(device, mxt_get_object_address(device,
	MXT_GEN_POWERCONFIG_T7, 0) + 2, 0x4b);
	mxt_write_config_reg(device, mxt_get_object_address(device,
	MXT_GEN_POWERCONFIG_T7, 0) + 3, 0x84);

	/* Write predefined configuration data to configuration objects */
	mxt_write_config_object(device, mxt_get_object_address(device,
	MXT_GEN_ACQUISITIONCONFIG_T8, 0), &t8_object);
	mxt_write_config_object(device, mxt_get_object_address(device,
	MXT_TOUCH_MULTITOUCHSCREEN_T9, 0), &t9_object);
	mxt_write_config_object(device, mxt_get_object_address(device,
	MXT_SPT_CTE_CONFIGURATION_T46, 0), &t46_object);
	mxt_write_config_object(device, mxt_get_object_address(device,
	MXT_PROCI_SHIELDLESS_T56, 0), &t56_object);

	/* Issue recalibration command to maXTouch device by writing a non-zero
	* value to the calibrate register */
	mxt_write_config_reg(device, mxt_get_object_address(device,
	MXT_GEN_COMMANDPROCESSOR_T6, 0)
	+ MXT_GEN_COMMANDPROCESSOR_CALIBRATE, 0x01);
}

void draw_screen(void) {
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
	ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, ILI9488_LCD_HEIGHT-1);
}

void draw_button(uint32_t clicked) {
	static uint32_t last_state = 255; // undefined
	if(clicked == last_state) return;
	
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_BLACK));
	ili9488_draw_filled_rectangle(BUTTON_X-BUTTON_W/2, BUTTON_Y-BUTTON_H/2, BUTTON_X+BUTTON_W/2, BUTTON_Y+BUTTON_H/2);
	if(clicked) {
		ili9488_set_foreground_color(COLOR_CONVERT(COLOR_TOMATO));
		pin_toggle(LED_PIO, LED_PIN_MASK);
		ili9488_draw_filled_rectangle(BUTTON_X-BUTTON_W/2+BUTTON_BORDER, BUTTON_Y+BUTTON_BORDER, BUTTON_X+BUTTON_W/2-BUTTON_BORDER, BUTTON_Y+BUTTON_H/2-BUTTON_BORDER);
		} else {
		ili9488_set_foreground_color(COLOR_CONVERT(COLOR_GREEN));
		pin_toggle(LED_PIO, LED_PIN_MASK);
		ili9488_draw_filled_rectangle(BUTTON_X-BUTTON_W/2+BUTTON_BORDER, BUTTON_Y-BUTTON_H/2+BUTTON_BORDER, BUTTON_X+BUTTON_W/2-BUTTON_BORDER, BUTTON_Y-BUTTON_BORDER);
	}
	last_state = clicked;
}

void draw_next_button() {
	ili9488_draw_pixmap(BNEXT_X-imgplay.width/2, BNEXT_Y-imgplay.height/2, imgplay.width, imgplay.height+2, imgplay.data);
	if (!lock_menu){
		ili9488_draw_pixmap(BPREV_X-imgret.width/2, BPREV_Y-imgret.height/2, imgret.width, imgret.height+2, imgret.data);
		font_draw_text(&calibri_24, "Prox.", ILI9488_LCD_WIDTH-55, ILI9488_LCD_HEIGHT-24, 1);
	}
	if (!lock_lavagem){
		ili9488_draw_pixmap(BPREV_X-imgstop.width/2, BPREV_Y-imgstop.height/2, imgstop.width, imgstop.height+2, imgstop.data);
		font_draw_text(&calibri_24, "Cont.", ILI9488_LCD_WIDTH-55, ILI9488_LCD_HEIGHT-24, 1);
	}
}

void draw_prev_button() {
	if (!lock_alerta){
		ili9488_draw_pixmap(BPREV_X-imgret.width/2, BPREV_Y-imgret.height/2, imgret.width, imgret.height+2, imgret.data);
		font_draw_text(&calibri_24, "Voltar", 5, ILI9488_LCD_HEIGHT-24, 1);
	}
	
	if (!lock_menu){
		ili9488_draw_pixmap(BPREV_X-imgret.width/2, BPREV_Y-imgret.height/2, imgret.width, imgret.height+2, imgret.data);
		font_draw_text(&calibri_24, "Anter.", 5, ILI9488_LCD_HEIGHT-24, 1);
	}
	
	if (!lock_lavagem){
		ili9488_draw_pixmap(BPREV_X-imgstop.width/2, BPREV_Y-imgstop.height/2, imgstop.width, imgstop.height+2, imgstop.data);
		font_draw_text(&calibri_24, "Cancel.", 5, ILI9488_LCD_HEIGHT-24, 1);
	}
	
}

void draw_ok_button() {
	if(!lock_menu || !lock_concluido){
		ili9488_draw_pixmap(BOK_X-imgok.width/2, BOK_Y-imgok.height/2, imgok.width, imgok.height+2, imgok.data);
		font_draw_text(&calibri_24, "OK", ILI9488_LCD_WIDTH/2-12, ILI9488_LCD_HEIGHT-24, 1);
	}
	if(!lock_lavagem){
		if(senha){
			ili9488_draw_pixmap(BOK_X-imglocked.width/2, BOK_Y-imglocked.height, imglocked.width, imglocked.height+2, imglocked.data);
			font_draw_text(&calibri_24, "Bloq.", ILI9488_LCD_WIDTH/2-20, ILI9488_LCD_HEIGHT-24, 1);
		}
		else{
			ili9488_draw_pixmap(BOK_X-imgpause.width/2, BOK_Y-imgpause.height/2, imgpause.width, imgpause.height+2, imgpause.data);
			font_draw_text(&calibri_24, "Pausar", ILI9488_LCD_WIDTH/2-30, ILI9488_LCD_HEIGHT-24, 1);
		}
	}
	
	/*
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_GREEN));
	ili9488_draw_filled_circle(BOK_X, BOK_Y, BOK_R);
	*/
}

void draw_senha(){
	uint32_t w = 25;
	uint32_t h = 2;
	uint32_t x1 = ILI9488_LCD_WIDTH/2-45;
	uint32_t x2 = ILI9488_LCD_WIDTH/2-15;
	uint32_t x3 = ILI9488_LCD_WIDTH/2+15;
	uint32_t x4 = ILI9488_LCD_WIDTH/2+45;
	uint32_t y = ILI9488_LCD_HEIGHT/2-80;
	
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_BLACK));
	ili9488_draw_filled_rectangle(x1-w/2, y-h/2, x1+w/2, y+h/2);
	ili9488_draw_filled_rectangle(x2-w/2, y-h/2, x2+w/2, y+h/2);
	ili9488_draw_filled_rectangle(x3-w/2, y-h/2, x3+w/2, y+h/2);
	ili9488_draw_filled_rectangle(x4-w/2, y-h/2, x4+w/2, y+h/2);
	
	stack *stac;
	
	if(senha){
		stac = &unlock;
	}
	else{
		stac = &lock;
	}
	
	char buffer0[32];
	sprintf(buffer0, "%d", stac->seq[0]);
	if(stac->seq[0] != -1){
		font_draw_text(&calibri_24, buffer0, x1-5, y-22, 1);
	}
	char buffer1[32];
	sprintf(buffer1, "%d", stac->seq[1]);
	if(stac->seq[1] != -1){
		font_draw_text(&calibri_24, buffer1, x2-5, y-22, 1);
	}
	char buffer2[32];
	sprintf(buffer2, "%d", stac->seq[2]);
	if(stac->seq[2] != -1){
		font_draw_text(&calibri_24, buffer2, x3-5, y-22, 1);
	}
	char buffer3[32];
	sprintf(buffer3, "%d", stac->seq[3]);
	if(stac->seq[3] != -1){
		font_draw_text(&calibri_24, buffer3, x4-5, y-22, 1);
	}
	
}

void font_draw_text(tFont *font, const char *text, int x, int y, int spacing) {
	char *p = text;
	while(*p != NULL) {
		char letter = *p;
		int letter_offset = letter - font->start_char;
		if(letter <= font->end_char) {
			tChar *current_char = font->chars + letter_offset;
			ili9488_draw_pixmap(x, y, current_char->image->width, current_char->image->height, current_char->image->data);
			x += current_char->image->width + spacing;
		}
		p++;
	}
}

void pin_toggle(Pio *pio, uint32_t mask){
	if(pio_get_output_data_status(pio, mask))
	pio_clear(pio, mask);
	else
	pio_set(pio,mask);
}

uint32_t convert_axis_system_x(uint32_t touch_y) {
	// entrada: 4096 - 0 (sistema de coordenadas atual)
	// saida: 0 - 320
	return ILI9488_LCD_WIDTH - ILI9488_LCD_WIDTH*touch_y/4096;
}

uint32_t convert_axis_system_y(uint32_t touch_x) {
	// entrada: 0 - 4096 (sistema de coordenadas atual)
	// saida: 0 - 320
	return ILI9488_LCD_HEIGHT*touch_x/4096;
}

int check_senha(){
	for(int i = 0; i < 4; i++){
		if(lock.seq[i] != unlock.seq[i]){
			return 0;
		}
	}
	return 1;
}

void update_screen(uint32_t tx, uint32_t ty) {
	if(!lock_menu){
		if(tx >= BNEXT_X-BNEXT_R && tx <= BNEXT_X+BNEXT_R) {
			if(ty >= BNEXT_Y-BNEXT_R && ty <= BNEXT_Y+BNEXT_R) {
				g_modo = g_modo->next;
				flag_menu = true;
			}
		}
		if(tx >= BPREV_X-BPREV_R && tx <= BPREV_X+BPREV_R) {
			if(ty >= BPREV_Y-BPREV_R && ty <= BPREV_Y+BPREV_R) {
				g_modo = g_modo->previous;
				flag_menu = true;
			}
		}
		
		if(tx >= BOK_X-BOK_R && tx <= BOK_X+BOK_R) {
			if(ty >= BOK_Y-BOK_R && ty <= BOK_Y+BOK_R) {
				lock_menu = true;
				counter = 0;
				stack_reset(&lock);
				stack_reset(&unlock);
				lock_lavagem = false;
				flag_stop_alarm = false;
				flag_lavagem = true;
			}
		}
		return;
	}
	
	if(!lock_alerta){
		if(tx >= BPREV_X-BPREV_R && tx <= BPREV_X+BPREV_R) {
			if(ty >= BPREV_Y-BPREV_R && ty <= BPREV_Y+BPREV_R) {
				alerta = false;
				lock_menu = false;
				lock_lavagem = true;
				flag_menu = true;
			}
		}
		return;
	}
	
	if(!lock_lavagem){
		if(tx >= B1_X-B1_R && tx <= B1_X+B1_R) {
			if(ty >= B1_Y-B1_R && ty <= B1_Y+B1_R) {
				if(senha){
					stack_push(1, &unlock);
				}
				else{
					stack_push(1, &lock);
				}
				flag_lavagem = true;
			}
		}
		if(tx >= B2_X-B2_R && tx <= B2_X+B2_R) {
			if(ty >= B2_Y-B2_R && ty <= B2_Y+B2_R) {
				if(senha){
					stack_push(2, &unlock);
				}
				else{
					stack_push(2, &lock);
				}
				flag_lavagem = true;
			}
		}
		
		if(tx >= B3_X-B3_R && tx <= B3_X+B3_R) {
			if(ty >= B3_Y-B3_R && ty <= B3_Y+B3_R) {
				if(senha){
					stack_push(3, &unlock);
				}
				else{
					stack_push(3, &lock);
				}
				flag_lavagem = true;
			}
		}
		
		if(tx >= B4_X-B4_R && tx <= B4_X+B4_R) {
			if(ty >= B4_Y-B4_R && ty <= B4_Y+B4_R) {
				if(senha){
					stack_push(4, &unlock);
				}
				else{
					stack_push(4, &lock);
				}
				flag_lavagem = true;
			}
		}
		
		if(tx >= BD_X-BD_R && tx <= BD_X+BD_R) {
			if(ty >= BD_Y-BD_R && ty <= BD_Y+BD_R) {
				if(senha){
					stack_pop(&unlock);
				}
				else{
					stack_pop(&lock);
				}
				flag_lavagem = true;
			}
		}
		
		if(tx >= BC_X-BC_R && tx <= BC_X+BC_R) {
			if(ty >= BC_Y-BC_R && ty <= BC_Y+BC_R) {
				if(senha){
					if(unlock.seq[3] != -1){
						if(check_senha()){
							senha = false;
							stack_reset(&lock);
							stack_reset(&unlock);
						}
					}
				}
				else{
					if(lock.seq[3] != -1){
						senha = true;
						if(pause){
							pause = false;
							flag_stop_alarm = false;
							reset_alarm();
						}
					}
				}
				flag_lavagem = true;
			}
		}
		
		if(!senha){
			if(!pause){
				if(tx >= BOK_X-BOK_R && tx <= BOK_X+BOK_R) {
					if(ty >= BOK_Y-BOK_R && ty <= BOK_Y+BOK_R) {
						pause = true;
						rtc_disable_interrupt(RTC,RTC_IER_ALREN | RTC_IER_SECEN  );
						flag_stop_alarm = true;
						flag_lavagem = true;
					}
				}
				return;
			}
			
			else if(pause){
				if(tx >= BPREV_X-BPREV_R && tx <= BPREV_X+BPREV_R) {
					if(ty >= BPREV_Y-BPREV_R && ty <= BPREV_Y+BPREV_R) {
						pause = false;
						lock_lavagem = true;
						lock_menu = false;
						flag_menu = true;
					}
				}
				else if(tx >= BNEXT_X-BNEXT_R && tx <= BNEXT_X+BNEXT_R) {
					if(ty >= BNEXT_Y-BNEXT_R && ty <= BNEXT_Y+BNEXT_R) {
						pause = false;
						flag_stop_alarm = false;
						reset_alarm();
						flag_lavagem = true;
					}
				}
				return;
			}
		}

	}
	
	if(!lock_concluido){
		if(tx >= BOK_R-BOK_R && tx <= BOK_X+BOK_R) {
			if(ty >= BOK_Y-BOK_R && ty <= BOK_Y+BOK_R) {
				senha = false;
				lock_menu = false;
				lock_concluido = true;
				flag_menu = true;
			}
		}
		return;
	}
	
	
}

int in_circle(uint32_t tx, uint32_t ty, uint32_t r){
	if(sqrt(tx*tx+ty*ty) <= r){
		return 1;
	}
	return 0;
}

void RTC_init(){
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(RTC, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(RTC, year, month, day, week);
	rtc_set_time(RTC, hour, minute, second);
	
	NVIC_DisableIRQ(RTC_IRQn);
	NVIC_ClearPendingIRQ(RTC_IRQn);
	NVIC_SetPriority(RTC_IRQn, 0);
	NVIC_EnableIRQ(RTC_IRQn);
	rtc_enable_interrupt(RTC,  RTC_IER_ALREN);
}

void LED_init(int estado){
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_set_output(LED_PIO, LED_PIN_MASK, estado, 0, 0 );
};

void BUT_init(void){
	pmc_enable_periph_clk(BUT_PIO_ID);
	
	pio_configure(BUT_PIO, PIO_INPUT, BUT_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT_PIO, BUT_IDX_MASK, 50);

	pio_handler_set(BUT_PIO, BUT_PIO_ID, BUT_IDX_MASK, PIO_IT_FALL_EDGE, BUT_handler);

	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 0);
	
	pio_enable_interrupt(BUT_PIO, BUT_IDX_MASK);
}

t_ciclo *initMenuOrder(){
	c_rapido.previous = &c_demo;
	c_rapido.next = &c_diario;

	c_diario.previous = &c_rapido;
	c_diario.next = &c_pesado;

	c_pesado.previous = &c_diario;
	c_pesado.next = &c_enxague;

	c_enxague.previous = &c_pesado;
	c_enxague.next = &c_centrifuga;

	c_centrifuga.previous = &c_enxague;
	c_centrifuga.next = &c_demo;
	
	c_demo.previous = &c_centrifuga;
	c_demo.next = &c_rapido;

	return(&c_demo);
}

void draw_lavagem(t_ciclo *mod){
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
	ili9488_draw_filled_rectangle(0,0,ILI9488_LCD_WIDTH,ILI9488_LCD_HEIGHT);
	
	char buffern[32];
	sprintf(buffern, "Modo: %s", mod->nome);
	font_draw_text(&calibri_24, buffern, 30, 30, 1);
	
	char buffertr[32];
	int tempo = mod->centrifugacaoTempo-counter+mod->enxagueQnt*mod->enxagueTempo;
	
	if(tempo == 0){
		counter = 0;
		flag_stop_alarm = true;
		flag_concluido = true;
		tempo = -1;
		lock_concluido = false;
		lock_lavagem = true;
		return;
	}
	
	sprintf(buffertr, "Tempo restante: %d min", tempo);
	font_draw_text(&calibri_24, buffertr, 30, 70, 1);
	
	if(pause){
		font_draw_text(&calibri_24, "PAUSADO", 100, 110, 1);
	}
	
	draw_senha();
	
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_BLACK));
	ili9488_draw_circle(B1_X, B1_Y, B1_R);
	font_draw_text(&calibri_24, "1", B1_X-5, B1_Y-10, 1);
	ili9488_draw_circle(B2_X, B2_Y, B2_R);
	font_draw_text(&calibri_24, "2", B2_X-5, B2_Y-10, 1);
	ili9488_draw_circle(B3_X, B3_Y, B3_R);
	font_draw_text(&calibri_24, "3", B3_X-5, B3_Y-10, 1);
	ili9488_draw_circle(B4_X, B4_Y, B4_R);
	font_draw_text(&calibri_24, "4", B4_X-5, B4_Y-10, 1);
	
	
	ili9488_draw_pixmap(BD_X-imgret.width/2, BD_Y-imgret.height/2, imgret.width, imgret.height+2, imgret.data);
	ili9488_draw_pixmap(BC_X-imgok.width/2, BC_Y-imgok.height/2, imgok.width, imgok.height+2, imgok.data);
	
	if(!senha){
		if(!pause){
			draw_ok_button();
		}
		else{
			draw_next_button();
			draw_prev_button();
		}
	}
	else{
		draw_ok_button();
	}
	
	
	
}

void set_alarm(){
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	//rtc_set_date_alarm(RTC, 1, month , 1, day);
	
	int hora, min, sec;
	rtc_get_time(RTC, &hora, &min, &sec);
	if (min >= 59){
		rtc_set_time_alarm(RTC, 1, hora+1, 1, min-59, 1, sec);
	}
	rtc_set_time_alarm(RTC, 1, hora, 1, min+1, 1, sec);
}

void draw_concluido(t_ciclo *mod){
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
	ili9488_draw_filled_rectangle(0,0,ILI9488_LCD_WIDTH,ILI9488_LCD_HEIGHT);
	
	char bufferc[32];
	sprintf(bufferc, "Lavagem %s", mod->nome);
	font_draw_text(&calibri_24, bufferc, 30, 30, 1);
	font_draw_text(&calibri_24, "concluida", 30, 60, 1);
	
	
	font_draw_text(&calibri_24, "Pressione OK", 30, 100, 1);
	font_draw_text(&calibri_24, "para retornar", 30, 130, 1);
	
	draw_ok_button();
}

void draw_menu(t_ciclo *mod){
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
	ili9488_draw_filled_rectangle(0,0,ILI9488_LCD_WIDTH,ILI9488_LCD_HEIGHT);
	
	char buffern[32];
	sprintf(buffern, "Modo: %s", mod->nome);
	font_draw_text(&calibri_24, buffern, 30, 30, 1);
	
	char buffert[32];
	sprintf(buffert, "Tempo enxague: %d min", mod->enxagueTempo);
	font_draw_text(&calibri_24, buffert, 30, 70, 1);
	
	char bufferq[32];
	sprintf(bufferq, "Enxagues: %d", mod->enxagueQnt);
	font_draw_text(&calibri_24, bufferq, 30, 110, 1);
	
	char buffercv[32];
	sprintf(buffercv, "Vel centrifug: %d RPM", mod->centrifugacaoRPM);
	font_draw_text(&calibri_24, buffercv, 30, 150, 1);
	
	char bufferct[32];
	sprintf(bufferct, "Tempo centrifug: %d min", mod->centrifugacaoTempo);
	font_draw_text(&calibri_24, bufferct, 30, 190, 1);
	
	char buffertt[32];
	sprintf(buffertt, "Tempo total: %d min", mod->centrifugacaoTempo+mod->enxagueQnt*mod->enxagueTempo);
	font_draw_text(&calibri_24, buffertt, 30, 230, 1);
	
	
	if(!strcmp(mod->nome, "Rapido")){
		ili9488_draw_pixmap(ILI9488_LCD_WIDTH/2-imgdiario.width/2, 290, imgfast.width, imgfast.height+2, imgfast.data);
	}
	else if(!strcmp(mod->nome, "Diario")){
		ili9488_draw_pixmap(ILI9488_LCD_WIDTH/2-imgdiario.width/2, 290, imgdiario.width, imgdiario.height+2, imgdiario.data);
	}
	else if(!strcmp(mod->nome, "Centrifuga")){
		ili9488_draw_pixmap(ILI9488_LCD_WIDTH/2-imgcentrifuga.width/2, 290, imgcentrifuga.width, imgcentrifuga.height+2, imgcentrifuga.data);
	}
	else if(!strcmp(mod->nome, "Enxague")){
		ili9488_draw_pixmap(ILI9488_LCD_WIDTH/2-imgenxague.width/2, 290, imgenxague.width, imgenxague.height+2, imgenxague.data);
	}
	else if(!strcmp(mod->nome, "Pesado")){
		ili9488_draw_pixmap(ILI9488_LCD_WIDTH/2-imgheavy.width/2, 290, imgheavy.width, imgheavy.height+2, imgheavy.data);
	}
	
	draw_next_button();
	draw_prev_button();
	draw_ok_button();
}

void draw_alerta(){
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
	ili9488_draw_filled_rectangle(0,0,ILI9488_LCD_WIDTH,ILI9488_LCD_HEIGHT);
	
	font_draw_text(&calibri_24, "ATENCAO!", 30, 30, 1);
	font_draw_text(&calibri_24, "Porta aberta", 30, 70, 1);
	
	font_draw_text(&calibri_24, "Fechar a porta", 30, 150, 1);
	font_draw_text(&calibri_24, "para iniciar lavagem", 30, 190, 1);
	
	draw_prev_button();
}

void reset_alarm(){
	rtc_clear_time_alarm(RTC);
	rtc_clear_date_alarm(RTC);
	rtc_clear_status(RTC, RTC_SR_ALARM);

	rtc_enable_interrupt(RTC,RTC_IER_ALREN   );
	set_alarm();
}

/************************************************************************/
/* Main Code	                                                        */
/************************************************************************/

int main(void)
{
	struct mxt_device device; /* Device data container */

	/* Initialize the USART configuration struct */
	const usart_serial_options_t usart_serial_options = {
		.baudrate     = USART_SERIAL_EXAMPLE_BAUDRATE,
		.charlength   = USART_SERIAL_CHAR_LENGTH,
		.paritytype   = USART_SERIAL_PARITY,
		.stopbits     = USART_SERIAL_STOP_BIT
	};

	/* Initialize system clocks */
	sysclk_init();
	
	/* Initialize board */
	board_init();
	
	/* Disable the watchdog */
	WDT->WDT_MR = WDT_MR_WDDIS;
	
	/* Configura Leds */
	LED_init(1);
	
	RTC_init();
	
	BUT_init();
	
	/* Configura lcd */
	configure_lcd();
	
	draw_screen();
	//draw_button(0);
	
	
	g_modo = initMenuOrder();
	
	/* Initialize the mXT touch device */
	mxt_init(&device);
	
	/* Initialize stdio on USART */
	stdio_serial_init(USART_SERIAL_EXAMPLE, &usart_serial_options);
	
	/************************************************************************/
	while(true) {
		/* Check for any pending messages and run message handler if any
		* message is found in the queue */
		if (mxt_is_message_pending(&device)) {
			mxt_handler(&device);
		}
		if(flag_menu){
			draw_menu(g_modo);
			flag_menu = false;
		}
		
		
		if(!lock_lavagem && porta_aberta){
			rtc_disable_interrupt(RTC,RTC_IER_ALREN | RTC_IER_SECEN  );
			alerta = true;
			lock_lavagem = true;
			lock_alerta = false;
			flag_lavagem = 0;
			draw_alerta();
		}
		else if(flag_lavagem){
			lock_lavagem = false;
			set_alarm();
			draw_lavagem(g_modo);
			flag_lavagem = false;			
		}
		
		if(alerta && !porta_aberta){
			reset_alarm();
			lock_alerta = true;
			lock_lavagem = false;
			alerta = false;
			draw_lavagem(g_modo);
		}
		if(flag_concluido){
			lock_concluido = false;
			draw_concluido(g_modo);
			flag_concluido = false;
			stack_reset(&lock);
			stack_reset(&unlock);
		}
	}
	return 0;
}
