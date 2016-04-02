#ifndef STK500_SDFFS_H
#define STK500_SDFFS_H

#include "STK500.h"

#ifdef __XTENSA__
#include "FS.h"
#else
#include <SD.h>
#endif

class STK500_SDFFS : public STK500
{
	public:
		STK500_SDFFS(int TxPin, int RxPin, int RstPin, int LEDPin) : STK500(TxPin, RxPin, RstPin, LEDPin) {};
		STK500_SDFFS(int RstPin, int LEDPin) : STK500(RstPin, LEDPin) {};
		bool ProgramArduino(File input, int baud);
		
	protected:
		int readPage(avrmem *buf);
		int readIntelHexLine(File input, int *address, unsigned char *buf);
		
		File *myFile;
};

#endif