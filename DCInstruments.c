/************************************************************************/
/*                                                                      */
/*    DCInstruments.C                                                   */
/*                                                                      */
/*    Calibration file for the DC Outs                                  */
/*    It also contains the reference voltage identification             */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    2/22/2016 (KeithV): Created                                        */
/************************************************************************/
#include <OpenScope.h>

#define ADC_FB_AVERAGING    32

// make sure (300000ul/ADC_FB_AVERAGING) is an integer 300000/32 = 9375
#define ADCuV(Vadc)         10ul * ((((Vadc) * (300000ul/ADC_FB_AVERAGING)) + 2048ul) / 4096ul) 


STATE InitSystemVoltages(void)
{
    static STATE stateVOLT = Idle;

    switch(stateVOLT)
    {
        case Idle:
            stateVOLT = VOLTUSB;
            // fall thru

        case VOLTUSB:
            if(FBUSB5V0uV(&uVUSB) == Idle)
            {
                stateVOLT = VOLT3V3;
            }
            break;
            
        case VOLT3V3:
            if(FBVCC3V3uV(&uV3V3) == Idle)
            {
                stateVOLT = VOLT3V0;
            }
            break;

        case VOLT3V0:
            if(FBREF3V0uV(&uVRef3V0) == Idle)
            {
                stateVOLT = VOLT1V5;
            }
            break;

        case VOLT1V5:
            if(FBREF1V5uV(&uVRef1V5) == Idle)
            {
                stateVOLT = VOLTNUSB;
            }
            break;

        case VOLTNUSB:
            if(FBNUSB5V0uV(&uVNUSB) == Idle)
            {
                stateVOLT = Idle;
            }
            break;
    }  
    
    return(stateVOLT);  
}


// uVDC = -40000*Dpwm + 7000000  --- ideally
STATE DCCalibrate(HINSTR hDCVolt) 
{
    DCVOLT * pDCVOLT = (DCVOLT *) hDCVolt;
    
    if(hDCVolt == NULL)
    {
        return(STATEError);
    }

    if(!(pDCVOLT->comhdr.activeFunc == DCFnCal || pDCVOLT->comhdr.activeFunc == SMFnNone))
    {
        return(Waiting);   
    }
    
    switch(pDCVOLT->comhdr.state)
    {
        case Idle:
            pDCVOLT->comhdr.activeFunc  = DCFnCal;
            pDCVOLT->pOCdc->OCxRS       = 100;          // set to +3v
            pDCVOLT->comhdr.tStart      = SYSGetMilliSecond();
            pDCVOLT->comhdr.state       = DCWaitHigh;
            break;
            
        case DCWaitHigh:
            if(SYSGetMilliSecond() - pDCVOLT->comhdr.tStart >= PWM_SETTLING_TIME)
            {
                pDCVOLT->comhdr.state     = DCReadHigh;    
            }
            break;

        case DCReadHigh:
            if(FBAWGorDCuV(pDCVOLT->channelFB, ((int32_t *) &(pDCVOLT->A))) == Idle)
            {
                pDCVOLT->pOCdc->OCxRS     = 250;          // set to -3V
                pDCVOLT->comhdr.tStart    = SYSGetMilliSecond();
                pDCVOLT->comhdr.state     = DCWaitLow;    
            }
            break;
            
        case DCWaitLow:
            if(SYSGetMilliSecond() - pDCVOLT->comhdr.tStart >= PWM_SETTLING_TIME)
            {
                pDCVOLT->comhdr.state           = DCReadLow;    
            }
            break;
            
        case DCReadLow:
            if(FBAWGorDCuV(pDCVOLT->channelFB, ((int32_t *) &(pDCVOLT->B))) == Idle)
            {
                int32_t D1 = pDCVOLT->A; 
                int32_t D2 = pDCVOLT->B;

                // B==7000000, A==40000  -- ideally
                *((int32_t *) &(pDCVOLT->A))    = ((D1 - D2) + 75l) / 150l;     // remove scaling in D1, D2  
                *((int32_t *) &(pDCVOLT->B))    = D1 + pDCVOLT->A * 100l;       // remove scaling from D1
            
                pDCVOLT->comhdr.idhdr.cfg       = CFGCAL;
                pDCVOLT->comhdr.state           = Idle;    
                pDCVOLT->comhdr.activeFunc      = SMFnNone;    
            }
            break;
            
        default:
            pDCVOLT->comhdr.state           = Idle;
            pDCVOLT->comhdr.activeFunc      = SMFnNone;
            return(STATEError);
    }
    
    return(pDCVOLT->comhdr.state);  
}

STATE DCSetVoltage(HINSTR hDCVolt, int32_t mvDCout) 
{
    DCVOLT *    pDCVOLT = (DCVOLT *) hDCVolt;
    
    if(hDCVolt == NULL)
    {
        return(STATEError);
    }

    if(!( (pDCVOLT->comhdr.activeFunc == DCFnSet && pDCVOLT->mvDCout == mvDCout)  || pDCVOLT->comhdr.activeFunc == SMFnNone))
    {
        return(Waiting);   
    }

    switch(pDCVOLT->comhdr.state)
    {
        case Idle:
            pDCVOLT->comhdr.activeFunc    = DCFnSet;
            pDCVOLT->mvDCout            = mvDCout;
                
            // uVout = -A * Dpwm + B
            // A * Dpwm = B - uVout
            // Dpwm = (B -uVout) / A
            pDCVOLT->pOCdc->OCxRS = (uint16_t) (((pDCVOLT->B - (1000l * mvDCout)) + (pDCVOLT->A/2)) / pDCVOLT->A);
            
            // set the timer
            pDCVOLT->comhdr.tStart      = SYSGetMilliSecond();
            pDCVOLT->comhdr.state       = DCSetWait;

            break;
            
        case DCSetWait:
            if(SYSGetMilliSecond() - pDCVOLT->comhdr.tStart >= PWM_SETTLING_TIME)
            {
                // just right, get out
                pDCVOLT->comhdr.state       = Idle;
                pDCVOLT->comhdr.activeFunc  = SMFnNone;                
            }
            break;
            
        default:
            pDCVOLT->comhdr.state       = Idle;
            pDCVOLT->comhdr.activeFunc  = SMFnNone;
            return(STATEError);
    } 
    
     return(pDCVOLT->comhdr.state);     
}
