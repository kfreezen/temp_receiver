#ifndef CRC16_H
#define CRC16_H

void CRC16_Init(void);
void CRC16_Update4Bits(unsigned char val);
void CRC16_Update(unsigned char val);
unsigned short CRC16_Generate(unsigned char* msg, int len);
unsigned char CRC16_GetHigh();
unsigned char CRC16_GetLow();

#endif
