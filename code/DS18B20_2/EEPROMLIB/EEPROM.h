/*
 * EEPROM.h
 *
 * Created: 25.10.2022 17:04:44
 *  Author: Евгений
 */ 


#ifndef EEPROM_H_
#define EEPROM_H_

void EEPROM_write(uint16_t uiAddress, uint8_t ucData);
uint8_t EEPROM_read(uint16_t uiAddress);
void EEPROM_read_block(uint16_t sAddress, uint16_t len, uint8_t* data);
void EEPROM_write_block(uint16_t sAddress, uint16_t len, uint8_t* data);

#endif /* EEPROM_H_ */