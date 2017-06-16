/************************************************************************/
/*                                                                      */
/*    OSSerial.h                                                        */
/*                                                                      */
/*    The Serial object used by the OpenScope                           */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2017, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*    1/20/2017(KeithV): Created                                        */
/************************************************************************/

#ifndef OSSerial_h
#define OSSerial_h

#include "./libraries/DMASerial/DMASerial.h"

#ifdef __cplusplus


//* ------------------------------------------------------------
//*                          READ THIS
//*     
//*  WARNING: THIS LIBRARY WILL ONLY WORK WITH UARTS THAT HAVE AN 8 BYTE DEEP FIFO   
//*     
//* The OSSerialOBJ MACRO will make it easy for you to create your   
//* Serial object instance AND the ISR routine    
//* You can use this macro at the global scope of a module    
//*     
//*     
//* OSSerialOBJ(obj, buff, buffSize, uart, dma, pri);  
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
//* As an example to create an instance of a OSSerial object with an instance name of SerialDMA using uart 5 dma 0 with a dma isr priority of 4  
//*     OSSerialOBJ(SerialDMA, uartBuff, sizeof(uartBuff), 5, 0, 4);
//*     
//* ------------------------------------------------------------
#define OSSerialOBJ(obj, buff, buffSize, uart, dma, pri) OSSerialOBJ2(obj, buff, buffSize, uart, dma, pri)

//* ------------------------------------------------------------
//* You MUST declare an ISR in your code
//* This is a helper Macro to declare the ISR    
//* If you use the OSSerialOBJ, this macro is automatically called   
//*     
//* OSSerialISR2(obj, vec, ipl)    
//*     Where:    
//*         obj is the name of the instance of your object.    
//*         vec is the DMA vector number  
//*         ipl is the priority you want to run the ISR, this needs to be the same as specified on the class constructor
//*     
//* As an example, if your object instance declaration was:   
//*     OSSerial SerialDMA(uartBuff, sizeof(uartBuff), &U4MODE, _UART4_RX_VECTOR, &DCH0CON, _DMA0_VECTOR);   
//*     
//* You could declare your ISR as    
//*     OSSerialISR(SerialDMA, _DMA0_VECTOR, 4);
//*     
//* ------------------------------------------------------------
#define OSSerialISR(obj, vec, ipl) OSSerialISR2(obj, vec, ipl)

// the actual implementation of the helper macros
#if defined(_UART1_RX_VECTOR) // _DMAx_VECTOR _UARTx_RX_VECTOR
    #define OSSerialOBJ2(obj, buff, buffSize, uart, dma, pri) \
        OSSerial obj(buff, buffSize, &U##uart##MODE, _UART##uart##_RX_VECTOR, &DCH##dma##CON, _DMA##dma##_VECTOR, pri); \
        OSSerialISR(obj, _DMA##dma##_VECTOR, pri)
#else // _DMAx_IRQ _DMA_x_VECTOR _UARTx_RX_IRQ 
    #define OSSerialOBJ2(obj, buff, buffSize, uart, dma, pri) \
        OSSerial obj(buff, buffSize, &U##uart##MODE, _UART##uart##_RX_IRQ, &DCH##dma##CON, _DMA##dma##_IRQ, pri); \
        OSSerialISR(obj, _DMA_##dma##_VECTOR, pri)
#endif

#if defined(OFF000)     // offset register
    #define OSSerialISR2(obj, vec, ipl) void __attribute__((nomips16, at_vector(vec),interrupt(IPL##ipl##SRS))) ISR_VECTOR_##vec(void) { obj.isr(); }
#else
    #define OSSerialISR2(obj, vec, ipl) void __attribute__((nomips16, vector(vec),interrupt(IPL##ipl##SOFT))) ISR_VECTOR_##vec(void) { obj.isr(); }
#endif

//* ------------------------------------------------------------
//* 		Object Class Declarations
//* ------------------------------------------------------------

class OSSerial : public DMASerial
{

private:

    uint8_t __attribute__((coherent)) _printBuff[0x4000];
    uint8_t *   printBuff;
    uint32_t    iPrintBuffStart = 0;
    uint32_t    iPrintBuffEnd = 0;
    uint32_t    cbDMA = 0;
    bool        fEnablePrint;

    OSSerial();

public:

    OSSerial(uint8_t * const pBuffP,  uint32_t const cbBuffP, volatile void * pUART, uint8_t const irqRxP, volatile void * pDMA, uint8_t const irqDMAP, uint8_t const priDMAP) :
        DMASerial(pBuffP, cbBuffP, pUART, irqRxP, pDMA, irqDMAP, priDMAP),
            printBuff((uint8_t *) _KVA2KSEG1(_printBuff)), iPrintBuffStart(0), iPrintBuffEnd(0), cbDMA(0), fEnablePrint(false) 
        {
        
        }

        void PeriodicTask(volatile void * pDMA); 
        
        // print methods
        using DMASerial::print;     // pick up all of the other print methods

        // if we say "using", then we pick up ALL of the print methods, so the following
        // can not be definded in DMASerial, we commented them out of DMASeral
        virtual size_t print(const char sz[]);
        virtual size_t print(char ch);

        bool EnablePrint(void) 
        { 
            return(fEnablePrint); 
        }

        bool EnablePrint(bool fEP) 
        { 
            fEnablePrint = fEP; 
            return(fEnablePrint);
        }
};

#endif

#endif