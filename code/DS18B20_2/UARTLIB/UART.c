/*
 * CFile1.c
 *
 * Created: 03.08.2022 13:12:55
 *  Author: Евгений
 */ 

//#include "main.h"
#include <avr/io.h>
#include "UART.h"
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

static bool is_init = false;

void uart_init(uint32_t boud) {
	
	//скорость=частота_кварца/(16*битрейт -1);
	
	uint8_t bset = F_CPU / (16 * boud) - 1;
	UBRRL=bset;
	UCSRB=(1<<TXEN)|(1<<RXEN);
	UCSRC=(1<<URSEL)|(3<<UCSZ0);
	
	is_init = true;
	
}

void uart_writeSerial(char* str) {
	
	if(!is_init) return;
	
	for(uint16_t i=0; i < strlen(str); i++)		//fixed см. коментарии (25.07.2020)
	{
		while(!(UCSRA&(1<<UDRE))); // wait ready of port
		UDR = str[i];
	}
	
	while(!(UCSRA&(1<<UDRE)));
	
}