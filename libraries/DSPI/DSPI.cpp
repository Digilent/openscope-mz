/************************************************************************/
/*																		*/
/*	DSPI.cpp	--	Implementation for Digilent SPI Library		*/
/*																		*/
/************************************************************************/
/*	Author: 	Gene Apperson											*/
/*	Copyright (c) 2011, Digilent Inc, All rights reserved				*/
/************************************************************************/
/*  Module Description: 												*/
/*																		*/
/* This is the main program module for the Digilent SPI library for use	*/
/* with the chipKIT system. This library supports access to all of the	*/
/* SPI ports defined on the board in use.								*/
/*																		*/
/************************************************************************/
/*  Revision History:													*/
/*																		*/
/*	10/28/2011(Gene Apperson): Created									*/
/*	05/27/2013(Claudia Goga) : Added PPS support for PIC32MX1/2			*/
/*	12/14/2016(Keith Vogel): Digilent owned relicensing                 */
/*	12/14/2016(Keith Vogel): Modified for the OpenScope                 */
/*																		*/
/************************************************************************/


/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */
#include	"DSPI.h"

#if defined(_SPI1CON_ENHBUF_POSITION)
#define ENH_BUFFER _SPI1CON_ENHBUF_POSITION
#elif defined(_SPI1ACON_ENHBUF_POSITION)
#define ENH_BUFFER _SPI1ACON_ENHBUF_POSITION
#endif

#if(_OSCCON_PBDIV_POSITION)
    #define SPI_PBCLK (F_CPU >> OSCCONbits.PBDIV)

// otherwise the SPI is on PB2
#else
    #define SPI_PBCLK (F_CPU / (PB2DIVbits.PBDIV + 1))
#endif

/* ------------------------------------------------------------ */
/***	DSPI::begin
**
**	Parameters:
**		none
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		Initialize the SPI port with all default values.
*/

bool DSPI::begin() {
    volatile IPC *      pIPC;
	volatile uint8_t    bTmp;   // volatile to make sure optimizer does not remove instruction
	uint16_t		    brg;

	/* Disable interrupts on this SPI controller.
	*/
	regIE.clr = bitErr + bitRx + bitTx;

	/* Disable and reset the SPI controller.
	*/
	spi.sxCon.reg = 0;

	/* Clear the receive buffer.
	*/
	bTmp = spi.sxBuf.reg;
    (void) bTmp;    // suppress unused variable complier warning

	/* Clear all SPI interrupt flags.
	*/
	regIF.clr = bitErr + bitRx + bitTx;

	/* Compute the address of the interrupt priority control register
	** used by this SPI controller.
	*/
    pIPC = ((volatile IPC *) (((uint8_t *) (&IPC0 + 4*(vecFault / 4))) + (vecFault % 4)));
    pIPC->Priority = vecPri;
    pIPC->subPriority = 0;

#if defined(_SPI1_FAULT_VECTOR)     // individual Vectors
    // RX vector
    pIPC = ((volatile IPC *) (((uint8_t *) (&IPC0 + 4*((vecFault+1) / 4))) + ((vecFault+1) % 4)));
    pIPC->Priority = vecPri;
    pIPC->subPriority = 0;

    // TX vector
    pIPC = ((volatile IPC *) (((uint8_t *) (&IPC0 + 4*((vecFault+2) / 4))) + ((vecFault+2) % 4)));
    pIPC->Priority = vecPri;
    pIPC->subPriority = 0;
#endif

	/* Set the default baud rate.
	*/
	brg = (uint16_t)((SPI_PBCLK / (2 * _DSPI_SPD_DEFAULT)) - 1);
	spi.sxBrg.reg = brg;

	/* Clear the receive overflow bit and receive overflow error flag
	*/
	spi.sxStat.clr = _SPI2STAT_SPIROV_MASK;
	fRov = 0;

    cbCur = 0;

	/* Enable the SPI controller.
	** Warning: if the SS pin ever becomes a LOW INPUT then SPI 
	** automatically switches to Slave, so the data direction of 
	** the SS pin MUST be kept as OUTPUT.
	*/
	spi.sxCon.reg = 0;
	spi.sxCon.set = _SPI2CON_ON_MASK + _SPI2CON_MSTEN_MASK + DSPI_MODE0;

    return(true);
}	

/* ------------------------------------------------------------ */
/***	DSPI::end
**
**	Parameters:
**		none
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		Return the object to the uninitialized state and disable
**		the SPI controller.
*/

void
DSPI::end() {

	spi.sxCon.reg = 0;	
	cbCur = 0;
}

/* ------------------------------------------------------------ */
/***	DSPI::setSpeed
**
**	Parameters:
**		spd		- clock speed to set in HZ
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		This sets the SPI clock speed to the highest supported
**		frequency that doesn't exceed the requested value. It computes
**		the appropriate value to load into the SPI baud register
**		based on requested clock speed and peripheral bus frequency.
*/

void
DSPI::setSpeed(uint32_t spd) {

	uint16_t	brg;

	/* Compute the baud rate divider for this frequency.
	*/
	brg = (uint16_t)((SPI_PBCLK / (2 * spd)) - 1);

	/* That the baud rate value is in the correct range.
	*/
	if (brg == 0xFFFF) {
		/* The user tried to set a frequency that is too high to support.
		** Set it to the highest supported frequency.
		*/
		brg = 0;
	}

	if (brg > 0x1FF) {
		/* The user tried to set a frequency that is too low to support.
		** Set it to the lowest supported frequency.
		*/
		brg = 0x1FF;
	}

	/* Write the value to the SPI baud rate register. Section 23. SPI
	** of the PIC32 Family Reference Manual says to disable the SPI
	** controller before writing to the baud register
	*/
	spi.sxCon.clr = _SPI2CON_ON_MASK;	// disable SPI
    storedBrg = spi.sxBrg.reg;
	spi.sxBrg.reg = brg;
	spi.sxCon.set = _SPI2CON_ON_MASK;	// enable SPI

}

/* Undo the last setSpeed() call to restore the previous baud rate */
void DSPI::unsetSpeed() { 
	spi.sxCon.clr = _SPI2CON_ON_MASK;	// disable SPI
	spi.sxBrg.reg = storedBrg;
	spi.sxCon.set = _SPI2CON_ON_MASK;	// enable SPI
}

/* ------------------------------------------------------------ */
/***	DSPI::setMode
**
**	Parameters:
**		mod		- requested SPI mode.
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		Set the SPI controller to the requested data mode. This
**		should be one of the values:
**			DSPI_MODE0, DSPI_MODE1, DSPI_MODE2, DSPI_MODE3
*/

void
DSPI::setMode(uint16_t mod) {

	if ((mod & ~(_SPI2CON_CKP_MASK | _SPI2CON_CKE_MASK)) != 0) {
		/* This is an invalid value.
		*/
		return;
	}

    storedMode = spi.sxCon.reg & (_SPI2CON_CKP_MASK | _SPI2CON_CKE_MASK);
	spi.sxCon.clr = _SPI2CON_ON_MASK;
	spi.sxCon.clr = (_SPI2CON_CKP_MASK | _SPI2CON_CKE_MASK);	// force both mode bits to 0

	spi.sxCon.set = mod;		// set the requested mode
	spi.sxCon.set = _SPI2CON_ON_MASK;
	
}

/* Undo the last setMode call */
void DSPI::unsetMode() {
	spi.sxCon.clr = _SPI2CON_ON_MASK;
	spi.sxCon.clr = (_SPI2CON_CKP_MASK | _SPI2CON_CKE_MASK);	// force both mode bits to 0

	spi.sxCon.set = storedMode;
	spi.sxCon.set = _SPI2CON_ON_MASK;
} 

/* ------------------------------------------------------------ */
/***	DSPI::setPinSelect
**
**	Parameters:
**		pin		- the pin to use as the slave select
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		This sets the pin used for slave select. It will make the
**		pin be an output driving high. This pin will then be use
**		by the setSelect method.
*/

void
DSPI::setTransferSize(uint8_t bits) {
	switch(bits) {
		default:
			// Anything that's not recognised we'll just take as 8 bit
		case DSPI_8BIT:
			spi.sxCon.clr = _SPI2CON_MODE32_MASK;
			spi.sxCon.clr = _SPI2CON_MODE16_MASK;
			break;
		case DSPI_16BIT:
			spi.sxCon.clr = _SPI2CON_MODE32_MASK;
			spi.sxCon.set = _SPI2CON_MODE16_MASK;
			break;
		case DSPI_32BIT:
			spi.sxCon.set = _SPI2CON_MODE32_MASK;
			spi.sxCon.clr = _SPI2CON_MODE16_MASK;
			break;
	}
}

/* ------------------------------------------------------------ */
/*				Data Transfer Functions							*/
/* ------------------------------------------------------------ */
/***	DSPI::transfer
**
**	Parameters:
**		bVal	- byte/word to send
**
**	Return Value:
**		returns byte/word received from the slave
**
**	Errors:
**		none
**
**	Description:
**		Send the specified byte to the SPI slave device, returning
**		the byte received from the slave device.
*/

uint32_t
DSPI::transfer(uint32_t bVal) {

	while ((spi.sxStat.reg & _SPI2STAT_SPITBE_MASK) == 0) {
	}
	spi.sxBuf.reg = bVal;

	while ((spi.sxStat.reg & _SPI2STAT_SPIRBF_MASK) == 0) {
	}

	return spi.sxBuf.reg;

}

/* ------------------------------------------------------------ */
/***	DSPI::transfer
**
**	Parameters:
**		cbReq	- number of bytes to transfer
**		pbSnd	- pointer to buffer to bytes to send
**		pbRcv	- pointer to hold received bytes
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		This function will send the requested bytes to the SPI
**		slave device, simultaneously saving the bytes received
**		from the slave device.
*/

void
DSPI::transfer(uint16_t cbReq, uint8_t * pbSnd, uint8_t * pbRcv) {

// If we have one ENHBUF we have all ENHBUF, and all the registers
// are the same.  We'll just use SPI1A's macros for all the ports
// as it makes no difference.

#if ENH_BUFFER
    spi.sxCon.set = 1<<ENH_BUFFER;
    uint16_t toWrite = cbReq;
    uint16_t toRead = cbReq;
    uint16_t rPos = 0;
    uint16_t wPos = 0;

    while (toWrite > 0 || toRead > 0) {
        if (toWrite > 0) {
            if ((spi.sxStat.reg & _SPI2STAT_SPITBF_MASK) == 0) {
                spi.sxBuf.reg = pbSnd[wPos++];
                toWrite--;
            }
        }
        if (toRead > 0) {
            if ((spi.sxStat.reg & _SPI2STAT_SPIRBE_MASK) == 0) {
                pbRcv[rPos++] = spi.sxBuf.reg;
                toRead--;
            }
        }
    }
    spi.sxCon.clr = 1<<ENH_BUFFER;
#else
    for (cbCur = cbReq; cbCur > 0; cbCur--) {
        *pbRcv++ = transfer(*pbSnd++);
    }
#endif
}

/* ------------------------------------------------------------ */
/***	DSPI::transfer
**
**	Parameters:
**		cbReq	- number of bytes to send to the slave
**		pbSnd	- buffer containing bytes to send
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		This function will send the requested bytes to the SPI
**		slave device, discarding the bytes received from the
**		slave.
*/

void
DSPI::transfer(uint16_t cbReq, uint8_t * pbSnd) {
#ifdef ENH_BUFFER
    spi.sxCon.set = 1<<ENH_BUFFER;
    uint16_t toWrite = cbReq;
    uint16_t toRead = cbReq;
    uint16_t wPos = 0;

    while (toWrite > 0 || toRead > 0) {
        if (toWrite > 0) {
            if ((spi.sxStat.reg & _SPI2STAT_SPITBF_MASK) == 0) {
                spi.sxBuf.reg = pbSnd[wPos++];
                toWrite--;
            }
        }
        if (toRead > 0) {
            if ((spi.sxStat.reg & _SPI2STAT_SPIRBE_MASK) == 0) {
                (void) spi.sxBuf.reg;
                toRead--;
            }
        }
    }
    spi.sxCon.clr = 1<<ENH_BUFFER;
#else

    for (cbCur = cbReq; cbCur > 0; cbCur--) {
        transfer(*pbSnd++);
    }
#endif
}

/* ------------------------------------------------------------ */
/***	DSPI::transfer
**
**	Parameters:
**		cbReq	- number of bytes to receive from the slave
**		bPad	- pad byte to send to slave
**		pbRcv	- buffer to hold received bytes
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		This function will receive the specified number of bytes
**		from the slave. The given pad byte will be sent to the
**		slave to cause the received bytes to be sent.
*/

void
DSPI::transfer(uint16_t cbReq, uint8_t bPad, uint8_t * pbRcv) {
#ifdef ENH_BUFFER
    spi.sxCon.set = 1<<ENH_BUFFER;
    uint16_t toWrite = cbReq;
    uint16_t toRead = cbReq;
    uint16_t rPos = 0;

    spi.sxCon.clr = _SPI2CON_MODE32_MASK | _SPI2CON_MODE16_MASK;

    while (toWrite > 0 || toRead > 0) {
        if (toWrite > 0) {
            if ((spi.sxStat.reg & _SPI2STAT_SPITBF_MASK) == 0) {
                spi.sxBuf.reg = bPad;
                toWrite--;
            }
        }
        if (toRead > 0) {
            if ((spi.sxStat.reg & _SPI2STAT_SPIRBE_MASK) == 0) {
                pbRcv[rPos++] = spi.sxBuf.reg;
                toRead--;
            }
        }
    }
    spi.sxCon.clr = 1<<ENH_BUFFER;
#else

    for (cbCur = cbReq; cbCur > 0; cbCur--) {
        *pbRcv++ = transfer(bPad);
    }
#endif
}


/* ------------------------------------------------------------ */
/*					Interrupt Control Functions					*/
/* ------------------------------------------------------------ */
/***	enableInterruptTransfer
**
**	Parameters:
**		none
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		Sets up the interrupt controller and enables interrupts
**		for this SPI port.
*/

void
DSPI::enableInterruptTransfer() {

	 clearOverflow();			// clear the receive overflow error flag

	/* Clear the interrupt flags and then enable SPI interrupts. Don't enable the
	** transmit interrupt now. This will happen in the transfer function that 
	** begins a data transfer operation.
	*/
	regIF.clr = bitRx + bitTx + bitErr;		// clear all interrupt flags
	regIE.set = bitRx + bitErr;				// enable interrupts

}

/* ------------------------------------------------------------ */
/***	disableInterruptTransfer
**
**	Parameters:
**		none
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		Turns off SPI interrupts for this SPI port.
*/

void
DSPI::disableInterruptTransfer() {

	/* Disable SPI interrupts and then clear the interrupt flags.
	*/
	regIE.clr = bitRx + bitTx + bitErr;	// disable all interrupts
	regIF.clr = bitRx + bitTx + bitErr;	// clear all interrupt flags

}

/* ------------------------------------------------------------ */
/***	DSPI::cancelIntTransfer
**
**	Parameters:
**		none
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		This will cancel an interrupt driven transfer. It is still
**		the caller's responsibility to drive SS high to release
**		the slave device.
*/

void
DSPI::cancelIntTransfer() {

	volatile uint8_t		bTmp;   // volatile to make sure optimizer does not remove instruction

	/* Clear the receive buffer.
	*/
	bTmp = spi.sxBuf.reg;
    (void) bTmp;    // suppress unused variable complier warning

	/* Clear the interrupt flags.
	*/
	regIF.clr = bitErr + bitRx + bitTx;

	/* Set the count of bytes remaining to 0.
	*/
	cbCur = 0;

}

/* ------------------------------------------------------------ */
/*					Interrupt Driven I/O Functions				*/
/* ------------------------------------------------------------ */
/***	DSPI::intTransfer
**
**	Parameters:
**		cbReq		- number of bytes to transfer
**		pbSnd		- pointer to buffer of bytes to transmit
**		pbRcv		- pointer to buffer to receive return bytes
**
**	Return Value:
**		none
**
**	Errors:
**		Overrun error handled in interrupt service routine
**
**	Description:
**		This function will set up and begin an interrupt driven
**		transfer where the received bytes are stored into a
**		receive buffer.
*/

void
DSPI::intTransfer(uint16_t cbReq, uint8_t * pbSnd, uint8_t * pbRcv) {

	/* Set up the control variables for the transfer.
	*/
	cbCur = cbReq;
	pbSndCur = pbSnd;
	pbRcvCur = pbRcv;

	/* Wait for the transmitter to be ready
	*/
	while ((spi.sxStat.reg & _SPI2STAT_SPITBE_MASK) == 0) {
	}

	/* Send the first byte.
	*/
	spi.sxBuf.reg = *pbSndCur++;

}

/* ------------------------------------------------------------ */
/***	DSPI::intTransfer
**
**	Parameters:
**		cbReq		- number of bytes to transfer
**		pbSnd		- pointer to buffer of bytes to transmit
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		This function will set up and begin an interrupt driven
**		transfer where the received bytes are discarded.
*/

void
DSPI::intTransfer(uint16_t cbReq, uint8_t * pbSnd) {

	/* Set up the control variables for the transfer.
	*/
	cbCur = cbReq;
	pbSndCur = pbSnd;
	pbRcvCur = 0;

	/* Wait for the transmitter to be ready
	*/
	while ((spi.sxStat.reg & _SPI2STAT_SPITBE_MASK) == 0) {
	}

	/* Send the first byte.
	*/
	spi.sxBuf.reg = *pbSndCur++;

}

/* ------------------------------------------------------------ */
/***	DSPI::intTransfer
**
**	Parameters:
**		cbReq		- number of bytes to receive
**		bPadT		- pad byte to send to the slave device
**		pbRcv		- pointer to buffer to receive returned bytes
**
**	Return Value:
**		none
**
**	Errors:
**		Overrun error handled by the interrupt service routine
**
**	Description:
**		This function will set up and begin an interrupt driven
**		transfer where there is no transmit buffer but the returned
**		bytes are stored. The given pad byte is sent to the slave
**		to cause the returned bytes to be sent by the slave.
*/

void
DSPI::intTransfer(uint16_t cbReq, uint8_t bPadT, uint8_t * pbRcv) {

	/* Set up the control variables for the transfer.
	*/
	cbCur = cbReq;
	pbSndCur = 0;
	pbRcvCur = pbRcv;
	bPad = bPadT;

	/* Wait for the transmitter to be ready
	*/
	while ((spi.sxStat.reg & _SPI2STAT_SPITBE_MASK) == 0) {
	}

	/* Send the first byte.
	*/
	spi.sxBuf.reg = bPad;

}

/* ------------------------------------------------------------ */
/***	DSPI::isr
**
**	Parameters:
**		none
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**	Description:
**		This function is called by the interrupt service routine
**		to handle SPI interrupts for this SPI port. It should only
**		be entered while performing an interrupt driven data
**		transfer operation.
*/

void
DSPI::isr() {

	uint8_t		bTmp;
	uint32_t	regIfs;

	/* Get the interrupt flag status.
	*/
	regIfs = regIF.reg;

	/* Check for and handle overrun error interrupt.
	*/
	if ((regIfs & bitErr) != 0) {
		fRov = 1;				// set the receive overflow error flag;
		spi.sxStat.clr = _SPI2STAT_SPIROV_MASK;	//clear status bit
		regIF.clr = bitErr;
	}

	/* Check for and handle receive interrupt.
	*/
	if ((regIfs & bitRx) != 0) {

		/* Get the received character.
		*/
		bTmp = spi.sxBuf.reg;		//read next byte from SPI controller
		cbCur -= 1;					//count this byte as received

		/* Are we storing it? pbRcvCur is 0 if we are sending only
		** and ignoring the received data.
		*/
		if (pbRcvCur != 0) {
			*pbRcvCur++ = bTmp;		//store the received byte into output buffer
		}

		/* Send the next byte to the slave. pbSndCur is 0 if we are
		** receiving only. In this case send the pad byte.
		*/
		if (cbCur > 0) {
			bTmp = (pbSndCur != 0) ? *pbSndCur++ : bPad;
			spi.sxBuf.reg = bTmp;
		}

		regIF.clr = bitRx;		//clear the receive interrupt flag

	}

}

