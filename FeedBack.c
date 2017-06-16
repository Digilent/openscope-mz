/************************************************************************/
/*                                                                      */
/*    FeedBack.C                                                        */
/*                                                                      */
/*    Read the Feedback circuits                                        */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2017, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    5/22/2017 (KeithV): Created                                        */
/************************************************************************/
#include <OpenScope.h>


/* ------------------------------------------------------------ */
/***	convertADCs
**
**	Parameters:
**		channelNumber - The PIC32 analog channel number as in the PIC32 datasheet
**
**	Return Value:
**      The converted value for that channel
**
**	Errors:
**     If return value of zero and error may have occured
**
**	Description:
**      Coverts the analog signal to a digital value on the 
**      given pic32 analog channel number
*/
static int convertADCs(uint8_t channelNumber)
{ 
    uint8_t vcn = channelNumber;        // assume our vitual channel number is the real one
    uint32_t adcTRGmode = ADCTRGMODE;   // save trigger mode

    // see if we are using alternate inputs
    switch(vcn)
    {
        case 43:
        case 44:
        case 50:
            return(0);
            break;

        case 45:
            ADCTRGMODEbits.SH0ALT = 1;
            vcn -= 45;
            break;

        case 46:
            ADCTRGMODEbits.SH1ALT = 1;
            vcn -= 45;
            break;

        case 47:
            ADCTRGMODEbits.SH2ALT = 1;
            vcn -= 45;
            break;

        case 48:
            ADCTRGMODEbits.SH3ALT = 1;
            vcn -= 45;
            break;

        case 49:
            ADCTRGMODEbits.SH4ALT = 1;
            vcn -= 45;
            break;

        default:
            break;
    }

    ADCCON3bits.ADINSEL   = vcn;            // say which channel to manually trigger
    ADCCON3bits.RQCNVRT  = 1;               // manually trigger it.

    // wait for completion of the conversion
    if(vcn < 32)
    {
        uint32_t mask = 0x1 << vcn;
        while((ADCDSTAT1 & mask) == 0);
    }
    else
    {
        uint32_t mask = 0x1 << (vcn - 32);
        while((ADCDSTAT2 & mask) == 0);
    }

    // return the trigger mode to what it was
    ADCTRGMODE = adcTRGmode;

    // return the converted data
    return((int) ((uint32_t *) &ADCDATA0)[vcn]);
}

/* ------------------------------------------------------------ */
/***	ReadFeedBackADC
**
**	Parameters:
**		channelNumber - The PIC32 analog channel number as in the PIC32 datasheet
**
**	Return Value:
**      The converted value for that channel
**
**	Errors:
**     If return value of zero and error may have occured
**
**	Description:
**      Converts the analog signal to a digital value on the 
**      given pic32 analog channel number. The converter must
**      a class 1 or 2 ADC, that is ADC 0 - 31
*/
static int32_t SumFeedBackADC(uint8_t channelNumber, uint8_t cSum)
{
    uint32_t i = 0;
    int32_t value = 0;

    // do an initial conversion to prime the ADC
    // this is a bug with the MZ ADC
    convertADCs(channelNumber);
        
    for(i=0; i<cSum; i++)
    {
        value += convertADCs(channelNumber);
    }

    return(value);
}

static STATE AverageADCInSoftware(uint8_t channelNumber, uint8_t pwr2Ave, int32_t * pResult)
{
    int32_t cSum    = 1<<pwr2Ave;
    int32_t sum     = SumFeedBackADC(channelNumber, cSum);
    bool    fNeg    = (sum < 0);

    if(fNeg) sum *= -1;

    if(pwr2Ave != 0) 
        {
            if((sum >> (pwr2Ave-1)) & 0x1) sum = (sum >> pwr2Ave) + 1;
            else                           sum = (sum >> pwr2Ave);
        }

    if(fNeg)    *pResult = -sum;
    else        *pResult = sum;

    return(Idle);
}

STATE AverageADC(uint8_t channelNumber, uint8_t pwr2Ave, int32_t * pResult)
{
    static uint8_t curChannel = 0xFF;
    uint32_t volatile __attribute__((unused)) flushADC;

    if(channelNumber != curChannel)
    {
        if(channelNumber >= ADCALT)
        {
            return(InvalidChannel);
        }
        else if(curChannel == 0xFF)
        {
            curChannel = channelNumber;
        }
        else
        {
            return(Waiting);
        }
    }

    // have to do the averaging in software
    if(curChannel >= ADCCLASS2)
    {
        curChannel = 0xFF;
        return(AverageADCInSoftware(channelNumber, pwr2Ave, pResult));
    }

    if(ADCFLTR1bits.AFEN == 0)
    {
        ADCFLTR1 = 0;

        // read the ADC to flush any interrupts
        flushADC = (&ADCDATA0)[curChannel];

        ADCFLTR1bits.DFMODE = 1;                        // put in averaging mode

        ADCFLTR1bits.OVRSAM = 0b01;                     // 4 samples       
        ADCFLTR1bits.CHNLID = channelNumber;            // the channel number
        IFS1CLR = _IFS1_ADCDF1IF_MASK;
        IEC1CLR = _IEC1_ADCDF1IE_MASK;
        ADCFLTR1bits.AFGIEN = 1;                        // Enable the interrupt (set the IF flag)
        ADCFLTR1bits.AFEN = 1;                          // turn on the digital filter
        ADCCON3bits.ADINSEL   = channelNumber;          // say which channel to manually trigger
        ADCCON3bits.RQCNVRT  = 1;                       // manually trigger it.

        // wait for the IF flag to get set, this will take about 8us
        while(!IFS1bits.ADCDF1IF);

        // now set up for the real conversion.
        ADCFLTR1bits.OVRSAM = (pwr2Ave-1) & 0b111;      // How many extra bits to oversample        
        IFS1CLR = _IFS1_ADCDF1IF_MASK;
        ADCCON3bits.RQCNVRT  = 1;                       // manually trigger it.
    }

    else if(IFS1bits.ADCDF1IF)
    {
        *pResult = (int32_t) ((int16_t) ADCFLTR1bits.FLTRDATA);
        ADCFLTR1 = 0;
        flushADC = (&ADCDATA0)[curChannel];  // flush any pending interrupts
        curChannel = 0xFF;
        return(Idle);
    }

    return(Acquiring);
}

STATE FBAWGorDCuV(uint32_t channelFB, int32_t * puVolts) 
{

    int32_t osValue;

    STATE curState = AverageADC(channelFB, 6, &osValue);

    if(curState == Idle) 
    {
        *puVolts = (int32_t) ((595000000ll * osValue - 409600ll * uVRef3V0 + 120832) / 241664);
    }

    return(curState);
}

STATE FBUSB5V0uV(uint32_t * puVolts) 
{
    int32_t osValue;

    STATE curState = AverageADC(CH_USB5V0_FB, 6, &osValue);

    if(curState == Idle) 
    {
        *puVolts = (uint32_t) ((6703125ll * osValue + 1376) / 2752);
    }

    return(curState);
}

STATE FBNUSB5V0uV(uint32_t * puVolts) 
{
    int32_t osValue;

    STATE curState = AverageADC(CH_VSS5V0_FB, 6, &osValue);

    if(curState == Idle) 
    {
        *puVolts = (uint32_t) ((uVRef3V0 * 4000ll - 3609375ll * osValue + 464) / 928);
    }

    return(curState);
}

STATE FBREF1V5uV(uint32_t * puVolts) 
{
    int32_t osValue;

    STATE curState = AverageADC(CH_VREF1V5_FB, 6, &osValue);

    if(curState == Idle) 
    {
        *puVolts = (uint32_t) ((46875l * osValue + 32)/64);
    }

    return(curState);
}

STATE FBREF3V0uV(uint32_t * puVolts) 
{
    int32_t osValue;

    STATE curState = AverageADC(CH_VREF3V0_FB, 6, &osValue);

    if(curState == Idle) 
    {
        *puVolts = (uint32_t) ((46875l * osValue + 16)/32);
    }

    return(curState);
}

STATE FBVCC3V3uV(uint32_t * puVolts) 
{
    int32_t osValue;

    STATE curState = AverageADC(CH_VCC3V3_FB, 6, &osValue);

    if(curState == Idle) 
    {
        *puVolts = (uint32_t) ((1140625ll * osValue + 352)/704);
    }

    return(curState);
}
