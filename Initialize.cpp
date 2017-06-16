
/************************************************************************/
/*                                                                      */
/*    Instrument.C                                                      */
/*                                                                      */
/*    Creates and manages Instrument Handles                            */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    2/25/2016 (KeithV): Created                                       */
/************************************************************************/
#include <OpenScope.h>


STATE ResetInstruments(void) 
{
    PJCMD   pjcmdT  = PJCMD();

    // check for calibration
    if(pjcmd.iCal.state.processing  != Idle)
    {
        return(InstrumentInUse);
    }

    //**************************************************************************
    //*******************  TMR 9 Abort Trigger  ********************************
    //**************************************************************************
    TRGAbort();
    memcpy(&pjcmd.trigger, &pjcmdT.trigger, sizeof(pjcmd.trigger));

    //**************************************************************************
    //*******************  GPIO PINs  ******************************************
    //**************************************************************************
    TRISE           = 0xFFFF;           // All input Tri-state
    memcpy(&pjcmd.igpio, &pjcmdT.igpio, sizeof(pjcmd.igpio));

    //**************************************************************************
    //*******************  DCOUT 1 PWM6  ***************************************
    //**************************************************************************
    OC6RS               = PWMIDEALCENTER;   // set to midpoint
    memcpy(&pjcmd.idcCh1, &pjcmdT.idcCh1, sizeof(pjcmd.idcCh1));

    //**************************************************************************
    //*******************  DCOUT 2 PWM4  ***************************************
    //**************************************************************************
    OC4RS               = PWMIDEALCENTER;          // set to midpoint
    memcpy(&pjcmd.idcCh2, &pjcmdT.idcCh2, sizeof(pjcmd.idcCh2));

    //**************************************************************************
    //*******************  AWG/DAC PWM7  ***************************************
    //*******************   LA           ***************************************
    //**************************************************************************
    T7CONbits.ON        = 0;                // turn off trigger timer
    DCH7CONbits.CHEN    = 0;                // turn off DMA
    DCH7DSA             = KVA_2_PA(&LATH);  // Latch H address for destination
    DCH7DSIZ            = 2;                // destination size 2 byte
    DCH7CSIZ            = 2;                // cell transfer size 2 byte
    DCH7SSIZ            = 0;                // init to zero
    LATH                = DACDATA(511);     // set DAC output to mid range
    OC7RS               = PWMIDEALCENTER;   // set to midpoint
    memcpy(&pjcmd.iawg, &pjcmdT.iawg, sizeof(pjcmd.iawg));
    memcpy(&pjcmd.ila,  &pjcmdT.ila,  sizeof(pjcmd.ila));

    //**************************************************************************
    //**************************************************************************
    //*******************  ADC 0/1  ********************************************
    //*******************  TMR3 OC5, DMA 3/4 ***********************************
    //**************************************************************************
    //**************************************************************************
    OSCSetGain(rgInstr[OSC1_ID], 4);
    T3CONbits.ON        = 0;                    
    DCH3CONbits.CHEN    = 0;
    DCH4CONbits.CHEN    = 0;
    OC8RS               = PWMIDEALCENTER;       // ADC offset   
    OC5R                = INTERLEAVEOC(32);     // interleave OC   
    memcpy(&pjcmd.ioscCh1, &pjcmdT.ioscCh1, sizeof(pjcmd.ioscCh1));

    //**************************************************************************
    //**************************************************************************
    //*******************  ADC 2/3  ********************************************
    //*******************  TMR5 OC1, DMA 5/6 ***********************************
    //**************************************************************************
    //**************************************************************************
    OSCSetGain(rgInstr[OSC2_ID], 4);
    T5CONbits.ON        = 0;                    
    DCH5CONbits.CHEN    = 0;
    DCH6CONbits.CHEN    = 0;
    OC9RS               = PWMIDEALCENTER;   // ADC offset         
    OC1R                = INTERLEAVEOC(32); // interleave OC    
    memcpy(&pjcmd.ioscCh2, &pjcmdT.ioscCh2, sizeof(pjcmd.ioscCh2));

    return(Idle);
}

STATE InitInstruments(void) 
{
    unsigned int    val;
    uint32_t        cTADWarmUp = 0;
  
    // We assume the processor is unlocked, so let's just assert that here
    // While the processor comes up in the unlocked state after reset it is
    // possible that the rest code was jumped to after someone locked the system
    // and we need to re-assert the unlock.
    SYSKEY = 0;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;

    CFGCONbits.IOLOCK   = 0;    // unlock PPS; same reasoning as explicitly unlocking the processor
    CFGCONbits.OCACLK   = 1;    // use alternate OC timers as inputs
    
    CFGCONbits.DMAPRI   = 1;    // give the DMA priority arbitration on the bus
    CFGCONbits.CPUPRI   = 0;    // give the CPU Least Recently Used priority arbitration on the bus

    CFGCONbits.TROEN    = 0;    // disable trace output

    CFGCONbits.TDOEN    = 1;    // use JTAG TDO output
    CFGCONbits.JTAGEN   = 1;    // enable the JTAG port
    
    CFGCONbits.IOANCPEN = 0;    // VDD >= 2.5v set to 0 (this is the charge pump for < 2.5v)              


    // Set wait states and enable prefetch buffer 
    PRECON = 0u 
            | (2u << _PRECON_PFMWS_POSITION)  // 2 wait states 
            | (3u << _PRECON_PREFEN_POSITION); // Enable prefetch for instructions + data 

    // Set the CP0 bit so that interrupt exceptions use the
	// special interrupt vector and not the general exception vector.
    asm volatile("mfc0   %0,$13" : "=r"(val));
    val |= 0x00800000;
    asm volatile("mtc0   %0,$13" : "+r"(val));

    // allow for debugging, this will stop the core timer when the debugger takes control
    _CP0_BIC_DEBUG(_CP0_DEBUG_COUNTDM_MASK); 

    // if JTAG support is enabled
    #if defined(_JTAG) && (_JTAG == 1)
	    CFGCON = CFGCON | _CFGCON_TDOEN_MASK | _CFGCON_TROEN_MASK | _CFGCON_JTAGEN_MASK;
    #else
	    CFGCONbits.JTAGEN = 0;
    #endif
   
    // jack up PB4 (IO Bus) to 200MHz
    while(!PB4DIVbits.PBDIVRDY);        // wait to ensure we can set it.
    PB4DIVbits.PBDIV = 0;               // set to sysclock speed
//    PB4DIVbits.PBDIV = 1;             // set to sysclock/2 speed
    while(!PB4DIVbits.PBDIVRDY);        // wait until we are done.
    
    // set up some default shadow registers for each interrupt priority level
    // the shadow register set used is the same as the priority level
    PRISS = 0x76543210;

	// Turn on multi-vectored interrupts.
    INTCONSET = _INTCON_MVEC_MASK;


    //**************************************************************************
    //*******************  Turn things OFF *************************************
    //**************************************************************************

    // timers OFF
    T1CON               = 0;                    
    T2CON               = 0;                    
    T3CON               = 0;                    
    T4CON               = 0;                    
    T5CON               = 0;                    
    T6CON               = 0;                    
    T7CON               = 0;                    
    T8CON               = 0;                    
    T9CON               = 0;  

    // OC OFF
    OC1CON              = 0; 
    OC2CON              = 0; 
    OC3CON              = 0; 
    OC4CON              = 0; 
    OC5CON              = 0; 
    OC6CON              = 0; 
    OC7CON              = 0; 
    OC8CON              = 0; 
    OC9CON              = 0; 

    // DMA OFF
    DCH0CON         = 0;
    DCH1CON         = 0;
    DCH2CON         = 0;
    DCH3CON         = 0;
    DCH4CON         = 0;
    DCH5CON         = 0;
    DCH6CON         = 0;
    DCH7CON         = 0;

    // change notice off
    CNCONE = 0;

    //**************************************************************************
    //*******************  Start with everything mapped as a digital pin  ******
    //**************************************************************************
    ANSELA              = 0;
    ANSELB              = 0;
    ANSELC              = 0;
    ANSELD              = 0;
    ANSELE              = 0;
    ANSELF              = 0;
    ANSELG              = 0;
    ANSELH              = 0;
    ANSELJ              = 0;

    //**************************************************************************
    //**************************  Serial I/O UART  *****************************
    //**************************************************************************
    //  0       RC14    SOSCO/RPC14/T1CK/RC14               RX Serial Monitor   RX Serial Monitor         
    //  1       RD11    EMDC/RPD11/RD11                     TX Serial Moinitor  TX Serial Moinitor     
    U5RXR               = 0b0111;
    RPD11R              = 0b0011; 

    //**************************************************************************
    //***************************  uSD SPI Port  *******************************
    //**************************************************************************
    //  51      RD15    AN33/RPD15/SCK6/RD15                uSD SCK6            uSD SCK6  
    //  52      RD14    AN32/AETXD0/RPD14/RD14              uSD SS6             uSD SS6 
    //  53      RD5     SQICS1/RPD5/RD5                     uSD SDI6            uSD SDI6
    //  54      RD12    EBID12/RPD12/PMD12/RD12             uSD SDO6            uSD SDO6
    SDI6R               = 0b0110;
    RPD12R              = 0b1010;

    //**************************************************************************
    //**************************  SD CS UART3  **********************************
    //**************************  Used for SD card CS  *************************
    //******* We do this becasue we can have anything on the IO bus ************
    //******* When the logic analyzer runs, not even CS ************************
    //**************************************************************************
#if defined(NO_IO_BUS)
    RPD14R              = 0b0001;   // U3TX
    U3MODEbits.IREN     = 1;        // give TX the positive sense idle state
    U3STAbits.UTXEN     = 1;        // enable transmit pin
    U3STAbits.UTXINV    = 1;        // Idle state is high
    U3BRG               = 650;      // approximately 9600 baud, don't care really 
    U3MODEbits.ON       = 1;        // turn on the controller.
#else
    TRISDbits.TRISD14   = 0;
    LATDbits.LATD14     = 1;        // set High, not selected
#endif

    // SD code assumes that if the SD card is not initialized a 1 will come
    // in on the MISO pin. However some SD cards leave their SDO (MISO) pin tristate
    // until initialized and a 1 may not come in to the MCU pre-initialization
    // So turn on the weak pullup to make sure that if everyone is tristated a 1
    // will shift in on this pin.
    // In addition, the OpenScope has a 10K pullup resistor on the board
    // so technecally we don't need to do this.
    CNPUDbits.CNPUD5    = 1;        // turn on the pullup resistor on uSD MISO
  
    // Turn on the pullup for the SD DET
    CNPUDbits.CNPUD1 = 1;
    
    //**************************************************************************
    //***************************  MRF24 SPI Port  *****************************
    //**************************************************************************
    //  55      RD10    RPD10/SCK4/RD10                     MRF24 SCK4          MRF24 SCK4
    //  56      RB15    EBIA0/AN10/RPB15/OCFB/PMA0/RB15     MRF24 SS4           MRF24 SS4            
    //  57      RG7     EBIA4/AN13/C1INC/RPG7/SDA4/PMA4/RG7 MRF24 SDI4          MRF24 SDI4
    //  58      RA15    RPA15/SDA1/RA15                     MRF24 SDO4          MRF24 SDO4
    //  59      RG8     EBIA3/AN12/C2IND/RPG8/SCL4/PMA3/RG8 MRF24 INT3          MRF24 INT3      
    //  60      RD13    EBID13/PMD13/RD13                   MRF24 HIB           MRF24 HIB
    //  61      RA4     EBIA14/PMCS1/PMA14/RA4              MRF24 RESET         MRF24 RESET
    SDI4R               = 0b0001;           // SDI
    RPA15R              = 0b1000;           // SDO

    //**************************************************************************
    //**************************  MRF24 CS UART4  ******************************
    //**************************  Used for MRF24 CS  ***************************
    //******* We do this becasue we can have anything on the IO bus ************
    //******* When the logic analyzer runs, not even CS ************************
    //**************************************************************************
#if defined(NO_IO_BUS)
    RPB15R              = 0b0010;   // U4TX, also in networkProfile.h
    U4MODEbits.IREN     = 1;        // give TX the positive sense idle state
    U4STAbits.UTXEN     = 1;        // enable transmit pin
    U4STAbits.UTXINV    = 1;        // Idle state is high
    U4BRG               = 650;      // approximately 9600 baud, don't care really 
    U4MODEbits.ON       = 1;        // turn on the controller.

    TRISGbits.TRISG1 = 1;   // make it an input
    CNPUGbits.CNPUG1 = 1;   // turn on pull up resistor
#else
    TRISBbits.TRISB15   = 0;
    LATBbits.LATB15     = 1;        // set High, not selected
#endif

    //**************************************************************************
    //*******************  GPIO PINs  ******************************************
    //**************************************************************************
    TRISE           = 0xFFFF;           // All input s

    //**************************************************************************
    //*******************  LEDs  and Buttons ***********************************
    //**************************************************************************

    // LED 1
    TRISJbits.TRISJ4 = 0;
    LATJbits.LATJ4 = 0;

    // LED 2
    TRISJbits.TRISJ2 = 0;
    LATJbits.LATJ2 = 0;

    // LED 3
    TRISJbits.TRISJ1 = 0;
    LATJbits.LATJ1 = 0;

    // LED 4
    TRISJbits.TRISJ0 = 0;
    LATJbits.LATJ0 = 0;

    // BUTTON 1 as in input
    TRISGbits.TRISG12 = 1;

    //**************************************************************************
    //*****************************  Set Pin Usage  ****************************
    //**************************************************************************
    TRISAbits.TRISA0    = 1;    //  AIN1FB
    TRISAbits.TRISA1    = 1;    //  AIN2FB
    TRISBbits.TRISB0    = 1;    //  AIN1
    TRISBbits.TRISB1    = 1;    //  AIN1
    TRISBbits.TRISB2    = 1;    //  AIN2
    TRISBbits.TRISB3    = 1;    //  AIN2
    TRISBbits.TRISB11   = 1;    //  DC1FB
    TRISBbits.TRISB12   = 1;    //  DC2FB
    TRISBbits.TRISB13   = 1;    //  AWGFB
    TRISCbits.TRISC4    = 1;    //  VCC5V0-FB
    TRISGbits.TRISG15   = 1;    //  VCC3V3FB
    TRISCbits.TRISC1    = 1;    //  VREF3V0FB
    TRISCbits.TRISC3    = 1;    //  VREF1V5FB
    TRISGbits.TRISG9    = 1;    //  VCC5V0FB

    ANSELAbits.ANSA0    = 1;    //  AIN1FB
    ANSELAbits.ANSA1    = 1;    //  AIN2FB 
    ANSELBbits.ANSB0    = 1;    //  AIN1
    ANSELBbits.ANSB1    = 1;    //  AIN1
    ANSELBbits.ANSB2    = 1;    //  AIN2
    ANSELBbits.ANSB3    = 1;    //  AIN2 
    ANSELBbits.ANSB11   = 1;    //  DC1FB
    ANSELBbits.ANSB12   = 1;    //  DC2FB
    ANSELBbits.ANSB13   = 1;    //  AWGFB
    ANSELCbits.ANSC4    = 1;    //  VCC5V0-FB
    ANSELGbits.ANSG15   = 1;    //  VCC3V3FB
    ANSELCbits.ANSC1    = 1;    //  VREF3V0FB
    ANSELCbits.ANSC3    = 1;    //  VREF1V5FB
    ANSELGbits.ANSG9    = 1;    //  VCC5V0FB

    //**************************************************************************
    //********************  Configure and turn on the ADCs  ********************
    //**************************************************************************
    
    // Initialize MCHP ADC Calibration Data
    ADC0CFG = DEVADC0;
    ADC1CFG = DEVADC1;
    ADC2CFG = DEVADC2;
    ADC3CFG = DEVADC3;
    ADC4CFG = DEVADC4;
    ADC7CFG = DEVADC7;

    ADCCON1     = 0; 
    ADCCON2     = 0; 
    ADCCON3     = 0; 
    ADCANCON    = 0;
    ADCTRGMODE  = 0;
    ADCIMCON1   = 0x00000155;   // signed single ended mode (not differential)
    ADCIMCON2   = 0x00000000;   // signed single ended mode (not differential)
    ADCIMCON3   = 0x00000000;   // signed single ended mode (not differential)
    ADCTRGSNS   = 0;
 
    // resolution 0 - 6bits, 1 - 8bits, 2 - 10bits, 3 - 12bits
    ADCCON1bits.SELRES  =   RES12BITS;  // shared ADC, 12 bits resolution (bits+2 TADs, 12bit resolution = 14 TAD).

    // 0 - no trigger, 1 - clearing software trigger, 2 - not clearing software trigger, the rest see datasheet
    ADCCON1bits.STRGSRC     = 1;    //Global software trigger / self clearing.

    // 0 - internal 3.3, 1 - use external VRef+, 2 - use external VRef-
    ADCCON3bits.VREFSEL     = VREFHEXT;    // use extern VREF3V0

    // these should be set if VDD <= 2.5v
    // ADCCON1bits.AICPMPEN     = 1;    // turn on the analog charge pump
    
    // set up the TQ and TAD and S&H times

    // TCLK: 00- pbClk3, 01 - SysClk, 10 - External Clk3, 11 - interal 8 MHz clk
    ADCCON3bits.ADCSEL      = CLKSRCSYSCLK;             // TCLK clk == Sys Clock == F_CPU  

    // Global ADC TQ Clock: Global ADC prescaler 0 - 63; Divide by (CONCLKDIV*2) However, the value 0 means divide by 1
    ADCCON3bits.CONCLKDIV   = TQCONCLKDIV;                // Divide by 1 == TCLK == SYSCLK == F_CPU

    // must be divisible by 2 
    ADCCON2bits.ADCDIV      = ADCTADDIV;   // run TAD at 50MHz

    ADCCON2bits.SAMC        = (ADCTADSH - 2);   // for the shared S&H this will allow source impedances < 10Kohm

    // with 50MHz TAD and 68 TAD S&H and 14 TAD for 12 bit resolution, that is 25000000 / (68+14) = 609,756 Sps or 1.64 us/sample

    // initialize the warm up timer
    // 20us or 500 TAD which ever is higher 1/20us == 50KHz
    cTADWarmUp = ((F_CPU / (ADCCON3bits.CONCLKDIV == 0 ? 1 : (ADCCON3bits.CONCLKDIV * 2))) / (F_CPU / ADCTADFREQ) / 50000ul);
    if(cTADWarmUp < 500) 
    {
        cTADWarmUp = 500;
    }

    // get the next higher power of the count
    for(val=0; val<16; val++)
    {
        if((cTADWarmUp >> val) == 0)
        {
            break;
        }
    }

    // the warm up count is 2^^X where X = 0 -15
    ADCANCONbits.WKUPCLKCNT = val; // Wakeup exponent = 2^^15 * TADx   
  
    // ADC 0
    ADC0TIMEbits.ADCDIV     = ADCCON2bits.ADCDIV;       // ADC0 TAD = 50MHz
    ADC0TIMEbits.SAMC       = (ADCTADDC-2);             // ADC0 sampling time = (SAMC+2) * TAD0
    ADC0TIMEbits.SELRES     = ADCCON1bits.SELRES;       // ADC0 resolution is 12 bits 
    ADCIMCON1bits.SIGN0     = 1;                        // signed data format

    // ADC 1
    ADC1TIMEbits.ADCDIV     = ADCCON2bits.ADCDIV;       // ADC1 TAD = 50MHz
    ADC1TIMEbits.SAMC       = (ADCTADDC-2);             // ADC1 sampling time = (SAMC+2) * TAD0
    ADC1TIMEbits.SELRES     = ADCCON1bits.SELRES;       // ADC1 resolution is 12 bits 
    ADCIMCON1bits.SIGN1     = 1;                        // signed data format

    // ADC 2
    ADC2TIMEbits.ADCDIV     = ADCCON2bits.ADCDIV;       // ADC2 TAD = 50MHz
    ADC2TIMEbits.SAMC       = (ADCTADDC-2);             // ADC2 sampling time = (SAMC+2) * TAD0
    ADC2TIMEbits.SELRES     = ADCCON1bits.SELRES;       // ADC2 resolution is 12 bits 
    ADCIMCON1bits.SIGN2     = 1;                        // signed data format

    // ADC 3
    ADC3TIMEbits.ADCDIV     = ADCCON2bits.ADCDIV;       // ADC3 TAD = 50MHz
    ADC3TIMEbits.SAMC       = (ADCTADDC-2);             // ADC3 sampling time = (SAMC+2) * TAD0
    ADC3TIMEbits.SELRES     = ADCCON1bits.SELRES;       // ADC3 resolution is 12 bits 
    ADCIMCON1bits.SIGN3     = 1;                        // signed data format

    // ADC 4
    ADC4TIMEbits.ADCDIV     = ADCCON2bits.ADCDIV;       // ADC4 TAD = 50MHz
    ADC4TIMEbits.SAMC       = ADCCON2bits.SAMC;         // ADC4 sampling time = (SAMC+2) * TAD0
    ADC4TIMEbits.SELRES     = ADCCON1bits.SELRES;       // ADC4 resolution is 12 bits 
    ADCIMCON1bits.SIGN4     = 1;                        // signed data format

    /* Configure ADCIRQENx */
    ADCCMPEN1 = 0; // No interrupts are used
    ADCCMPEN2 = 0;
    
    /* Configure ADCCSSx */
    ADCCSS1 = 0; // No scanning is used
    ADCCSS2 = 0;
    
    /* Configure ADCCMPxCON */
    ADCCMP1 = 0; // No digital comparators are used. Setting the ADCCMPxCON
    ADCCMP2 = 0; // register to '0' ensures that the comparator is disabled.
    ADCCMP3 = 0; // Other registers are ?don't care?.
    ADCCMP4 = 0;
    ADCCMP5 = 0;
    ADCCMP6 = 0;    

    /* Configure ADCFLTRx */
    ADCFLTR1 = 0; // Clear all bits
    ADCFLTR2 = 0;
    ADCFLTR3 = 0;
    ADCFLTR4 = 0;
    ADCFLTR5 = 0;
    ADCFLTR6 = 0;
    
    // disable all global interrupts
    ADCGIRQEN1 = 0;
    ADCGIRQEN2 = 0;
    
    /* Early interrupt */
    ADCEIEN1 = 0; // No early interrupt
    ADCEIEN2 = 0;

    // turn on the ADCs
    ADCCON1bits.ON = 1;

    /* Wait for voltage reference to be stable */
    while(!ADCCON2bits.BGVRRDY); // Wait until the reference voltage is ready
    while(ADCCON2bits.REFFLT); // Wait if there is a fault with the reference voltage
    
    /* Enable clock to analog circuit */
    ADCANCONbits.ANEN0 = 1; // Enable the clock to analog bias and digital control
    ADCANCONbits.ANEN1 = 1; // Enable the clock to analog bias and digital control
    ADCANCONbits.ANEN2 = 1; // Enable the clock to analog bias and digital control
    ADCANCONbits.ANEN3 = 1; // Enable the clock to analog bias and digital control
    ADCANCONbits.ANEN4 = 1; // Enable the clock to analog bias and digital control
    ADCANCONbits.ANEN7 = 1; // Enable the clock to analog bias and digital control
   
    /* Wait for ADC to be ready */
    while(!ADCANCONbits.WKRDY0); // Wait until ADC0 is ready
    while(!ADCANCONbits.WKRDY1); // Wait until ADC1 is ready
    while(!ADCANCONbits.WKRDY2); // Wait until ADC2 is ready
    while(!ADCANCONbits.WKRDY3); // Wait until ADC3 is ready
    while(!ADCANCONbits.WKRDY4); // Wait until ADC4 is ready
    while(!ADCANCONbits.WKRDY7); // Wait until ADC7 is ready
        
    /* Enable the ADC module */
    ADCCON3bits.DIGEN0 = 1; // Enable ADC0
    ADCCON3bits.DIGEN1 = 1; // Enable ADC1
    ADCCON3bits.DIGEN2 = 1; // Enable ADC2
    ADCCON3bits.DIGEN3 = 1; // Enable ADC3
    ADCCON3bits.DIGEN4 = 1; // Enable ADC3
    ADCCON3bits.DIGEN7 = 1; // Enable shared ADC

    // This has conflicting documentation, this is to make
    // sure we do not have early ADC interrupts enabled.
    ADCCON2bits.ADCEIOVR = 0;           // override early interrupts

    //**************************************************************************
    //*******************  Timers to drive PWM Offset circuits  ****************
    //******************** T2 drives DC Output timers **************************
    //**************************************************************************
    T2CONbits.TCKPS = PWMPRESCALER;         // timer pre scaler
    PR2             = (PWMPERIOD - 1);      // match count
    TMR2            = 0;                    // init the timer
    T2CONbits.ON    = 1;                    // Turn on the timer
    
    //**************************************************************************
    //*******************  Timers to drive PWM Offset circuits  ****************
    //******************** T6 drives AWG and Analog In *************************
    //**************************************************************************
    T6CONbits.TCKPS = PWMPRESCALER;         // timer pre scaler
    PR6             = (PWMPERIOD - 1);      // match count
    TMR6            = 0;                    // init the timer
    T6CONbits.ON    = 1;                    // Turn on the timer

    //**************************************************************************
    //*******************  TMR 9 Abort Trigger  ********************************
    //**************************************************************************
    T9CONbits.TCKPS     = 0;                    // timer pre scaler; default to divide by 1
    PR9                 = (100 - 1);            // Default match on 100 counts
    TMR9                = 0;                    // Set timer value to zero

    //**************************************************************************
    //*******************  DMA  ************************************************
    //**************************************************************************
    DMACONbits.ON   = 1;    // turn on the DMA module
    
    //**************************************************************************
    //*******************  DCOUT 1 PWM6  ***************************************
    //**************************************************************************
    RPD2R               = 0b1100;           // map OC6 to RD2
    OC6CONbits.OCTSEL   = 0;                // T2 selection
    OC6CONbits.OCM      = 0b101;            // PWM Mode
    OC6R                = 0;                // go high on counter T2 wrap
    OC6RS               = PWMIDEALCENTER;   // set to midpoint
    OC6CONbits.ON       = 1;                // turn on the output compare  
    
    //**************************************************************************
    //*******************  DCOUT 2 PWM4  ***************************************
    //**************************************************************************
    RPC13R              = 0b1011;           // map OC4 to RC13
    OC4CONbits.OCTSEL   = 0;                // T2 selection
    OC4CONbits.OCM      = 0b101;            // PWM Mode
    OC4R                = 0;                // go high on counter T2 wrap
    OC4RS               = PWMIDEALCENTER;          // set to midpoint
    OC4CONbits.ON       = 1;                // turn on the output compare  

    //**************************************************************************
    //*******************  AWG/DAC PWM7  ***************************************
    //*******************  LA/DAC PWM7  ***************************************
    //**************************************************************************
    T7CON               = 0;                // initialize timer7
    OC7CON              = 0;                // AWG/DAC PWM7
    DCH7CON             = 0;                // clear the con reg
    DCH7ECON            = 0;                // clear the econ reg
    CNCONE              = 0;                // LA change notice off
    CNNEE               = 0;                // no negative edges
    CNENE               = 0;                // no positive edges

    // set up the R2R Ladder latch
    LATH                = DACDATA(511);     // set DAC output to mid range
    TRISH               = 0;                // set DAC port H to output
    
    // set up Timer 7
    T7CONbits.TCKPS     = AWGPRESCALER;         // timer prescaler
    TMR7                = 0;                    // clear timer value
    PR7                 = AWGMINTMRCNT-1;          // Now how often the DMA is triggered

    // setup the offset PWM, AWG only
    RPG0R               = 0b1100;           // map OC7 to RG0
    OC7CONbits.OCTSEL   = 0;                // T6 selection
    OC7CONbits.OCM      = 0b101;            // PWM Mode
    OC7R                = 0;                // go high on counter T6 wrap
    OC7RS               = PWMIDEALCENTER;   // set to midpoint
    OC7CONbits.ON       = 1;                // turn on the output compare  
    
    // DMA setup AWG/LA -> TMR7, PRI=3, CHAEN = 1, CSIZ=2, IRQEN=1
    // AWG LATH, 
    DCH7CONbits.CHPRI   = 3;                    // Give the DAC the highest priority
    DCH7CONbits.CHAEN   = 1;                    // auto enable, keep cycling on block transfer 
    DCH7ECONbits.CHSIRQ = _TIMER_7_VECTOR;      // event start IRQ Timer 7
    DCH7ECONbits.SIRQEN = 1;                    // enable start IRQ
    DCH7DSA             = KVA_2_PA(&LATH);      // Latch H address for destination
    DCH7DSIZ            = 2;                    // destination size 2 byte
    DCH7CSIZ            = 2;                    // cell transfer size 2 byte
    DCH7SSIZ            = 0;                    // init to zero
    
    //**************************************************************************
    //**************************************************************************
    //*******************  ADC 0/1  ********************************************
    //*******************  TMR3 OC5, DMA 3/4 ***********************************
    //**************************************************************************
    //**************************************************************************
        
    // set up the gain MUX select; set to max attenuation
    // IN1
    TRISAbits.TRISA2    = 0;
    LATAbits.LATA2      = 1;

    // IN2
    TRISAbits.TRISA3    = 0;
    LATAbits.LATA3      = 1;

    // set up the AIN1 offset PWM
    OC8CON              = 0;                // AIN1 PWM8
    RPD9R               = 0b1100;           // map OC8 to RD9
    OC8CONbits.OCTSEL   = 0;                // T6 selection
    OC8CONbits.OCM      = 0b101;            // PWM Mode
    OC8R                = 0;                // go high on counter T6 wrap
    OC8RS               = PWMIDEALCENTER;          // set to midpoint
    OC8CONbits.ON       = 1;                // turn on the output compare  
   
    // set up triggers; same time as DMA transfer, you will get the previous value
    // we must toss the first value!
    ADCTRG1bits.TRGSRC0 = 0b00110;      // Set trigger TMR3
    ADCTRG1bits.TRGSRC1 = 0b01010;      // Set trigger OC5

    // Enable ADC 0/1 interrupts
    ADCGIRQEN1bits.AGIEN0 = 1;          // ADC0 interrupt enable
    ADCGIRQEN1bits.AGIEN1 = 1;          // ADC1 interrupt enable 

    // timer 3 initialization
    T3CON               = 0;                    // clear the timer
    T3CONbits.TCKPS     = 0;                    // timer pre scaler; default to divide by 1 (0=/1, 1=/8, 2=/64, 3=/256)
    PR3                 = INTERLEAVEPR(32);     // Default match on 32 counts
    TMR3                = 0;                    // Set timer value to zero
    
    // set up output compare 5
    OC5CON              = 0;                    // clear the OC register
    OC5CONbits.OCTSEL   = 1;                    // Alt T3 selection
    OC5CONbits.OCM      = 0b101;                // continuous toggle
    OC5RS               = 0;                    // Doc say interrupt triggered on RS, that is wrong
    OC5R                = INTERLEAVEOC(32);     // Positive pulse, this seems to trigger the pulse
 
    // DMA 3
    DCH3CON             = 0;                    // clear the DMA control register
    DCH3ECON            = 0;                    // clear extended control register
    DCH3INT             = 0;                    // clear all interrupts
    DCH3CSIZ            = 2;                    // 2 bytes transferred per event (cell transfer)
    DCH3SSA             = KVA_2_PA(&ADCDATA0);  // transfer source physical address
    DCH3SSIZ            = 2;                    // source size 2 bytes
    DCH3DSA             = KVA_2_PA(rgOSC1Buff); // our input buffer
    DCH3DSIZ            = AINDMASIZE;          // 32K byte destination size
    DCH3CONbits.CHPRI   = 2;                    // run at priority 2                   
//    DCH3ECONbits.CHSIRQ = _TIMER_3_VECTOR;      // trigger on TMR3 
    DCH3ECONbits.CHSIRQ = _ADC_DATA0_VECTOR;    // ADC 0 completion
    DCH3ECONbits.SIRQEN = 1;                    // enable IRQ trigger
    DCH3CONbits.CHAEN   = 1;                    // continuous operations until an abort               
    DCH3ECONbits.CHAIRQ = _TIMER_9_VECTOR;      // abort defaults to TMR 9
    DCH3ECONbits.AIRQEN = 0;                    // abort trigger disable    
    IEC4bits.DMA3IE     = 0;                    // clear DMA interrupts 
    IFS4bits.DMA3IF     = 0;                    // clear DMA interrupt flag
   
    // DMA 4
    DCH4CON             = 0;                    // clear the DMA control register
    DCH4ECON            = 0;                    // clear extended control register
    DCH4INT             = 0;                    // clear all interrupts
    DCH4CSIZ            = 2;                    // 2 bytes transferred per event (cell transfer)
    DCH4SSA             = KVA_2_PA(&ADCDATA1);  // transfer source physical address
    DCH4SSIZ            = 2;                    // source size 2 bytes
    DCH4DSA             = DCH3DSA + DCH3DSIZ;   // The other half of the buffer
    DCH4DSIZ            = AINDMASIZE;          // 32K byte destination size 
    DCH4CONbits.CHPRI   = 2;                    // run at priority 2                   
//    DCH4ECONbits.CHSIRQ = _OUTPUT_COMPARE_5_VECTOR; // trigger on OC5
    DCH4ECONbits.CHSIRQ = _ADC_DATA1_VECTOR;    // ADC 1 completion
    DCH4ECONbits.SIRQEN = 1;                    // enable IRQ trigger
    DCH4CONbits.CHAEN   = 1;                    // continuous operations until an abort               
    DCH4ECONbits.CHAIRQ = _TIMER_9_VECTOR;      // abort defaults to TMR 9
    DCH4ECONbits.AIRQEN = 0;                    // abort trigger disable    
    IEC4bits.DMA4IE     = 0;                    // clear DMA interrupts 
    IFS4bits.DMA4IF     = 0;                    // clear DMA interrupt flag
   
    //**************************************************************************
    //**************************************************************************
    //*******************  ADC 2/3  ********************************************
    //*******************  TMR5 OC1, DMA 5/6 ***********************************
    //**************************************************************************
    //**************************************************************************
    
    // set up the gain MUX select; set to max attenuation
    // IN1
    TRISAbits.TRISA6    = 0;
    LATAbits.LATA6      = 1;

    // IN2
    TRISAbits.TRISA7    = 0;
    LATAbits.LATA7      = 1;

    // set up the AIN1 offset PWM
    OC9CON              = 0;                // AIN2 PWM9
    RPC2R               = 0b1101;           // map OC9 to RC2
    OC9CONbits.OCTSEL   = 0;                // T6 selection
    OC9CONbits.OCM      = 0b101;            // PWM Mode
    OC9R                = 0;                // go high on counter T6 wrap
    OC9RS               = PWMIDEALCENTER;          // set to midpoint
    OC9CONbits.ON       = 1;                // turn on the output compare   

    // set up triggers; same time as DMA transfer, you will get the previous value
    // we must toss the first value!
    ADCTRG1bits.TRGSRC2 = 0b00111;              // set trigger TMR5
    ADCTRG1bits.TRGSRC3 = 0b01000;              // Set trigger OC1
 
    // turn on interrupts for ADC 2/3
    ADCGIRQEN1bits.AGIEN2 = 1;                  // ADC2 interrupt enable
    ADCGIRQEN1bits.AGIEN3 = 1;                  // ADC3 interrupt enable

    // timer 5 initialization
    T5CON               = 0;                    // clear the timer
    T5CONbits.TCKPS     = 0;                    // timer pre scaler; default to divide by 1 (0=/1, 1=/8, 2=/64, 3=/256)
    PR5                 = INTERLEAVEPR(32);     // Default match on 32 counts
    TMR5                = 0;                    // Set timer value to zero
    
    // set up output compare 1
    OC1CON              = 0;                    // clear the OC register
    OC1CONbits.OCTSEL   = 1;                    // Alt T5 selection
    OC1CONbits.OCM      = 0b101;                // continuous toggle
    OC1RS               = 0;                    // Doc say interrupt triggered on RS, that is wrong
    OC1R                = INTERLEAVEOC(32);     // Positive pulse, this seems to trigger the pulse
    
    // DMA 5
    DCH5CON             = 0;                    // clear the DMA control register
    DCH5ECON            = 0;                    // clear extended control register
    DCH5INT             = 0;                    // clear all interrupts
    DCH5CSIZ            = 2;                    // 2 bytes transferred per event (cell transfer)
    DCH5SSA             = KVA_2_PA(&ADCDATA2);  // transfer source physical address
    DCH5SSIZ            = 2;                    // source size 2 bytes
    DCH5DSA             = KVA_2_PA(rgOSC2Buff); // our input buffer
    DCH5DSIZ            = AINDMASIZE;          // 32K byte destination size   
    DCH5CONbits.CHPRI   = 2;                    // run at priority 2                   
//    DCH5ECONbits.CHSIRQ = _TIMER_5_VECTOR;      // trigger on TMR5 
    DCH5ECONbits.CHSIRQ = _ADC_DATA2_VECTOR;    // ADC 2 completion 
    DCH5ECONbits.SIRQEN = 1;                    // enable IRQ trigger
    DCH5CONbits.CHAEN   = 1;                    // continuous operations until an abort               
    DCH5ECONbits.CHAIRQ = _TIMER_9_VECTOR;      // abort defaults to TMR 9
    DCH5ECONbits.AIRQEN = 0;                    // abort trigger disable    
    IEC4bits.DMA5IE     = 0;                    // clear DMA interrupts 
    IFS4bits.DMA5IF     = 0;                    // clear DMA interrupt flag
   
    // DMA 6
    DCH6CON             = 0;                    // clear the DMA control register
    DCH6ECON            = 0;                    // clear extended control register
    DCH6INT             = 0;                    // clear all interrupts
    DCH6CSIZ            = 2;                    // 2 bytes transferred per event (cell transfer)
    DCH6SSA             = KVA_2_PA(&ADCDATA3);  // transfer source physical address
    DCH6SSIZ            = 2;                    // source size 2 bytes
    DCH6DSA             = DCH5DSA + DCH5DSIZ;   // The other half of the buffer
    DCH6DSIZ            = AINDMASIZE;          // 32K byte destination size  
    DCH6CONbits.CHPRI   = 2;                    // run at priority 2                   
//    DCH6ECONbits.CHSIRQ = _OUTPUT_COMPARE_1_VECTOR; // trigger on OC1
    DCH6ECONbits.CHSIRQ = _ADC_DATA3_VECTOR;    // ADC 3 completion
    DCH6ECONbits.SIRQEN = 1;                    // enable IRQ trigger
    DCH6CONbits.CHAEN   = 1;                    // continuous operations until an abort               
    DCH6ECONbits.CHAIRQ = _TIMER_9_VECTOR;      // abort defaults to TMR 9
    DCH6ECONbits.AIRQEN = 0;                    // abort trigger disable    
    IEC4bits.DMA6IE     = 0;                    // clear DMA interrupts 
    IFS4bits.DMA6IF     = 0;                    // clear DMA interrupt flag
   
    //*************************************************************************
    //**************************************************************************
    //*******************  Trigger  ********************************************
    //*******************  TMR9 Digital Compare 1/2, 3/4 ***********************
    //**************************************************************************
    //**************************************************************************
    IPC10bits.T9IP      = 7;
    IPC10bits.T9IS      = 0;
            
    IPC11bits.ADCDC2IP = 6;
    IPC11bits.ADCDC2IS = 0;

    IPC30bits.CNEIP    = 6;
    IPC30bits.CNEIS    = 0;

    IPC11bits.ADCDC1IP = 5;
    IPC11bits.ADCDC1IS = 0;
    
	// enable interrupts
    asm volatile("ei    %0" : "=r"(val));
    
    return(Idle);
}

#define MACWAITTIME 10000            // how long to wait for the MAC address to be set

STATE CFGSysInit(void)
{
    static STATE state = INITAdaptor;
    static uint32_t tStart = 0;
    STATE retState = Idle;

    switch (state) 
    {
        case INITAdaptor:
            
            if(deIPcK.deIPInit())
            {
                tStart = SYSGetMilliSecond();
                state = INITWaitForMacAddress;
            }
            else 
            {
                // hard error, just get stuck at this state
                state = INITUnableToSetNetworkAdaptor;
            }
            break;

        case INITWaitForMacAddress:              
            if(deIPcK.getMyMac(macOpenScope))
            {
                state = INITMRFVer;
            }
            else if((SYSGetMilliSecond() - tStart) > MACWAITTIME)
            {
                // Hard error, just get stuck at this state
                state = INITMACFailedToResolve;
            }
            break;

        case INITMRFVer:
            WF_DeviceInfoGet(&myMRFDeviceInfo);
            state = INITWebServer;
            break;

        // this should not fail
        case INITWebServer:
            if ((retState = HTTPSetup()) == Idle) {
                state = INITInitFileSystem;
            }
            break;

        case INITInitFileSystem:
            if ((retState = CFGOpenVol()) == Idle) {
                Serial.println("File Systems Initialized");
                state = Done;
            }
            else if(IsStateAnError(retState))
            {
                Serial.print("Unable to initialize the file systems. Error 0x");
                Serial.println(retState, 16);
                state = retState;
            }
            break;        
            
        case Done:
            return(Idle);
            break;

        default:
            break;
    }

    return(state);
}



