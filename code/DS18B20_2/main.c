
#include <stdlib.h>
#include "main.h"
#include "../UARTLIB/UART.h"
#include "../DS28S20LIB/ds18S20.h"
#include "../TM1637LIB/TM1637.h"
#include "../EEPROMLIB/EEPROM.h"

#define DBG 1

#define WAIT_TIME 3000  // in milliseconds
#define STARTUP_MESSAGE_SEGMENTS ((uint8_t[]) { TM1637_SPAT_BLANK, TM1637_SPAT_H, TM1637_SPAT_I, TM1637_SPAT_BLANK })
#define MAX_RETRY_READ18B20 10

#define SETTINGS_KEY 0xA5B6

//LED
#define LED_PORT		PORTB
#define LED_PIN			PB0

//Buttons
#define BTN_SET_PORT	PORTC
#define BTN_SET_PIN		PC1
#define BTN_NEXT_PORT	PORTC
#define BTN_NEXT_PIN	PC2
#define BTN_PREV_PORT	PORTC
#define BTN_PREV_PIN	PC3

//Fans
#define FAN1_PORT		PORTD
#define FAN1_PIN		PD2
#define FAN2_PORT		PORTD
#define FAN2_PIN		PD3

//Menu
#define MENU_DELAY		100
#define MIN_TEMP		25
#define MAX_TEMP		60
#define MIN_TRES		1
#define MAX_TRES		11
#define MIN_BRIGH		1
#define MAX_BRIGH		7


typedef struct  
{
	uint16_t key;
	uint8_t t1_on;
	uint8_t t2_on;
	uint8_t brightness;
	uint8_t ghyst; 
	uint16_t reserve;

} Settings;

Settings settings;

TSDS18x20 DS18x20;
TSDS18x20 *pDS18x20 = &DS18x20;
static uint8_t cur_temp = 0;
static bool meas_error = false;
static uint8_t curr_retry = 0;

//state fan 1 and 2	
static bool state1 = false;
static bool state2 = false;

//Animation
#define DISP_CHNG_DELAY 30UL
#define ANIM_STAGE_STEP 1

static bool animate = false;
static uint8_t animate_cnt = 0;
static uint8_t anim_stage = 0;

void read_settings();
void save_settings();

static inline void wait(void) {
	_delay_ms(WAIT_TIME);
}

static inline void w(void) {
	_delay_ms(WAIT_TIME * 0.1);
}

void display_temp() {
	
	if (meas_error) {
		TM1637_setSegment(TM1637_SPAT_BLANK, 0);
		TM1637_setSegment(TM1637_SPAT_e, 1);
		TM1637_setSegment(TM1637_SPAT_r, 2);
		TM1637_setSegment(TM1637_SPAT_r, 3);		
		return;
	}
	
	uint8_t data[8] = { 16, 16 };
	uint8_t _digits = 2;

	uint8_t v = cur_temp;
	int last = _digits;

	for (int i = 0; i < last; i++)
	{
		uint8_t t = v / 10;
		data[_digits - i - 1] = v - 10 * t;  // faster than %
		v = t;
	}
		
	// Display degrees at position 0
	TM1637_displayDigits(data, 2, 0);
		
	// Display "degC" at position 2
	TM1637_setSegment(TM1637_SPAT_DEGREE, 2);
	TM1637_setSegment(TM1637_SPAT_C, 3);

}

void update_display() {
	
	animate_cnt++;
	if(animate_cnt == DISP_CHNG_DELAY) {
		
		if((state1 || state2) && !animate) {
			animate = true;
			uart_writeSerial("Animation on\r\n");
		} else {
			if(animate) animate = false;
		}
		animate_cnt = 0;
	}
	
	if(!animate) {
		display_temp();
		return;
	}
	
	if(!(animate_cnt % ANIM_STAGE_STEP)) {
		anim_stage++;
		if (anim_stage > 3) anim_stage = 0;
	}
	
	uint8_t seg1 = TM1637_SPAT_BLANK;
	uint8_t seg2 = TM1637_SPAT_BLANK;
	
	switch (anim_stage) {
		
		case 0:
			seg1 = TM1637_SPAT_COR_TL;
			seg2 = TM1637_SPAT_COR_BR;
			break;
				
		case 1:
			seg1 =  TM1637_SPAT_HBAR_TB;
			seg2 =	TM1637_SPAT_HBAR_TB;
			break;
			
		case 2:
			seg1 =  TM1637_SPAT_COR_BL;
			seg2 =	TM1637_SPAT_COR_TR;
			break;
			
		case 3:
			seg1 =  TM1637_SPAT_VBAR_L;
			seg2 =	TM1637_SPAT_VBAR_R;
			break;
	}
	
	uint8_t segments[4] = {seg1, seg2, TM1637_SPAT_BLANK, TM1637_SPAT_BLANK};
	if (state2) {
		segments[2] = seg1;
		segments[3] = seg2;
	}
	//TM1637_clear();
	TM1637_setSegments(segments, 4, 0);		

}

void on_off_fans() {
		
	if(cur_temp >= settings.t1_on && !state1) {
		FAN1_PORT |= (1<<FAN1_PIN);	
		state1 = true;
		#ifdef DBG
			uart_writeSerial("Fan 1 starts\r\n");
		#endif // DBG
		
	}
	
	if(cur_temp >= settings.t2_on && !state2) {
		FAN2_PORT |= (1<<FAN2_PIN);
		state2 = true;
		#ifdef DBG
			uart_writeSerial("Fan 2 starts\r\n");
		#endif // DBG
	}
	
	if (cur_temp < settings.t1_on - settings.ghyst && state1) {
		FAN1_PORT &= ~(1<<FAN1_PIN);
		state1 = false;	
		#ifdef DBG
			uart_writeSerial("Fan 1 stops\r\n");
		#endif // DBG
	}
	
	if (cur_temp < settings.t2_on - settings.ghyst && state2) {
		FAN2_PORT &= ~(1<<FAN2_PIN);
		state2 = false;
		#ifdef DBG
			uart_writeSerial("Fan 2 stops\r\n");
		#endif // DBG
	}
	
}	

void read_settings() {
	
	 EEPROM_read_block(0, sizeof(Settings), (uint8_t*)&settings);
	 if(settings.key != SETTINGS_KEY) {
			
				
		#ifdef DBG
			 char buffer[10];
			 itoa(settings.key, buffer, 10);
			 uart_writeSerial(buffer);
			 uart_writeSerial("\r\n");
		#endif // DBG
		 
		 settings.key = SETTINGS_KEY;
		 settings.t1_on = 33;
		 settings.t2_on = 40;
		 settings.brightness = TM1637_MAX_BRIGHTNESS;
		 settings.ghyst = 3;
		 save_settings();
		
	 } 
	 
	#ifdef DBG
	 else {
		uart_writeSerial("Settings ok!\r\n");	 
	 }
	#endif // DBG
	 	
}

void save_settings() {
	EEPROM_write_block(0, sizeof(Settings), (uint8_t*)&settings);
}	

void set_param(uint8_t num, uint8_t dir) {
	
	if(dir == 0) return;
	
	switch(num) {
		
		case 0: //t1_on
			settings.t1_on += dir;
			break;	
		case 1: //t2_on
			settings.t2_on += dir;
			break;
		case 2: //threshold
			settings.ghyst += dir;
			break;
		case 3: //bright
			settings.brightness += dir;
			break;		
	}
	
	//Bounding temp
	if(settings.t1_on < MIN_TEMP) settings.t1_on = MIN_TEMP;
	if(settings.t2_on < MIN_TEMP) settings.t2_on = MIN_TEMP;
	if(settings.t1_on > MAX_TEMP) settings.t1_on = MAX_TEMP;
	if(settings.t2_on > MAX_TEMP) settings.t2_on = MAX_TEMP;
	
	//Bounding threshold
	if(settings.ghyst < MIN_TRES) settings.ghyst = MIN_TRES;
	if(settings.ghyst > MAX_TRES) settings.ghyst = MAX_TRES;
	
	//Bounding brightness
	if(settings.brightness < MIN_BRIGH) settings.brightness = MIN_BRIGH;
	if(settings.brightness > MAX_BRIGH) settings.brightness = MAX_BRIGH;
		
}

void display_menu(){
	
	int menu_item = 0;
	int direction = 0;
	int menu_display_cnt = 0;
	
	uint8_t segs[4] = {TM1637_SPAT_BLANK, TM1637_SPAT_BLANK, TM1637_SPAT_BLANK, TM1637_SPAT_BLANK};
		
	_delay_ms(150);
		
	while(menu_display_cnt < MENU_DELAY) {
		
		menu_display_cnt++;
	
		if(!(PINC & (1<<BTN_SET_PIN))) {
		
			menu_item++;
			menu_display_cnt = 0;
			if(menu_item > 4) {
			
				save_settings();
				TM1637_turnOnAndSetBrightness(settings.brightness);
				return;
			
			}
		}
	
		if(!(PINC & (1<<BTN_NEXT_PIN))) {
			menu_display_cnt = 0;
			direction = 1; 
		}
	
		if(!(PINC & (1<<BTN_PREV_PIN))) {
			menu_display_cnt = 0;
			direction = -1;
		}
	
		//Display menu
		switch (menu_item) {
		case 0: //t1_on
			set_param(0, direction);		
			segs[0] = TM1637_SPAT_t;
			segs[1] = TM1637_SPAT_1 | TM1637_SPAT_DOT;
			segs[2] = TM1637_digitToSegment[settings.t1_on / 10];
			segs[3] = TM1637_digitToSegment[settings.t1_on % 10];
			break;
		case 1: //t2_on
			set_param(1, direction);
			segs[0] = TM1637_SPAT_t;
			segs[1] = TM1637_SPAT_2 | TM1637_SPAT_DOT;
			segs[2] = TM1637_digitToSegment[settings.t2_on / 10];
			segs[3] = TM1637_digitToSegment[settings.t2_on % 10];
			break;
		case 2: //threhold
			set_param(2, direction);
			segs[0] = TM1637_SPAT_t;
			segs[1] = TM1637_SPAT_h | TM1637_SPAT_DOT;
			segs[2] = TM1637_digitToSegment[settings.ghyst / 10];
			segs[3] = TM1637_digitToSegment[settings.ghyst % 10];
			break;
		case 3: //brightness
			set_param(3, direction);
			segs[0] = TM1637_SPAT_b;
			segs[1] = TM1637_SPAT_r | TM1637_SPAT_DOT;
			segs[2] = TM1637_digitToSegment[settings.brightness / 10];
			segs[3] = TM1637_digitToSegment[settings.brightness % 10];
			break;
		}
		direction = 0;
		TM1637_setSegments(segs, 4, 0);
		_delay_ms(150);
	}
	
	//No changes after timeout
	read_settings();
	TM1637_turnOnAndSetBrightness(settings.brightness);	
}

void temp_measurement() {
	
	// Initiate a temperature conversion and get the temperature reading
	if (DS18x20_MeasureTemperature(pDS18x20))
	{
				
		cur_temp = (uint8_t) DS18x20_TemperatureValue(pDS18x20);
		curr_retry = 0;
		meas_error = false;
				
		// Send the temperature over serial port
		#ifdef DBG
			char buffer[10];
			uart_writeSerial("Current Temperature is: ");
			itoa(cur_temp, buffer, 10);
			uart_writeSerial(buffer);
			uart_writeSerial("C\r\n");
		#endif // DBG
								
		} else {
				
		#ifdef DBG
			uart_writeSerial("CRC error!!!\r\n");
		#endif // DBG
		meas_error = true;
		curr_retry++;
		if(curr_retry >= MAX_RETRY_READ18B20) {
			cli();
			LED_PORT |= (1<<LED_PIN);
			while(1);
		}
	}

}

void timer_init() {
	
	
	TCCR1B |= (1<<WGM12); // устанавливаем режим СТС (сброс по совпадению)
	TIMSK |= (1<<OCIE1A); // устанавливаем бит разрешения прерывания 1ого счетчика по совпадению с OCR1A(H и L)
	OCR1AH = 0b10000000; //записываем в регистр число для сравнения (32768)
	OCR1AL = 0b00000000;
	TCCR1B |= (1<<CS12) | (1<<CS10); //установим делитель (1024).
	
}

ISR (TIMER1_COMPA_vect) {
	
	cli();	
	temp_measurement();
	on_off_fans();
	sei();

}

int main(void)
{
    
	#ifdef DBG
		uint8_t msg[] = "----Start!----\r\n";
		uart_init(9600);
		uart_writeSerial((char*)msg);
	#endif // DBG
	
	read_settings();
	
	TM1637_init();
	TM1637_setSegments(STARTUP_MESSAGE_SEGMENTS, TM1637_DIGITS_COUNT, 0);
	//TM1637_turnOnAndSetBrightness(TM1637_MAX_BRIGHTNESS);
	TM1637_turnOnAndSetBrightness(settings.brightness);
	wait();
	
	//LED
	//DDRB |= (1<<PB0); //PB0 - OUT
	*(&LED_PORT-1) |= (1<<LED_PIN);
	
	//FANS
	// DDRD |= (1<<PD2) | (1<<PD3);
	*(&FAN1_PORT-1) |= (1<<FAN1_PIN);
	*(&FAN2_PORT-1) |= (1<<FAN2_PIN);
	
	//BUTTONS
	//DDRC &= ~(1<<PC1);
	//DDRC &= ~(1<<PC2);
	//DDRC &= ~(1<<PC3);
	*(&BTN_SET_PORT-1) &= ~(1<<BTN_SET_PIN);
	*(&BTN_NEXT_PORT-1) &= ~(1<<BTN_NEXT_PIN);
	*(&BTN_PREV_PORT-1) &= ~(1<<BTN_PREV_PIN);
	
		
	//char buffer[10];
	
	// Init DS18B20 sensor
	if (DS18x20_Init(pDS18x20, &PORTC, PC0))
	{
		#ifdef DBG		
			uart_writeSerial("Error!!! Can not find 1-Wire device attached on the bus!\r\n");
		#endif // DBG
		
		LED_PORT |= (1<<LED_PIN);
		while(1);
	}
	#ifdef DBG
	else
		uart_writeSerial("1-Wire device detected on the bus.\r\n");
	#endif // DBG

	timer_init();
	sei();

	// Set DS18B20 resolution to 9 bits.
	DS18x20_SetResolution(pDS18x20, CONF_RES_9b);
	DS18x20_WriteScratchpad(pDS18x20);
	
    while (1) 
    {
		
		update_display();
				
		if(!(PINC & (1<<BTN_SET_PIN))) {
			display_menu();	
		}
							
		//LED
		LED_PORT ^= (1<<LED_PIN);
		_delay_ms(100);
		
    }
}

