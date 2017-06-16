/************************************************************************/
/*																		*/
/*	DSPI.h	--	Declarations for Digilent SPI Library        			*/
/*																		*/
/************************************************************************/
/*	Author:		Gene Apperson											*/
/*	Copyright (c) 2011, Digilent Inc. All rights reserved.				*/
/************************************************************************/
/*  Revision History:													*/
/*																		*/
/*	10/28/2011(GeneApperson): Created									*/
/*	05/27/2013(ClaudiaGoga): added PPS support for PIC32MX1 and PIC32MX2*/
/*	12/14/2016(Keith Vogel): Digilent owned relicensing                 */
/*	12/14/2016(Keith Vogel): Modified for the OpenScope                 */
/*																		*/
/************************************************************************/

#if !defined(_DSPI_H_)
#define	_DSPI_H_

#ifdef __cplusplus

#include <p32xxxx.h>
#include <inttypes.h>

/* ------------------------------------------------------------ */
/*					Miscellaneous Declarations					*/
/* ------------------------------------------------------------ */

#define	DSPI_MODE0	(_SPI2CON_CKE_MASK)                     // CKP = 0 CKE = 1
#define	DSPI_MODE1	(0)                                     // CKP = 0 CKE = 0
#define	DSPI_MODE2	(_SPI2CON_CKP_MASK |_SPI2CON_CKE_MASK)  // CKP = 1 CKE = 1
#define	DSPI_MODE3	(_SPI2CON_CKP_MASK)                     // CKP = 1 CKE = 0

#define DSPI_8BIT	8
#define DSPI_16BIT	16
#define DSPI_32BIT	32

//#define	_DSPI_SPD_DEFAULT	1000000L
#define	_DSPI_SPD_DEFAULT	500000L


//* ------------------------------------------------------------
//*                          READ THIS
//*     
//*  WARNING: THIS LIBRARY WILL ONLY WORK WITH UARTS THAT HAVE AN 8 BYTE DEEP FIFO   
//*     
//* The DSPIOBJ MACRO will make it easy for you to create your   
//* Serial object instance AND the ISR routine    
//* You can use this macro at the global scope of a module    
//*     
//*     
//* DSPIOBJ(obj, spi, pri, regCS, maskCS); 
//*     Where:    
//*         obj:        The name of the instance of the object to be declared   
//*         spi:        The SPI channel to use, NOT THE SPI BASE. i.e. "2" for SPI 2
//*         pri:        The priority to run the SPI ISR at (1 - 7).
//*         regCS:      This is the chip select LATx register. i.e. LATD
//*         maskCS:     This is the chip select mask i.e. (1<<14)  for pin 14
//*     
//*     For PPS:      
//*         You must set up the PPS registers in advance
//*     
//*     
//* As an example to create an instance of a DSPI object with an instance name of myDSPI using SPI 6 with an  isr priority of 3  
//*     DSPIOBJ(myDSPI, 6, 3, LATD, (1<<14));
//*     
//* ------------------------------------------------------------
#define DSPIPOBJ(obj, spi, pri, pinCS) DSPIOBJ(obj, spi, pri, pinCS)
#define DSPIOBJ(obj, spi, pri, regCS, maskCS) DSPIOBJ2(obj, spi, pri, regCS, maskCS)

//* ------------------------------------------------------------
//* You MUST declare an ISR in your code
//* This is a helper Macro to declare the ISR    
//* If you use the DSPIOBJ, this macro is automatically called   
//*     
//* DSPIISR(obj, vec, ipl)    
//*     Where:    
//*         obj is the name of the instance of your object.    
//*         vec is the DMA vector number  
//*         ipl is the priority you want to run the ISR, this needs to be the same as specified on the class constructor
//*     
//* As an example, if your object instance declaration was:   
//*    DSPIOBJ(myDSPI, 6, 3, LATD, (1<<14));   
//*     
//* You could declare your ISR as    
//*     DSPIISR(myDSPI, _SPI6_FAULT_VECTOR, 3);
//*     
//* ------------------------------------------------------------
#define DSPIISR(obj, vec, ipl) DSPIISR2(obj, vec, ipl)

/* ------------------------------------------------------------ */
/*		Resolved Macro implementations for declaration   		*/
/*		And creating the interrupt routines       				*/
/* ------------------------------------------------------------ */
#if defined(_SPI1_FAULT_VECTOR)     // individual Vectors _SPIx_FAULT_VECTOR
    #define DSPIOBJ2(obj, spi, pri, regCS, maskCS) \
        DSPI obj(SPI##spi##CON, _SPI##spi##_FAULT_VECTOR, _SPI##spi##_FAULT_VECTOR, pri, regCS, maskCS); \
        DSPIISR(obj, _SPI##spi##_FAULT_VECTOR, pri) \
        DSPIISR(obj, _SPI##spi##_RX_VECTOR, pri) \
        DSPIISR(obj, _SPI##spi##_TX_VECTOR, pri)
#else   // common vector  _SPIx_ERR_IRQ ; _SPI_x_VECTOR
    #define DSPIOBJ2(obj, spi, pri, regCS, maskCS) \
        DSPI obj(SPI##spi##CON, _SPI##spi##_ERR_IRQ, _SPI_##spi##_VECTOR, pri, regCS, maskCS); \
        DSPIISR(obj, _SPI_##spi##_VECTOR, pri) 
#endif

#if defined(OFF000)     // offset register
    #define DSPIISR2(obj, vec, ipl) void __attribute__((nomips16, at_vector(vec), interrupt(IPL##ipl##SRS))) ISR_VECTOR_##vec(void) { obj.isr(); }
#else   // vector table
    #define DSPIISR2(obj, vec, ipl) void __attribute__((nomips16, vector(vec), interrupt(IPL##ipl##SOFT))) ISR_VECTOR_##vec(void) { obj.isr(); }
#endif

/* ------------------------------------------------------------ */
/*		Abstract base class (interface) so DSPI and SoftSPI		*/
/*		Can implement a generic interface        				*/
/* ------------------------------------------------------------ */

class DGSPI
{
    public:

        // Initialization and setup functions.
        virtual bool        begin() = 0;
        virtual void        end() = 0;
        virtual void        setSpeed(uint32_t spd) = 0;
        virtual void        setMode(uint16_t mod) = 0;

        // Data transfer functions
        virtual void        setSelect(uint8_t sel) = 0;
        virtual uint8_t     transfer(uint8_t bVal) = 0;
        virtual void        transfer(uint16_t cbReq, uint8_t * pbSnd, uint8_t * pbRcv) = 0;
        virtual void        transfer(uint16_t cbReq, uint8_t * pbSnd) = 0;
        virtual void        transfer(uint16_t cbReq, uint8_t bPad, uint8_t * pbRcv) = 0;
};


/* ------------------------------------------------------------ */
/*					Object Class Declarations					*/
/* ------------------------------------------------------------ */

class DSPI : public DGSPI {

private:

    typedef struct _REG
    {
        volatile unsigned int reg;
        volatile unsigned int clr;
        volatile unsigned int set;
        volatile unsigned int inv;
    } REG;

    typedef struct _SPI {
        union
        {
            volatile __SPI2CONbits_t    spiCon;
            volatile REG                sxCon;
        };
        union
        {
            volatile __SPI2STATbits_t   spiStat;
            volatile REG                sxStat;
        };
        volatile REG sxBuf;
        volatile REG sxBrg;
    } SPI;

    typedef union _IPC {
        struct {
        unsigned subPriority:2;
        unsigned Priority:3;
        unsigned :3;
        };
        uint8_t b;
    } __attribute__((packed)) IPC;

    SPI&                spi;        // the spi base register
//    REG&                regSS;      // register for the CS pin
    volatile unsigned int&  regSS;      // register for the CS pin
    uint32_t const      maskSS;     // the pin mask for the register

    uint8_t const       vecFault;   // fault IRQ
    uint8_t const       vecPri;     // priority to run the interrupt routines
    REG&                regIE;      // spi IE register
    REG&                regIF;      // spi IF register
 	uint32_t const      bitErr;		//overrun error interrupt flag bit
	uint32_t const      bitRx;		//receive interrupt flag bit
	uint32_t const      bitTx;		//transmit interrupt flag bit

	volatile uint8_t *	pbSndCur;	//current point in transmit buffer
	volatile uint8_t *	pbRcvCur;	//current point in receive buffer
	volatile uint16_t	cbCur;		//count of bytes left to transfer
	uint8_t				bPad;		//pad byte for some transfers
	uint8_t				fRov;		//receive overflow error flag

    uint32_t            storedBrg;  // Previous baud rate before a setSpeed
    uint32_t            storedMode; // Previous mode before a setMode

    // this are really #defines in the name space of the class
    static int const high = 1;
    static int const low = 0;

//    void inline _setReg(REG& reg, uint32_t const mask, int state)
//    {
//        if(state == 0) reg.clr = mask;
//        else reg.set = mask;
//    }
    void inline _setReg(volatile unsigned int& reg, uint32_t const mask, int state)
    {
        if(state == 0) reg &= ~(mask);
        else reg |= mask;
    }

public:

    DSPI(volatile unsigned int& spiDSPI, uint8_t irqErr, uint8_t vecErr, uint8_t pri, volatile unsigned int& regCS, uint32_t const maskCS) : 
        spi(*((SPI *) &spiDSPI)), 
        regSS(regCS), maskSS(maskCS),
        vecFault(vecErr), vecPri(pri),
        regIE(*(((REG *) &IEC0) + (irqErr / 32))),
        regIF(*(((REG *) &IFS0) + (irqErr / 32))),
        bitErr(1 << (irqErr % 32)), bitRx(bitErr << 1), bitTx(bitErr << 2)
    {
    }

    // Initialization and setup functions.
    bool		begin();
    void		end();
    void		setSpeed(uint32_t spd);
    void        unsetSpeed();
    void		setMode(uint16_t  mod);
    void        unsetMode();
    void		setTransferSize(uint8_t txsize);

    // Data transfer functions.
    void		setSelect(volatile unsigned int& regCS, uint32_t const maskCS, uint8_t sel) { _setReg(regCS, maskCS, sel); };
    void		setSelect(uint8_t sel) { _setReg(regSS, maskSS, sel); };
    uint32_t	transfer(uint32_t bVal);
    uint8_t	    transfer(uint8_t bVal) { return((uint8_t) transfer((uint32_t) bVal)); }
    void		transfer(uint16_t cbReq, uint8_t * pbSnd, uint8_t * pbRcv);
    void		transfer(uint16_t cbReq, uint8_t * pbSnd);
    void		transfer(uint16_t cbReq, uint8_t bPad, uint8_t * pbRcv);

    // Interrupt control and interrupt driven I/O functions
    void		enableInterruptTransfer();
    void		disableInterruptTransfer();
    void		intTransfer(uint16_t cbReq, uint8_t * pbSnd, uint8_t * pbRcv);
    void		intTransfer(uint16_t cbReq, uint8_t * pbSnd);
    void		intTransfer(uint16_t cbReq, uint8_t bPadT, uint8_t * pbRcv);
    void		cancelIntTransfer();
    uint16_t	transCount() { return cbCur; };
    int			isOverflow() { return fRov; };
    void		clearOverflow() { fRov = 0; };

    // the ISR routine
	void	isr();
};
#endif // C++
#endif	// _DSPI_H_

/************************************************************************/
