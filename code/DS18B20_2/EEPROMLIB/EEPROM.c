
#include <avr/io.h>
#include "EEPROM.h"

void EEPROM_write(uint16_t uiAddress, uint8_t ucData) {

	while(EECR & (1<<EEWE)); //ждем освобождения флага окончания последней операцией с памятью
	EEAR = uiAddress; //Устанавливаем адрес
	EEDR = ucData; //Пищем данные в регистр
	EECR |= (1<<EEMWE); //Разрешаем запись
	EECR |= (1<<EEWE); //Пишем байт в память

}

uint8_t EEPROM_read(uint16_t uiAddress) {

	while(EECR & (1<<EEWE)); //ждем освобождения флага окончания последней операцией с памятью
	EEAR = uiAddress; //Устанавливаем адрес
	EECR |= (1<<EERE); //Запускаем операцию считывания из памяти в регистр данных
	return EEDR; //Возвращаем результат

} 

void EEPROM_read_block(uint16_t sAddress, uint16_t len, uint8_t* data) {
	
	uint16_t pos = 0;
	while(len) {
		
		*(data + pos) = EEPROM_read(sAddress + pos);
		pos++;
		len--;
		
	}	
}

void EEPROM_write_block(uint16_t sAddress, uint16_t len, uint8_t* data) {
	
	uint16_t pos = 0;
	while(len) {
		
		EEPROM_write(sAddress + pos, *(data + pos));
		pos++;
		len--;		
	}
}

