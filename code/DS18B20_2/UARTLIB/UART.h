/*
 * UART.h
 *
 * Created: 03.08.2022 13:13:31
 *  Author: Евгений
 */ 


#ifndef UART_H_
#define UART_H_

	void uart_init(uint32_t boud);
	void uart_writeSerial(char* str);

#endif /* UART_H_ */