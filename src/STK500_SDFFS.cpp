#include "STK500_SDFFS.h"

bool STK500_SDFFS::ProgramArduino(File input, int baud)
{
	myFile = &input;
	return programArduino(baud);
	myFile = NULL;
}

int STK500_SDFFS::readPage(avrmem *buf)
{
  int len;
  int address;
  int total_len = 0;
  unsigned char linemembuffer[16];
  bool first = true;
  // grab 128 bytes or less (one page)
  while (total_len < (128 - 15)) {
  //for (int i=0 ; i < 8; i++){
    len = readIntelHexLine(*myFile, &address, &linemembuffer[0]);
	//DEBUGP("addr ");
	//DEBUGP(address, HEX);
	//DEBUGP(" len ");
	//DEBUGPLN(len);
    if (len < 0) {
      break;
	}
    else if (len == 0) {
		
	}
    else {
		if (first)// first record determines the page address
		{
		  buf->pageaddress = address;
		  first = false;
		}
		memcpy((buf->buf)+(total_len), linemembuffer, len);
		total_len += len;
	}
  }
  buf->size = total_len;
  return total_len;
}

// read one line of intel hex from file. Return the number of databytes
// Since the arduino code is always sequential, ignore the address for now.
// If you want to burn bootloaders, etc. we'll have to modify to 
// return the address.

// INTEL HEX FORMAT:
// :<8-bit record size><16bit address><8bit record type><data...><8bit checksum>
int STK500_SDFFS::readIntelHexLine(File input, int *address, unsigned char *buf){
  unsigned char c;
  int i=0;
  linebuffer[0] = 0;
  while (true){
    if (input.available()){
      c = input.read();
      // this should handle unix or ms-dos line endings.
      // break out when you reach either, then check
      // for lf in stream to discard
      if ((c == 0x0d)|| (c == 0x0a))
        break;
      else
        linebuffer[i++] =c;
    }
    else return -1; //end of file
  }
  linebuffer[i]= 0; // terminate the string
  //peek at the next byte and discard if line ending char.
  if (input.peek() == 0xa)
    input.read();
  
  if (linebuffer[0] == ':')
  {
	  int len = hex2byte(&linebuffer[1]);
	  *address = (hex2byte(&linebuffer[3]) <<8) |
				   (hex2byte(&linebuffer[5]));
	  int j=0;
	  for (int i = 9; i < ((len*2)+9); i +=2){
		buf[j] = hex2byte(&linebuffer[i]);
		j++;
	  }
	  return len;
  }
  else
  {
    return 0;
  }
}