/************************************************************************/
/*    DMASerial.cpp                                                     */
/*    Hardware Serial Library that uses DMA                             */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    11/23/2016 (KeithV): Created                                      */
/************************************************************************/

#include "DMASerial.h"

/* ------------------------------------------------------------ */
/*			General Declarations								*/
/* ------------------------------------------------------------ */

// if this is an MX with a PBDIV in the OSCCON
#if(_OSCCON_PBDIV_POSITION)
    #define UART_PBCLK (F_CPU >> OSCCONbits.PBDIV)

// otherwise the UART is on PB2
#else
    #define UART_PBCLK (F_CPU / (PB2DIVbits.PBDIV + 1))
#endif

// Baud rate above which we use high baud divisor (BRGH = 1)
#define LOW_HIGH_BAUD_SPLIT     200000

int DMASerial::begin(unsigned long baudRate, uint8_t address, uint8_t fAddress) {

    // clear the DMA interrupt flags
    IEDMAClr = IEIFDMAMask;
    IFDMAClr = IEIFDMAMask;

    uart.mode.w = 0;
    uart.sta.w = 0;

    if(fAddress)
    {
        if (baudRate < LOW_HIGH_BAUD_SPLIT) {
            // calculate actual BAUD generate value.
            uart.brg = ((UART_PBCLK / 16 / baudRate) - 1);  
            // set to 9 data bits, no parity
            uart.modeSet = 0b11 << _U1MODE_PDSEL_POSITION;                             
        } else {
            // calculate actual BAUD generate value.
            uart.brg = ((UART_PBCLK / 4 / baudRate) - 1);
            // set to 9 data bits, no parity
            uart.modeSet =  _U1MODE_BRGH_MASK + (0b11 << _U1MODE_PDSEL_POSITION); 
        }

        // set address of RS485 slave, enable transmitter and receiver and auto address detection
        uart.staSet = _U1STA_ADM_EN_MASK + (address << _U1STA_ADDR_POSITION) + _U1STA_UTXEN_MASK + _U1STA_URXEN_MASK;  
        enableAddressDetection(); // enable auto address detection
    }
    else
    {
        if (baudRate < LOW_HIGH_BAUD_SPLIT)
        {
            uart.brg    = ((UART_PBCLK / 16 / baudRate) - 1);       // calculate actual BAUD generate value.
        }
        else
        {
            uart.brg    = ((UART_PBCLK / 4 / baudRate) - 1);        // calculate actual BAUD generate value.
            uart.mode.w = _U1MODE_BRGH_MASK;                        // enable UART module
        }
        uart.sta.w  = _U1STA_UTXEN_MASK + _U1STA_URXEN_MASK;        // enable transmitter and receiver
    }

    // make sure the DMA controller is on
    if(!DMACONbits.ON) DMACONbits.ON = 1;

    // set the DMA interrupt priorities
    IPCDMA.Priority = priDMA;
    IPCDMA.subPriority = 0;

    // set up the DMA channel
    dma.conClr      = 0xFFFFFFFF;
    dma.econClr     = 0xFFFFFFFF;
    dma.intrClr     = 0xFFFFFFFF;
    dma.con.CHPRI   = 0;                    // Give the UART the lowest priority
    dma.con.CHAEN   = 1;                    // auto enable, keep cycling on block transfer
    dma.econ.CHSIRQ = irqRx;                // When there is a byte in the UART RX, DMA it out
    dma.econ.SIRQEN = 1;                    // enable start IRQ
    dma.ssa         = _KVA2PA((void *) &uart.rxReg);   // Source uart reg
    dma.ssiz        = 1;                    // source size 1 byte
    dma.dsa         = _KVA2PA(pBuff);       // the ring buffer
    dma.dsiz        = cbBuff;               // destination size 
    dma.csiz        = 1;                    // cell transfer size 1 byte
    dma.con.CHEN    = 1;                    // turn on the controller

    purge();
    uart.modeSet = _U1MODE_ON_MASK;         // enable UART module

    return(true);
}

int DMASerial::readBuffer(uint8_t * pBuffF, uint32_t cbBuffF)
{
    int cbAvailable = 0;
    int index       = 0;
    int i           = 0;

    // still processing someone else
    if(!isDMARxDone()) return(false);

    // before switching DMA pointers, move the remaining contents 
    // of the ring buffer to the target buffer.
    while(cbBuffF > 0 && available() > 0)
    {
        cbAvailable = _min(available(), (int) cbBuffF);                              // how many byte are in the buffer
        // move these out of the buffer now before switching

        for(; i < cbAvailable; i++)  pBuffF[index+i] = read();
        cbBuffF -= cbAvailable;
        index += cbAvailable;
        i = 0;
    }

    // all dregs of the input buffer have been copied except maybe 1 or 2
    // characters, we will pick this up after turning off the DMA

    if(cbBuffF > 0)
    {       
        // WARNING, if an interrupt or any combinations of interrupts 
        // can occur that will take longer than the time it takes for 8 characters
        // to come in, then those interrupts could cause a UART overflow error
        // and characters could be lost and the UART locked up while the DMA is disabled. 
        // if this is the case, then interrupts should be disabled while
        // switching the DMA pointers.

        // _disableInterrupts();                        // disable insterrupts

        // disable the DMA so no more char come in
        // we now rely on the 8 char UART FIFO
        dma.conClr      = _DCH0CON_CHEN_MASK;           // disable this DMA channel

        // get a final count of how many char to transfer
        // this should be 0 or 1 char
        // no more char are comeing in because the DMA is disabled
        // we are banking on the 8 byte FIFO in the UART to deal with 
        // this dead time. We have something like 28 usec to do this
        // BUT... that includes all of the potential interrupt routines getting in there as well
        cbAvailable = _min(available(), (int) cbBuffF); 
        for(; i < cbAvailable; i++)  pBuffF[index+i] = read();
        cbBuffF -= cbAvailable;
        index += cbAvailable;
    
        // make sure we have room in the buffer for all of this
        if(cbBuffF > 0)
        {
            dma.conClr      = _DCH0CON_CHAEN_MASK;      // disable auto cycling
            dma.dsa         = _KVA2PA(pBuffF) + index;  //  point to the new buffer
            dma.dsiz        = cbBuffF;                  // new size

            // enable the block transfer complete interrupt
            dma.intrClr     = 0xFFFFFFFF;
            dma.intrSet     = _DCH0INT_CHBCIE_MASK;
            IFDMAClr        = IEIFDMAMask;
            IEDMASet        = IEIFDMAMask;
        }

        dma.conSet      = _DCH0CON_CHEN_MASK;           // enable the channel
        // _enableInterrupts();                         // enable interrupts
    }

    return(true);
}

int DMASerial::writeBuffer(uint8_t * pBuf, uint32_t cbBuf, volatile void * pDMA)
{
    if(isDMATxDone()) 
    {
        pdmaTx = (DMA *) pDMA;

        // if a cached address is given, copy into non-cached memory
        if((uint8_t *) _KVA2KSEG1(pBuf) != pBuf && _KVA_IS_RAM(pBuf)) memcpy((void *) _KVA2KSEG1(pBuf), pBuf, cbBuf);

        // set up the DMA channel
        pdmaTx->conClr      = 0xFFFFFFFF;
        pdmaTx->econClr     = 0xFFFFFFFF;
        pdmaTx->intrClr     = 0xFFFFFFFF;
        pdmaTx->con.CHPRI   = 0;                    // Give the UART the lowest priority
        pdmaTx->con.CHAEN   = 0;                    // auto enable, don't cycling on block transfer
        pdmaTx->econ.CHSIRQ = irqRx+1;              // trigger on the transmit IRQ (rxIRQ + 1)
        pdmaTx->econ.SIRQEN = 1;                    // enable start IRQ
        pdmaTx->ssa         = _KVA2PA(pBuf);        // Source buffer
        pdmaTx->ssiz        = cbBuf;                    // source size 1 byte
        pdmaTx->dsa         = _KVA2PA((void *) &uart.txReg);    // the transmit buffer
        pdmaTx->dsiz        = 1;                    // destination size 
        pdmaTx->csiz        = 1;                    // cell transfer size 1 byte

        // wait until we can get at least one space in the transmit buffer.
        while ((uart.sta.w & _U1STA_UTXBF_MASK) != 0);

        // turn on the controller and let it rip
        pdmaTx->con.CHEN    = 1;                    

        return(true);
    }

    return(false);
}

/* ------------------------------------------------------------ */
/***	DMASerial::read
**
**	Parameters:
**		none
**
**	Return Value:
**		next character from the receive buffer
**
**	Errors:
**		returns -1 if no characters in buffer
**
**	Description:
**		Return the next character from the receive buffer and remove
**		it from the buffer, or -1 if no characters are available in
**		the buffer.
*/
int DMASerial::read(void)
{
	unsigned char ch;

	// if the head isn't ahead of the tail, we don't have any characters
	if (!dma.con.CHAEN || dma.dptr == iRead)
	{
		return(-1);
	}
	else
	{
		ch	= pBuff[iRead];
		iRead	= (iRead + 1) % cbBuff;
		return (ch);
	}
}

/* ------------------------------------------------------------ */
/***	DMASerial::write
**
**	Parameters:
**		ch		- the character to transmit
**
**	Return Value:
**		0 if not ready, 1 if one char transmitted.
**
**	Errors:
**		none
**
**	Description:
**		Wait until the transmitter is idle, and then transmit the
**		specified character.
*/
size_t DMASerial::write(uint8_t ch)
{
    // if we are doing a DMA write, this funcion won't work
    if(isDMATxDone()) 
    {
	    while ((uart.sta.w & _U1STA_UTXBF_MASK) != 0);	//check the UTXBF bit
	    uart.txReg = ch;
        return(1);
    }

    return(0);
}

// stream read methods
// love to make these inline, but they need to be virtual for the stream class
int DMASerial::available(void)
{
	if(dma.con.CHAEN) return (cbBuff + dma.dptr - iRead) % cbBuff;
    else return(0);
}

int DMASerial::peek()
{
	if (!dma.con.CHAEN || dma.dptr == iRead) return(-1);
	else return(pBuff[iRead]);
}

void DMASerial::flush()
{
	while ((uart.sta.w & _U1STA_TRMT_MASK) == 0);	//check the TRMT bit
}

void DMASerial::purge()
{
    if(dma.con.CHAEN) iRead = dma.dptr;
}

//#if 0
// print methods
size_t DMASerial::print(const char sz[]) 
{ 
    return(write((const uint8_t *) sz, strlen(sz))); 
}

size_t DMASerial::print(char ch) 
{ 
    return(write((uint8_t) ch)); 
}
// #endif

void DMASerial::isr(void)
{
    // clear the IE/IF flag
    IEDMAClr = IEIFDMAMask;
    IFDMAClr = IEIFDMAMask;

    // restore the original DMA buffers.
    dma.intrClr     = 0xFFFFFFFF;           // clear interrut IE/IF      
    dma.conSet      = _DCH0CON_CHAEN_MASK;  // turn on auto enable    
    dma.dsa         = _KVA2PA(pBuff);        // the ring buffer
    dma.dsiz        = cbBuff;               // destination size 

    purge();                                // reset the pointers
    dma.conSet      = _DCH0CON_CHEN_MASK;   // enable the channel
}

