/************************************************************************/
/*    DMASerial.h                                                       */
/*    Hardware Serial Library that uses DMA                             */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    11/23/2016 (KeithV): Created                                      */
/************************************************************************/

#ifndef DMASerial_h
#define DMASerial_h

#ifdef __cplusplus

#include <inttypes.h>
#include <p32xxxx.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//* ------------------------------------------------------------
//*                          READ THIS
//*     
//*  WARNING: THIS LIBRARY WILL ONLY WORK WITH UARTS THAT HAVE AN 8 BYTE DEEP FIFO   
//*     
//* The DMASerialOBJ MACRO will make it easy for you to create your   
//* Serial object instance AND the ISR routine    
//* You can use this macro at the global scope of a module    
//*     
//*     
//* DMASerialOBJ(obj, buff, buffSize, uart, dma, pri);  
//*     Where:    
//*         obj:        The name of the instance of the object to be declared   
//*         buff:       User supplied ring buffer for Rx data to be put
//*         buffSize:   The size of the ring buffer
//*         uart:       The uart number to use, NOT THE UART BASE. i.e. "4" for UART4
//*         dma:        The DMA channel to use, NOT THE DMA BASE. i.e. "2" for DMA 2
//*         pri:        The priority to run the DMA ISR at (1 - 7).
//*     
//*     For PPS:      
//*         You must set up the PPS registers in advance
//*     
//*     
//* As an example to create an instance of a DMASerial object with an instance name of SerialDMA using uart 5 dma 0 with a dma isr priority of 4  
//*     DMASerialOBJ(SerialDMA, uartBuff, sizeof(uartBuff), 5, 0, 4);
//*     
//* ------------------------------------------------------------
#define DMASerialOBJ(obj, buff, buffSize, uart, dma, pri) DMASerialOBJ2(obj, buff, buffSize, uart, dma, pri)

//* ------------------------------------------------------------
//* You MUST declare an ISR in your code
//* This is a helper Macro to declare the ISR    
//* If you use the DMASerialOBJ, this macro is automatically called   
//*     
//* DMASerialISR2(obj, vec, ipl)    
//*     Where:    
//*         obj is the name of the instance of your object.    
//*         vec is the DMA vector number  
//*         ipl is the priority you want to run the ISR, this needs to be the same as specified on the class constructor
//*     
//* As an example, if your object instance declaration was:   
//*     DMASerial SerialDMA(uartBuff, sizeof(uartBuff), &U4MODE, _UART4_RX_VECTOR, &DCH0CON, _DMA0_VECTOR);   
//*     
//* You could declare your ISR as    
//*     DMASerialISR(SerialDMA, _DMA0_VECTOR, 4);
//*     
//* ------------------------------------------------------------
#define DMASerialISR(obj, vec, ipl) DMASerialISR2(obj, vec, ipl)

// the actual implementation of the helper macros
#if defined(_UART1_RX_VECTOR) // _DMAx_VECTOR _UARTx_RX_VECTOR
    #define DMASerialOBJ2(obj, buff, buffSize, uart, dma, pri) \
        DMASerial obj(buff, buffSize, &U##uart##MODE, _UART##uart##_RX_VECTOR, &DCH##dma##CON, _DMA##dma##_VECTOR, pri); \
        DMASerialISR(obj, _DMA##dma##_VECTOR, pri)
#else // _DMAx_IRQ _DMA_x_VECTOR _UARTx_RX_IRQ 
    #define DMASerialOBJ2(obj, buff, buffSize, uart, dma, pri) \
        DMASerial obj(buff, buffSize, &U##uart##MODE, _UART##uart##_RX_IRQ, &DCH##dma##CON, _DMA##dma##_IRQ, pri); \
        DMASerialISR(obj, _DMA_##dma##_VECTOR, pri)
#endif

#if defined(OFF000)     // offset register
    #define DMASerialISR2(obj, vec, ipl) void __attribute__((nomips16, at_vector(vec),interrupt(IPL##ipl##SRS))) ISR_VECTOR_##vec(void) { obj.isr(); }
#else
    #define DMASerialISR2(obj, vec, ipl) void __attribute__((nomips16, vector(vec),interrupt(IPL##ipl##SOFT))) ISR_VECTOR_##vec(void) { obj.isr(); }
#endif

//* ------------------------------------------------------------
//* 		Object Class Declarations
//* ------------------------------------------------------------

class DMASerial
{
	protected:

        typedef struct _UART
        {
            volatile __U1MODEbits_t mode;
            volatile uint32_t       modeClr;
            volatile uint32_t       modeSet;
            volatile uint32_t       modeInv;
            volatile __U1STAbits_t  sta;
            volatile uint32_t       staClr;
            volatile uint32_t       staSet;
            volatile uint32_t       staInv;
            volatile uint32_t       txReg;
            uint32_t                txPad[3];
            volatile uint32_t       rxReg;
            uint32_t                rxPad[3];
            volatile uint32_t       brg;
            volatile uint32_t       brgClr;
            volatile uint32_t       brgSet;
            volatile uint32_t       brgInv;
        } UART;

        typedef struct _DMA
        {
            volatile __DCH0CONbits_t con;
            volatile uint32_t conClr;
            volatile uint32_t conSet;
            volatile uint32_t conInv;

            volatile __DCH0ECONbits_t econ;
            volatile uint32_t econClr;
            volatile uint32_t econSet;
            volatile uint32_t econInv;

            volatile __DCH0INTbits_t intr;
            volatile uint32_t intrClr;
            volatile uint32_t intrSet;
            volatile uint32_t intrInv;

            volatile uint32_t ssa;
            volatile uint32_t ssaClr;
            volatile uint32_t ssaSet;
            volatile uint32_t ssaInv;

            volatile uint32_t dsa;
            volatile uint32_t dsaClr;
            volatile uint32_t dsaSet;
            volatile uint32_t dsaInv;

            volatile uint32_t ssiz;
            volatile uint32_t ssizClr;
            volatile uint32_t ssizSet;
            volatile uint32_t ssizInv;

            volatile uint32_t dsiz;
            volatile uint32_t dsizClr;
            volatile uint32_t dsizSet;
            volatile uint32_t dsizInv;

            volatile uint32_t sptr;
            volatile uint32_t sptrClr;
            volatile uint32_t sptrSet;
            volatile uint32_t sptrInv;

            volatile uint32_t dptr;
            volatile uint32_t dptrClr;
            volatile uint32_t dptrSet;
            volatile uint32_t dptrInv;

            volatile uint32_t csiz;
            volatile uint32_t csizClr;
            volatile uint32_t csizSet;
            volatile uint32_t csizInv;

            volatile uint32_t cptr;
            volatile uint32_t cptrClr;
            volatile uint32_t cptrSet;
            volatile uint32_t cptrInv;

            volatile uint32_t dat;
            volatile uint32_t datClr;
            volatile uint32_t datSet;
            volatile uint32_t datInv;
        } DMA;

        typedef union _IPC {
          struct {
            unsigned subPriority:2;
            unsigned Priority:3;
            unsigned :3;
          };
          uint8_t b;
        } __attribute__((packed)) IPC;


        // Class variables
		volatile UART&          uart;		// uart registers
        volatile DMA&           dma;        // dma registers
        uint8_t * const         pBuff;      // the ring buffer
        uint32_t  const         cbBuff;     // the size of the ring buffer

        volatile DMA *          pdmaTx;     // pointer to a Tx DMA channel

        uint16_t                iRead;      // next index in the ring buffer to read
        uint8_t const           irqRx;      // what is the IRQ to tie the DMA to
        uint8_t const           irqDMA;     // DMA IRQ
        uint8_t const           priDMA;     // DMA ISR priority

        volatile uint32_t&      IEDMA;      // DMA IE register
        volatile uint32_t&      IEDMAClr;   // DMA IE register
        volatile uint32_t&      IEDMASet;   // DMA IE register
        volatile uint32_t&      IFDMA;      // DMA IF register
        volatile uint32_t&      IFDMAClr;   // DMA IF register
        volatile uint32_t&      IFDMASet;   // DMA IF register
        uint32_t const          IEIFDMAMask; // the IE or IF DMA mask 
        volatile IPC&           IPCDMA;     // IPC register for the DMA interrupt

    //**************************************************************************
    //  These are common macros that may not exist.                           **
    //  Because I want this to compile with minimal external headers          **
    //  I provide these common macros as inline methods                       **
    //**************************************************************************

        int inline _min(int a, int b)
        {
            return(a < b ? a : b);
        }

        uint32_t inline _KVA2PA(void * v)
        {
            return(((uint32_t) (v)) & 0x1fffffff);
        }

        uint32_t  inline _KVA2KSEG0(void * v)
        {
            return(_KVA2PA(v) | 0x80000000);
        }

        uint32_t inline _KVA2KSEG1(void * v)
        {
            return(((uint32_t) (v)) | 0xA0000000);
        }

        bool inline _KVA_IS_RAM(void * v)
        {
            return((((uint32_t) (v)) & 0x10000000) == 0);
        }

        uint32_t inline __attribute__((nomips16))  _enableInterrupts(void)
        {
            uint32_t status = 0;
            asm volatile("ei    %0" : "=r"(status));
            return status;
        }

        uint32_t inline __attribute__((nomips16)) _disableInterrupts(void)
        {
            uint32_t status = 0;
            asm volatile("di    %0" : "=r"(status));
            return status;
        }

        void inline __attribute__((nomips16))  _restoreInterrupts(uint32_t st)
        {
            if (st & 0x00000001) asm volatile("ei");
            else asm volatile("di");
        }

    //**************************************************************************
    //  End of common macros                                                  **
    //**************************************************************************

	public:

    //**************************************************************************
    //  Constructors                                                          **
    //**************************************************************************
	DMASerial(uint8_t * const pBuffP,  uint32_t const cbBuffP, volatile void * pUART, uint8_t const irqRxP, volatile void * pDMA, uint8_t const irqDMAP, uint8_t const priDMAP) :
        uart(*((UART *) pUART)), dma(*((DMA *) pDMA)), pBuff((uint8_t * const) _KVA2KSEG1(pBuffP)), cbBuff(cbBuffP), pdmaTx(NULL),
        iRead(0), irqRx(irqRxP), irqDMA(irqDMAP), priDMA(priDMAP),
        IEDMA(*((uint32_t *) (&IEC0 + 4*(irqDMAP / 32)))), IEDMAClr(*(&IEDMA+1)), IEDMASet(*(&IEDMA+2)), 
        IFDMA(*((uint32_t *) (&IFS0 + 4*(irqDMAP / 32)))), IFDMAClr(*(&IFDMA+1)), IFDMASet(*(&IFDMA+2)),
        IEIFDMAMask((1ul << (irqDMAP % 32))),
        IPCDMA(*((volatile IPC *) (((uint8_t *) (&IPC0 + 4*(irqDMAP / 4))) + (irqDMAP % 4))))
        {
        }

    //**************************************************************************
    //  Public Methods                                                        **
    //**************************************************************************

        int	            begin(unsigned long baudRate) {return(begin(baudRate, 0, false));}
        int             begin(unsigned long baudRate, uint8_t address, uint8_t fAddress = true);
        void inline     end()
        {
            dma.con.CHEN    = 0;    // turn off the controller
	        uart.mode.w =	0;
        }

        // DMA block read methodes
		int		        readBuffer(uint8_t * pBuff, uint32_t cbBuff);
        int inline      isDMARxDone(void)
        {
            return(!dma.con.CHAEN);
        }
        void inline     abortDMARx(void)
        {
            if(!isDMARxDone()) isr();
        }

        // DMA block write methodes
        int             writeBuffer(uint8_t * pBuf, uint32_t cbBuf, volatile void * pDMA);
        int inline      isDMATxDone(void)
        {
            return(pdmaTx == NULL || !pdmaTx->con.CHEN);
        }
        void inline     abortDMATx(void)
        {
            if(!isDMATxDone()) pdmaTx->con.CHEN = 0;
        }

        // stream read methods
        virtual int     available(void);
        virtual int     peek(void);
		virtual int     read(void);
        virtual void    purge(void);

        // stream write methods
		virtual	size_t	write(uint8_t);
        size_t write(const uint8_t *buffer, size_t size)
        {
            size_t i;
            for(i=0; i<size; i++) write(buffer[i]);
            return(size);
        }

        virtual void    flush(void);


        // print methods
        //virtual size_t print(const char sz[]) { return(write((const uint8_t *) sz, strlen(sz))); }
        //virtual size_t print(char ch) { return(write((uint8_t) ch)); }

        // print methods
        virtual size_t print(const char sz[]);
        virtual size_t print(char ch);

        // signed ints
        size_t print(long value, int radix = 10)
        {
            char sz[32];
            itoa(value, sz, radix);
            return(print(sz));
        }
        size_t print(int value, int radix = 10) {return(print((long) value, radix));}

        // unsigned ints
        size_t print(unsigned long value, int radix = 10)
        {
            char sz[32];
            utoa(value, sz, radix);
            return(print(sz));
        }
        size_t print(unsigned char value, int radix = 10) { return(print((unsigned long) value, radix));}
        size_t print(unsigned int value, int radix = 10) { return(print((unsigned long) value, radix));}

        // println()
        size_t println(void) {return(print("\r\n"));}
        size_t println(const char sz[]) {size_t cb = print(sz)+2; print("\r\n"); return(cb);}
        size_t println(char ch) {size_t cb = print(ch)+2; print("\r\n"); return(cb);}
        size_t println(unsigned char value, int radix = 10) {size_t cb = print(value, radix)+2; print("\r\n"); return(cb);}
        size_t println(int value, int radix = 10) {size_t cb = print(value, radix)+2; print("\r\n"); return(cb);}
        size_t println(unsigned int value, int radix = 10) {size_t cb = print(value, radix)+2; print("\r\n"); return(cb);}
        size_t println(long value, int radix = 10) {size_t cb = print(value, radix)+2; print("\r\n"); return(cb);}
        size_t println(unsigned long value, int radix = 10) {size_t cb = print(value, radix)+2; print("\r\n"); return(cb);}

        operator int() {
            return 1;
        }

        void inline enableAddressDetection(void) {
            uart.staSet = _U1STA_ADDEN_MASK;
        }

        void inline disableAddressDetection(void) {
            uart.staClr = _U1STA_ADDEN_MASK;;
        }

        // the ISR routine to call on this instance
        void            isr(void);
};

#endif	//	__cplusplus
#endif  //	DMASerial_h
