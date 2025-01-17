/*-----------------------------------------------------------------------------
 * Hardware-dependent code for usb_jtag
 *-----------------------------------------------------------------------------
 * Copyright (C) 2007 Kolja Waschk, ixo.de
 *-----------------------------------------------------------------------------
 * This code is part of usbjtag. usbjtag is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version. usbjtag is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.  You should have received a
 * copy of the GNU General Public License along with this program in the file
 * COPYING; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * St, Fifth Floor, Boston, MA  02110-1301  USA
 *-----------------------------------------------------------------------------
 */

#include <fx2regs.h>
#include "hardware.h"
#include "delay.h"

// Hardware model: Geeetech CY7C68013A dev.board, https://sigrok.org/wiki/Lcsoft_Mini_Board

//-----------------------------------------------------------------------------
// comment out (undefine!) if you don't want PS, AS or OE signals

#define HAVE_PS_MODE 1
#define HAVE_AS_MODE 1
#define HAVE_OE_LED  1

// comment in (define!) if you want outputs disabled when possible
//#define HAVE_OENABLE 1

//-----------------------------------------------------------------------------

/* JTAG TCK, AS/PS DCLK */

sbit at 0x83          TCK; /* Port A.3 */
#define bmTCKOE       bmBIT3
#define SetTCK(x)     do{TCK=(x);}while(0)

/* JTAG TDI, AS ASDI, PS DATA0 */

sbit at 0x85          TDI; /* Port A.5 */
#define bmTDIOE       bmBIT5
#define SetTDI(x)     do{TDI=(x);}while(0)

/* JTAG TMS, AS/PS nCONFIG */

sbit at 0x87          TMS; /* Port A.7 */
#define bmTMSOE       bmBIT7
#define SetTMS(x)     do{TMS=(x);}while(0)

/* JTAG TDO, AS/PS CONF_DONE */

sbit at 0x86          TDO; /* Port A.6 */
#define bmTDOOE       bmBIT6
#define GetTDO(x)     TDO

/* JTAG ENABLE */
sbit JTAG_EN = 0x80; /* Port A.0 */
#define bmJTAG_EN bmBIT0

//-----------------------------------------------------------------------------

#if defined(HAVE_PS_MODE) || defined(HAVE_AS_MODE)

  /* AS DATAOUT, PS nSTATUS */

  sbit at 0x84        ASDO; /* Port A.4 */
  #define bmASDOOE    bmBIT4
  #define GetASDO(x)  ASDO

#else

  #define bmASDOOE    0
  #define GetASDO(x)  1

#endif

//-----------------------------------------------------------------------------

#if defined(HAVE_AS_MODE)

  /* AS Mode nCS */

  sbit at 0x82        NCS; /* Port A.2 */
  #define bmNCSOE     bmBIT2
  #define SetNCS(x)   do{NCS=(x);}while(0)
  #define GetNCS(x)   NCS

  /* AS Mode nCE */

  sbit at 0x81        NCE; /* Port A.1 */
  #define bmNCEOE     bmBIT1
  #define SetNCE(x)   do{NCE=(x);}while(0)

  unsigned char ProgIO_ShiftInOut_AS(unsigned char x);

#else

  #define bmNCSOE     0
  #define SetNCS(x)   while(0){}
  #define GetNCS(x)   1
  #define bmNCEOE     0
  #define SetNCE(x)   while(0){}

  #define ProgIO_ShiftInOut_AS(x) ProgIO_ShiftInOut(x)

#endif

//-----------------------------------------------------------------------------

#ifdef HAVE_OE_LED

  sbit at 0x87        OELED; /* Port A.0 */
  #define bmOELEDOE   bmBIT0
  #define SetOELED(x) do{OELED=(x);}while(0)

#else

  #define bmOELEDOE   0
  #define SetOELED(x) while(0){}

#endif

//-----------------------------------------------------------------------------

#define bmPROGOUTOE (bmTCKOE|bmTDIOE|bmTMSOE|bmNCEOE|bmNCSOE|bmOELEDOE)
#define bmPROGINOE  (bmTDOOE|bmASDOOE)

//-----------------------------------------------------------------------------

void ProgIO_Poll(void)    {}
// These aren't called anywhere in usbjtag.c, but I plan to do so...
void ProgIO_Enable(void)  {}
void ProgIO_Disable(void) {}
void ProgIO_Deinit(void)  {}


void ProgIO_Init(void)
{
  /* The following code depends on your actual circuit design.
     Make required changes _before_ you try the code! */
  
  // set the CPU clock to 48MHz, enable clock output to FPGA
  CPUCS = bmCLKOE | bmCLKSPD1;

   // put the system in FIFO mode by default
   // internal clock source at 48Mhz, drive output pin, synchronous mode
   // NOTE: Altera USB-Blaster does not work in another mode
   IFCONFIG =  bmIFCLKSRC | bm3048MHZ | bmIFCLKOE;
   IFCONFIG |= bmASYNC | bmIFCFG1 | bmIFCFG0;

   // set Port A output enable (so we can handle the JTAG enable signal)
   OEA = (1 << 7);

   // set port E output enable (actually only PE6 is used to enable/disable the 1.2V regulator, but for strange reasons we also have to enable PE1-5)
   OEE = 0x7F;

   // disconnect the 1.2V converter (in the configuration process, we can not draw
   // more than 100ma)
   IOE = (1 << 6);

   // activate JTAG outputs on Port A
   OEA = bmTDIOE | bmTCKOE | bmTMSOE | bmJTAG_EN;
}

void ProgIO_Set_State(unsigned char d)
{
  /* Set state of output pins:
   *
   * d.0 => TCK
   * d.1 => TMS
   * d.2 => nCE (only #ifdef HAVE_AS_MODE)
   * d.3 => nCS (only #ifdef HAVE_AS_MODE)
   * d.4 => TDI
   * d.5 => LED / Output Enable
   */

  SetTCK((d & bmBIT0) ? 1 : 0);
  SetTMS((d & bmBIT1) ? 1 : 0);
#ifdef HAVE_AS_MODE
  SetNCE((d & bmBIT2) ? 1 : 0);
  SetNCS((d & bmBIT3) ? 1 : 0);
#endif
  SetTDI((d & bmBIT4) ? 1 : 0);
#ifdef HAVE_OE_LED
  SetOELED((d & bmBIT5) ? 1 : 0);
#endif
}

unsigned char ProgIO_Set_Get_State(unsigned char d)
{
  /* Set state of output pins (s.a.)
   * then read state of input pins:
   *
   * TDO => d.0
   * DATAOUT => d.1 (only #ifdef HAVE_AS_MODE)
   */

  ProgIO_Set_State(d);
   return (GetASDO()<<1)|GetTDO();
}

//-----------------------------------------------------------------------------

void ProgIO_ShiftOut(unsigned char c)
{
  /* Shift out byte C: 
   *
   * 8x {
   *   Output least significant bit on TDI
   *   Raise TCK
   *   Shift c right
   *   Lower TCK
   * }
   */
 
  (void)c; /* argument passed in DPL */

  _asm
        MOV  A,DPL
        ;; Bit0
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        ;; Bit1
        RRC  A
        CLR  _TCK
        MOV  _TDI,C
        SETB _TCK
        ;; Bit2
        RRC  A
        CLR  _TCK
        MOV  _TDI,C
        SETB _TCK
        ;; Bit3
        RRC  A
        CLR  _TCK
        MOV  _TDI,C
        SETB _TCK
        ;; Bit4
        RRC  A
        CLR  _TCK
        MOV  _TDI,C
        SETB _TCK
        ;; Bit5
        RRC  A
        CLR  _TCK
        MOV  _TDI,C
        SETB _TCK
        ;; Bit6
        RRC  A
        CLR  _TCK
        MOV  _TDI,C
        SETB _TCK
        ;; Bit7
        RRC  A
        CLR  _TCK
        MOV  _TDI,C
        SETB _TCK
        NOP 
        CLR  _TCK
        ret
  _endasm;
}

/*
;; For ShiftInOut, the timing is a little more
;; critical because we have to read _TDO/shift/set _TDI
;; when _TCK is low. But 20% duty cycle at 48/4/5 MHz
;; is just like 50% at 6 Mhz, and that's still acceptable
*/

#if HAVE_AS_MODE

unsigned char ProgIO_ShiftInOut_JTAG(unsigned char c);
unsigned char ProgIO_ShiftInOut_AS(unsigned char c);

unsigned char ProgIO_ShiftInOut(unsigned char c)
{
  if(GetNCS(x)) return ProgIO_ShiftInOut_JTAG(c);
  return ProgIO_ShiftInOut_AS(c);
}

#else /* HAVE_AS_MODE */

#define ProgIO_ShiftInOut_JTAG(x) ProgIO_ShiftInOut(x)

#endif

unsigned char ProgIO_ShiftInOut_JTAG(unsigned char c)
{
  /* Shift out byte C, shift in from TDO:
   *
   * 8x {
   *   Read carry from TDO
   *   Output least significant bit on TDI
   *   Raise TCK
   *   Shift c right, append carry (TDO) at left
   *   Lower TCK
   * }
   * Return c.
   */

   (void)c; /* argument passed in DPL */

  _asm
        MOV  A,DPL

        ;; Bit0
        MOV  C,_TDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit1
        MOV  C,_TDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit2
        MOV  C,_TDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit3
        MOV  C,_TDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit4
        MOV  C,_TDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit5
        MOV  C,_TDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit6
        MOV  C,_TDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit7
        MOV  C,_TDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK

        MOV  DPL,A
        ret
  _endasm;

  /* return value in DPL */

  return c;
}

#ifdef HAVE_AS_MODE

unsigned char ProgIO_ShiftInOut_AS(unsigned char c)
{
  /* Shift out byte C, shift in from TDO:
   *
   * 8x {
   *   Read carry from TDO
   *   Output least significant bit on TDI
   *   Raise TCK
   *   Shift c right, append carry (TDO) at left
   *   Lower TCK
   * }
   * Return c.
   */

  (void)c; /* argument passed in DPL */

  _asm
        MOV  A,DPL

        ;; Bit0
        MOV  C,_ASDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit1
        MOV  C,_ASDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit2
        MOV  C,_ASDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit3
        MOV  C,_ASDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit4
        MOV  C,_ASDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit5
        MOV  C,_ASDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit6
        MOV  C,_ASDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK
        ;; Bit7
        MOV  C,_ASDO
        RRC  A
        MOV  _TDI,C
        SETB _TCK
        CLR  _TCK

        MOV  DPL,A
        ret
  _endasm;
  return c;
}

#endif /* HAVE_AS_MODE */


