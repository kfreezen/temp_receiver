#ifndef CRC16_H
#define CRC16_H

typedef struct {
	unsigned char CRC16_High, CRC16_Low;
} CRC16_Ctx;

// Thread-safe version of the CRC16_* functions
class CRC16 {
public:
	void init();

	void update(unsigned char val);
	unsigned short generate(unsigned char* msg, int len);

	unsigned char getLow();
	unsigned char getHigh();

private:
	void update4Bits(unsigned char val);

	unsigned char CRC16_High, CRC16_Low;
};

void CRC16_Init(void);
void CRC16_Update4Bits(unsigned char val);
void CRC16_Update(unsigned char val);
unsigned short CRC16_Generate(unsigned char* msg, int len);
unsigned char CRC16_GetHigh();
unsigned char CRC16_GetLow();

#endif
