#include "STK500.h"
/* Read hex file from sd/micro-sd card and program
 *  another arduino via ttl serial.
 * borrows heavily from avrdude source code (GPL license)
 *   Copyright (C) 2002-2004 Brian S. Dean <bsd@bsdhome.com>
 *   Copyright (C) 2008 Joerg Wunsch
 *  Created 12/26/2011 Kevin Osborn
 */

#define DEFAULT_BAUD 115200
#ifdef __XTENSA__
	#define DEBUG_BAUD 115200
#else
	#define DEBUG_BAUD 19200
#endif

// STANDALONE_DEBUG sends error messages out the main 
// serial port. Not useful after you are actually trying to slave
// another arduino
//#define STANDALONE_DEBUG
#ifdef STANDALONE_DEBUG
	#define DEBUGPLN Serial.println
	#define DEBUGP Serial.print
#else
	#ifdef __XTENSA__
		#define DEBUGPLN Serial1.println
		#define DEBUGP Serial1.print
	#else
		SoftwareSerial *sSerial;
		#define DEBUGPLN sSerial->println
		#define DEBUGP sSerial->print
	#endif
#endif

STK500::STK500(int TxPin, int RxPin, int RstPin, int LEDPin)
{
	txPin = TxPin;
	rxPin = RxPin;
	rstPin = RstPin;
	LED1 = LEDPin;
	setup();
}

STK500::STK500(int RstPin, int LEDPin)
{
	txPin = -1;
	rxPin = -1;
	rstPin = RstPin;
	LED1 = LEDPin;
	setup();
}

#ifndef __XTENSA__
STK500::STK500(SoftwareSerial* softSer, int RstPin, int LEDPin)
{
	txPin = -1;
	rxPin = -1;
	rstPin = RstPin;
	LED1 = LEDPin;
	softSerial = softSer;
	setup();
}
#endif

// Returns true for success
bool STK500::programArduino(int baud = DEFAULT_BAUD){
  Serial.begin(baud);
  digitalWrite(rstPin,HIGH);
  
  unsigned int major=0;
  unsigned int minor=0;
  int syncCode = 0;
  //delay(100);
  //DEBUGPLN("reset");
  toggle_Reset();
  //DEBUGPLN("getsync");
  delay(500);
  //delay(100);
  syncCode = STK500_getsync();
  
  if (syncCode!=0)
  {
	  DEBUGPLN(F("Err:nosync"));
	  return false;
  }
  //DEBUGPLN("SYNCOK");
  //Serial.setTimeout(100);
  //DEBUGPLN("Sync success");
  STK500_getparm(Parm_STK_SW_MAJOR, &major);
  DEBUGP(F("s_maj: "));
  DEBUGPLN(major);
  STK500_getparm(Parm_STK_SW_MINOR, &minor);
  DEBUGP(F("s_min: "));
  DEBUGPLN(minor);
  
  if (!major && !minor)
  {
	  //DEBUGPLN("Error: no major and minor");
	  DEBUGPLN(F("Err ver"));
	  return false;
  }
  
  //return false;

  //enter program mode
  	#ifdef __XTENSA__
	yield();
	#endif
  //DEBUGPLN("program_enable");
  STK500_program_enable();
  	#ifdef __XTENSA__
	yield();
	#endif

  DEBUGP(F("Flashing..."));
  int size = 0;
  
  while (readPage(&mybuf) > 0){
	//DEBUGP("Writing page ");
	//DEBUGP(mybuf.pageaddress, HEX);
	//DEBUGP(" size ");
	//DEBUGPLN(mybuf.size);
    STK500_loadaddr(mybuf.pageaddress>>1);
	#ifdef __XTENSA__
	delay(10);
	#endif
    STK500_paged_write(&mybuf, mybuf.size, mybuf.size);
	size += mybuf.size;
	#ifdef __XTENSA__
	yield();
	#endif
  }

  #ifdef __XTENSA__
  yield();
  #endif
  //return true;
  DEBUGPLN(F("done."));
  //DEBUGP("Wrote ");
  //DEBUGP(size);
  //DEBUGPLN(" bytes to flash.");

  // could verify programming by reading back pages and comparing but for now, close out
  STK500_disable();
  //delay(100);
  toggle_Reset();
  //myFile.close();
  //blinky(4,500);
}

void STK500::setup() {
  //digitalWrite(LED1,HIGH);
  //initialize serial port. Note that it's a good idea 
  // to use the softserial library here, so you can do debugging 
  // on USB.
  #ifndef STANDALONE_DEBUG
	#ifdef __XTENSA__
	Serial1.begin(DEBUG_BAUD);
	#else
	if (softSerial != NULL) {
		sSerial = softSerial;
	}
    else if ((txPin >= 0) && (rxPin >=0) && (sSerial == NULL))
	{
		pinMode(rxPin, INPUT);
		pinMode(txPin, OUTPUT);
		sSerial = new SoftwareSerial(rxPin,txPin);
		sSerial->begin(DEBUG_BAUD);
	}
	#endif
  #endif
  mybuf.buf = &mempage[0];
  // and the regular serial port for error messages, etc.
  pinMode(rstPin,OUTPUT);
  digitalWrite(rstPin,HIGH);
  if (LED1 >= 0)
	pinMode(LED1,OUTPUT);
}

unsigned char STK500::hex2byte(unsigned char *code){
  unsigned char result =0;

  if ((code[0] >= '0') && (code[0] <='9')){
    result = ((int)code[0] - '0') << 4;
  }
  else if ((code[0] >='A') && (code[0] <= 'F')) {
    result = ((int)code[0] - 'A'+10) << 4;
  }
  if ((code[1] >= '0') && (code[1] <='9')){
    result |= ((int)code[1] - '0');
  }
  else if ((code[1] >='A') && (code[1] <= 'F'))  
    result |= ((int)code[1] -'A'+10);  
  return result;
}

void STK500::blinky(int times, long delaytime){
  if (LED1 >= 0)
  {
	for (int i = 0 ; i < times; i++){
		digitalWrite(LED1,HIGH);
		delay(delaytime);
		digitalWrite(LED1, LOW);
		delay (delaytime);
	}
  }
}

void STK500::toggle_Reset()
{
  digitalWrite(rstPin, LOW);
  delay(100);
  digitalWrite(rstPin,HIGH);
}

//original avrdude error messages get copied to ram and overflow, wo use numeric codes.
static void error1(int errno,unsigned char detail){
  DEBUGP(F("error: "));
  DEBUGP(errno);
  DEBUGP(F(" detail: 0x"));
  DEBUGPLN(detail,HEX);
}

static void error(int errno){
  DEBUGP(F("error" ));
  DEBUGPLN(errno);
}

static int STK500_send(byte *buf, unsigned int len)
{
  Serial.write(buf,len);
}

static int STK500_recv(byte * buf, unsigned int len)
{
  int rv;
  
  rv = Serial.readBytes((char *)buf,len);
  if (rv < 0) {
    error(ERRORNOPGMR);
    return -1;
  }
  
  return rv;
}

static int STK500_drain()
{
	
  byte buf[2];
  unsigned int len = 2;
  Serial.setTimeout(500);
  int rv = Serial.readBytes((char *)buf, len);
  
  if (rv > 0)
  {
	  //DEBUGPLN("drained bytes");
  }
  
  Serial.setTimeout(1000);
  //DEBUGPLN("drain");
  /*
  while (Serial.available()> 0)
  {  
    DEBUGP("draining: ");
    DEBUGPLN(Serial.read(),HEX);
  }
  */
  return 1;
}

int STK500::STK500_getsync()
{
  byte buf[8], resp[8];

  /*
   * get in sync */
  buf[0] = Cmnd_STK_GET_SYNC;
  buf[1] = Sync_CRC_EOP;
  
  /*
   * First send and drain a few times to get rid of line noise 
   */
  int tries = 30;
  bool foundSync = false;
  int bytesRecv = 0;
  resp[0] = 0;
  resp[1] = 0;
  
  /*
  while (tries > 0)
  {
	  DEBUGP("Tries left ");
	  DEBUGPLN(tries);
	  STK500_send(buf, 2);
	  Serial.setTimeout(50);
	  bytesRecv = STK500_recv(resp, 2);
	  DEBUGP("Got ");
	  DEBUGP(bytesRecv);
	  DEBUGPLN(" bytes");
	  if ((resp[0] == Resp_STK_INSYNC) &&
		  (resp[1] == Resp_STK_OK))
	  {
		foundSync = true;
		break;
	  }
	  resp[0] = 0;
	  resp[1] = 0;
	  tries--;
  }
  
  Serial.setTimeout(1000);
  
  if (!foundSync)
	  return -1;
  
  DEBUGPLN("Found sync");
  return 0;
  */
  
  //DEBUGPLN("SEND SYNC");
  STK500_send(buf, 2);
  //delay(100);
  STK500_drain();
  //delay(100);
  //DEBUGPLN("SEND SYNC");
  STK500_send(buf, 2);
  //delay(100);
  STK500_drain();
  //delay(100);
  //DEBUGPLN("SEND SYNC");
  STK500_send(buf, 2);
  //delay(100);
  
  if (STK500_recv(resp, 1) < 0)
    return -1;
  
  if (resp[0] != Resp_STK_INSYNC) {
	  DEBUGPLN("ERRB0");
    error1(ERRORPROTOSYNC,resp[0]);
    //STK500_drain();
    return -1;
  }

  if (STK500_recv(resp, 1) < 0)
    return -1;
  if (resp[0] != Resp_STK_OK) {
	  DEBUGPLN("ERRB1");
    error1(ERRORNOTOK,resp[0]);
    return -1;
  }
  return 0;
}

int STK500::STK500_getparm(unsigned parm, unsigned * value)
{
  byte buf[8];
  unsigned v;
  int tries = 0;

 retry:
  tries++;
  buf[0] = Cmnd_STK_GET_PARAMETER;
  buf[1] = parm;
  buf[2] = Sync_CRC_EOP;

  STK500_send(buf, 3);
  //delay(100);
  if (STK500_recv(buf, 1) < 0) {
	  error(ERRORNOSERIALDATA);
    //return -1;
  }
  if (buf[0] == Resp_STK_NOSYNC) {
    if (tries > 33) {
      error(ERRORNOSYNC);
	  //DEBUGPLN("Too many tries");
      return -1;
    }
    if (STK500_getsync() < 0) {
	   error(ERRORNOSERIALDATA);
      //return -1;
    }
      
    goto retry;
  }
  else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC,buf[0]);
    //return -2;
	if (tries > 33)
	{
		//DEBUGPLN("TOO MANY TRIES");
		return -1;
	}
	goto retry;
  }

  if (STK500_recv(buf, 1) < 0) {
	  error(ERRORNOSERIALDATA);
    return -1;
  }
  v = buf[0];

  if (STK500_recv(buf, 1) < 0) {
	  error(ERRORNOSERIALDATA);
    return -1;
  }
  if (buf[0] == Resp_STK_FAILED) {
    error1(ERRORPARMFAILED,v);
    return -3;
  }
  else if (buf[0] != Resp_STK_OK) {
    error1(ERRORNOTOK,buf[0]);
    return -3;
  }

  *value = v;

  return 0;
}

/* read signature bytes - arduino version */
int STK500::arduino_read_sig_bytes(AVRMEM * m)
{
  unsigned char buf[8];

  /* Signature byte reads are always 3 bytes. */

  if (m->size < 3) {
    //DEBUGPLN("memsize too small for sig byte read");
    return -1;
  }

  buf[0] = Cmnd_STK_READ_SIGN;
  buf[1] = Sync_CRC_EOP;

  STK500_send(buf, 2);

  if (STK500_recv(buf, 5) < 0)
    return -1;
  if (buf[0] == Resp_STK_NOSYNC) {
    error(ERRORNOSYNC);
	return -1;
  } else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC,buf[0]);
	return -2;
  }
  if (buf[4] != Resp_STK_OK) {
    error1(ERRORNOTOK,buf[4]);
    return -3;
  }

  m->buf[0] = buf[1];
  m->buf[1] = buf[2];
  m->buf[2] = buf[3];

  return 3;
}

int STK500::STK500_loadaddr(unsigned int addr)
{
  unsigned char buf[8];
  int tries;

  tries = 0;
 retry:
  tries++;
  buf[0] = Cmnd_STK_LOAD_ADDRESS;
  buf[1] = addr & 0xff;
  buf[2] = (addr >> 8) & 0xff;
  buf[3] = Sync_CRC_EOP;


  STK500_send(buf, 4);

  if (STK500_recv(buf, 1) < 0)
    return -1;
  if (buf[0] == Resp_STK_NOSYNC) {
    if (tries > 33) {
      error(ERRORNOSYNC);
      return -1;
    }
    if (STK500_getsync() < 0)
      return -1;
    goto retry;
  }
  else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC, buf[0]);
    return -1;
  }

  if (STK500_recv(buf, 1) < 0)
    return -1;
  if (buf[0] == Resp_STK_OK) {
    return 0;
  }

  error1(ERRORPROTOSYNC, buf[0]);
  return -1;
}

int STK500::STK500_paged_write(AVRMEM * m, int page_size, int n_bytes)
{
  // This code from avrdude has the luxury of living on a PC and copying buffers around.
  // not for us...
 // unsigned char buf[page_size + 16];
 unsigned char cmd_buf[16]; //just the header
  int memtype;
 // unsigned int addr;
  int block_size;
  int tries;
  unsigned int n;
  unsigned int i;
  int flash;

  // Fix page size to 128 because that's what arduino expects
  page_size = 128;
  //EEPROM isn't supported
  memtype = 'F';
  flash = 1;


    /* build command block and send data separeately on arduino*/
    
    i = 0;
    cmd_buf[i++] = Cmnd_STK_PROG_PAGE;
    cmd_buf[i++] = (page_size >> 8) & 0xff;
    cmd_buf[i++] = page_size & 0xff;
    cmd_buf[i++] = memtype;
    STK500_send(cmd_buf,4);
    STK500_send(&m->buf[0], page_size);
    cmd_buf[0] = Sync_CRC_EOP;
    STK500_send( cmd_buf, 1);

    if (STK500_recv(cmd_buf, 1) < 0)
      return -1; // errr need to fix this... 
    if (cmd_buf[0] == Resp_STK_NOSYNC) {
        error(ERRORNOSYNC);
        return -3;
     }
    else if (cmd_buf[0] != Resp_STK_INSYNC) {

     error1(ERRORPROTOSYNC, cmd_buf[0]);
      return -4;
    }
    
    if (STK500_recv(cmd_buf, 1) < 0)
      return -1;
    if (cmd_buf[0] != Resp_STK_OK) {
    error1(ERRORNOTOK,cmd_buf[0]);

      return -5;
    }
  

  return n_bytes;
}

#ifdef LOADVERIFY //maybe sometime? note code needs to be re-written won't work as is
static int STK500_paged_load(AVRMEM * m, int page_size, int n_bytes)
{
  unsigned char buf[16];
  int memtype;
  unsigned int addr;
  int a_div;
  int tries;
  unsigned int n;
  int block_size;

  memtype = 'F';


  a_div = 1;

  if (n_bytes > m->size) {
    n_bytes = m->size;
    n = m->size;
  }
  else {
    if ((n_bytes % page_size) != 0) {
      n = n_bytes + page_size - (n_bytes % page_size);
    }
    else {
      n = n_bytes;
    }
  }

  for (addr = 0; addr < n; addr += page_size) {
//    report_progress (addr, n_bytes, NULL);

    if ((addr + page_size > n_bytes)) {
	   block_size = n_bytes % page_size;
	}
	else {
	   block_size = page_size;
	}
  
    tries = 0;
  retry:
    tries++;
    STK500_loadaddr(addr/a_div);
    buf[0] = Cmnd_STK_READ_PAGE;
    buf[1] = (block_size >> 8) & 0xff;
    buf[2] = block_size & 0xff;
    buf[3] = memtype;
    buf[4] = Sync_CRC_EOP;
    STK500_send(buf, 5);

    if (STK500_recv(buf, 1) < 0)
      return -1;
    if (buf[0] == Resp_STK_NOSYNC) {
      if (tries > 33) {
        error(ERRORNOSYNC);
        return -3;
      }
      if (STK500_getsync() < 0)
	return -1;
      goto retry;
    }
    else if (buf[0] != Resp_STK_INSYNC) {
      error1(ERRORPROTOSYNC, buf[0]);
      return -4;
    }

    if (STK500_recv(&m->buf[addr], block_size) < 0)
      return -1;

    if (STK500_recv(buf, 1) < 0)
      return -1;

    if (buf[0] != Resp_STK_OK) {
        error1(ERRORPROTOSYNC, buf[0]);
        return -5;
      }
    }
  

  return n_bytes;
}
#endif

/*
 * issue the 'program enable' command to the AVR device
 */
int STK500::STK500_program_enable()
{
  unsigned char buf[8];
  int tries=0;

 retry:
  
  tries++;

  buf[0] = Cmnd_STK_ENTER_PROGMODE;
  buf[1] = Sync_CRC_EOP;

  STK500_send( buf, 2);
  if (STK500_recv( buf, 1) < 0)
    return -1;
  if (buf[0] == Resp_STK_NOSYNC) {
    if (tries > 33) {
      error(ERRORNOSYNC);
      return -1;
    }
    if (STK500_getsync()< 0)
      return -1;
    goto retry;
  }
  else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC,buf[0]);
    return -1;
  }

  if (STK500_recv( buf, 1) < 0)
    return -1;
  if (buf[0] == Resp_STK_OK) {
    return 0;
  }
  else if (buf[0] == Resp_STK_NODEVICE) {
    error(ERRORNODEVICE);
    return -1;
  }

  if(buf[0] == Resp_STK_FAILED)
  {
      error(ERRORNOPROGMODE);
	  return -1;
  }


  error1(ERRORUNKNOWNRESP,buf[0]);

  return -1;
}

void STK500::STK500_disable()
{
  unsigned char buf[8];
  int tries=0;

 retry:
  
  tries++;

  buf[0] = Cmnd_STK_LEAVE_PROGMODE;
  buf[1] = Sync_CRC_EOP;

  STK500_send( buf, 2);
  if (STK500_recv( buf, 1) < 0)
    return;
  if (buf[0] == Resp_STK_NOSYNC) {
    if (tries > 33) {
      error(ERRORNOSYNC);
      return;
    }
    if (STK500_getsync() < 0)
      return;
    goto retry;
  }
  else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC,buf[0]);
    return;
  }

  if (STK500_recv( buf, 1) < 0)
    return;
  if (buf[0] == Resp_STK_OK) {
    return;
  }
  else if (buf[0] == Resp_STK_NODEVICE) {
    error(ERRORNODEVICE);
    return;
  }

  error1(ERRORUNKNOWNRESP,buf[0]);

  return;
}

void dumphex(unsigned char *buf,int len)
{
  for (int i = 0; i < len; i++)
  {
    if (i%16 == 0)
      DEBUGPLN();
    DEBUGP(buf[i],HEX);
	DEBUGP(" ");
  }
  DEBUGPLN();
}
