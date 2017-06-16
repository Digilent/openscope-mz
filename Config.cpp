/************************************************************************/
/*                                                                      */
/*    Config.cpp                                                        */
/*                                                                      */
/*    Stores, saves, maintains configuration data                       */
/*    Such as calibration data, network parameters and the like         */
/*                                                                      */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    5/10/2016 (KeithV): Created                                       */
/************************************************************************/
#include <OpenScope.h>

STATE CFGOpenVol(void)
{
    FRESULT fr = FR_OK;

    // See if the drive is not ready, and if not mount it
    if(!DFATFS::fsvolmounted(DFATFS::szFatFsVols[VOLFLASH]))
    {
        if((fr = DFATFS::fsmount(flashVol, DFATFS::szFatFsVols[VOLFLASH], 1)) == FR_NO_FILESYSTEM)
        {
            // if we can't mount because there is no filesystem, create one.
            if((fr = DFATFS::fsmkfs(flashVol)) == FR_OK)
            {
                return(CFGCheckingFileSystems);
            }
            else if(fr != FR_OK)
            {
                return(CFGFileSystemError | fr);
            }
        }
        else if(fr != FR_OK)
        {
            return(CFGMountError | fr);
        }
        else
        {
            return(CFGCheckingFileSystems);
        }
    }

    // flash filesystem is created and mounted
    else 
    {
        return(Idle);
    }

    // really should never get here
    ASSERT(NEVER_SHOULD_GET_HERE);
    return(Idle);
}

STATE CFGSdHotSwapTask(void)
{
    static STATE state = Idle;
    static uint32_t tStart = 0;
    FRESULT fr = FR_OK;

    // We can not do this while running the logic analyzer, we can do no IO
    if(fBlockIOBus)
    {

        // check if we are waiting without using the IO bus
        if(state == Waiting && SYSGetMilliSecond() - tStart >= SDWAITTIME)
        {
            // hold this is a completed state
            // so when we come back we will complete the SD card mount
            tStart = SYSGetMilliSecond() - SDWAITTIME;
        }
    }

    // normal operations
    else
    {
        switch(state)
        {
            case Idle:
 
                // if we see a low on the pin
                // something is stuffed in the SD card
                if(!GetGPIO(PIN_SD_DET))
                {
                    state = Waiting;
                    tStart = SYSGetMilliSecond();
    
                }
                break;

            case Waiting:
                // if the SD card is still in after our wait time, mount it
                if(SYSGetMilliSecond() - tStart >= SDWAITTIME && !GetGPIO(PIN_SD_DET))
                {
                    // if it is mounted, unmount it.
                    if(DFATFS::fsvolmounted(DFATFS::szFatFsVols[VOLSD])) DFATFS::fsunmount(DFATFS::szFatFsVols[VOLSD]);

                    // now mount the volume, don't care if succeeds, just try
                    if((fr = DFATFS::fsmount (dSDVol, DFATFS::szFatFsVols[VOLSD], 1)) == FR_OK)
                    {
                        Serial.println("SD card detected and mounted");
                    }
                    else
                    {
                        Serial.print("SD card detected but unable to mount. FR = ");
                        Serial.println(fr, 10);
                    }

                    state = CFGUSBInserted;
                }
                break;

            case CFGUSBInserted:

                // if someone pulls the USB card out, unmount it
                if(GetGPIO(PIN_SD_DET))
                {
                    DFATFS::fsunmount(DFATFS::szFatFsVols[VOLSD]);

                    // SD card removed
                    Serial.println("SD card removed");

                    state = Idle;
                }
                break;

            default:
                ASSERT(NEVER_SHOULD_GET_HERE);                      // we should never get here
                break;
        }
    }
    
    return(Idle);
}

// file name is DEVICE_ORG_MAC_SUFFIX
uint32_t CFGCreateFileName(const IDHDR& idHDR, char const * const szSuffix, char * sz, uint32_t cb)
{
    #define cbOther (sizeof(szCFG) + 13)                                                    // sizeof(czCFG) includes null terminator, 12 char for Mac, 1 char for _ about 19 char
    uint32_t cbDevice       = (idHDR.id != NULL_ID) ? strlen(rgszInstr[idHDR.id]) : 0;      // Instrument... OSC1, AWG1, usually 4-5 char long 
    uint32_t cbSuffix       = (szSuffix != NULL) ? strlen(szSuffix) + 1 : 0;                // Any suffix string + leading _
    uint32_t cbStr          = 0;             
    int32_t  i              = 0;
        
    // Is the provided string long enough?
    if(cb < (cbDevice + cbSuffix + 14)) // everything + 12 char for MAC + leading _ + null terminator = + 14
    {
        return(0);
    }
     
    // file name is DEVICE_ORG_MAC_SUFFIX
    // exp AWG1_USER_23F623985764_SUFFIX

    // DEVICE
    if(cbDevice > 0)
    {
        memcpy(sz, rgszInstr[idHDR.id], cbDevice);
        cbStr += cbDevice;
    }

    // DEVICE_ORG_MAC
    sz[cbStr++]  = '_';          // add _
    for(i=0; i<6; i++)
    {
        uint32_t inc = 2;

        if(idHDR.mac.u8[i] < 16) 
        {
            sz[cbStr++] = '0';
            inc--;
        }

        itoa(idHDR.mac.u8[i], &sz[cbStr], 16);
        cbStr += inc;      
    }

    // DEVICE_ORG_MAC_SUFFIX
    if(cbSuffix > 0)
    {
        cbSuffix--;
        sz[cbStr++]  = '_';          // add _
        memcpy(&sz[cbStr], szSuffix, cbSuffix);
        cbStr += cbSuffix;
    }

    // put in null terminator.
    sz[cbStr] = '\0';

    return(cbStr);
}

STATE CFGCalibrateInstruments(INSTRGRP& instrGrp) 
{
    STATE       retState    = Idle;

    switch (instrGrp.state) 
    {

        case Idle:
            {
                if(instrGrp.chInstr == 0)
                {
                    return(CFGNoInstrumentsInGroup);
                }

                instrGrp.tStart = SYSGetMilliSecond();
                for(instrGrp.iInstr=0; instrGrp.iInstr<instrGrp.chInstr; instrGrp.iInstr++)
                {
                    if(instrGrp.rgUsage[instrGrp.iInstr] == instrIDCal)
                    {
                        memcpy((void *) ((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->mac.u8,  macOpenScope.u8, sizeof(MACADDR));
                    }
                }

                instrGrp.iInstr = 0;
                instrGrp.state = CFGCal;
            }
            break;

        case CFGCal:
            if(instrGrp.rgUsage[instrGrp.iInstr] == instrIDCal)
            {
                switch(((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->id)
                {
                    case DCVOLT1_ID:
                    case DCVOLT2_ID:
                        if((retState = DCCalibrate(instrGrp.rghInstr[instrGrp.iInstr])) == Idle)
                        {
                            instrGrp.state = CFGCalNext;
                        }
                        else if(IsStateAnError(retState))
                        {
                            instrGrp.state = Idle;
                            return(retState);
                        }
                        break;

                    case AWG1_ID:
                        if((retState = AWGCalibrate(instrGrp.rghInstr[instrGrp.iInstr])) == Idle)
                        {
                            instrGrp.state = CFGCalNext;
                        }
                        else if(IsStateAnError(retState))
                        {
                            instrGrp.state = Idle;
                            return(retState);
                        }
                        break;

                    case OSC1_ID:
                    case OSC2_ID:
                        if((retState = OSCCalibrate(instrGrp.rghInstr[instrGrp.iInstr], instrGrp.rghInstr[instrGrp.iInstr+1])) == Idle)
                        {
                            instrGrp.state = CFGCalNext;
                        }
                        else if(IsStateAnError(retState))
                        {
                            instrGrp.state = Idle;
                            return(retState);
                        }
                        break;

                    default:
                        instrGrp.state = CFGCalNext;
                        break;
                }
            }
            else
            {
                instrGrp.state = CFGCalNext;
            }
            break;

        case CFGCalNext:
            instrGrp.iInstr++;
            if(instrGrp.iInstr >= instrGrp.chInstr)
            {
                instrGrp.state = CFGCalTime;
            }
            else
            {
                instrGrp.state = CFGCal;
            }
            break;

        case CFGCalTime:
            instrGrp.tStart = SYSGetMilliSecond() - instrGrp.tStart;
            instrGrp.state = Done;
            break;

        default:
        case Done:
            instrGrp.state = Idle;
            break;
    }

    return (instrGrp.state);
}


STATE CFGSaveCalibration(INSTRGRP& instrGrp, VOLTYPE const vol, CFGNAME const cfgName) 
{
    static char sz[128];        // should be less than 32, but we must have the memory
    STATE       retState    = Idle;

    switch (instrGrp.state) 
    {

        case Idle:
            instrGrp.iInstr = 0;
            // fall thru

        case CFGCalCheckInstr:
            instrGrp.state = CFGCalSaveNext;
            if(instrGrp.rgUsage[instrGrp.iInstr] == instrIDCal)
            {
                CFGNAME cfgNameCur = ((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->cfg;

                if(cfgNameCur == CFGUNCAL || cfgNameCur == CFGNONE || cfgNameCur == CFGEND )
                {
                    instrGrp.state = Idle;
                    return(CFGInstrumentNotCalibrated);
                }

                switch(((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->id)
                {
                    case OSC1_ID:
                    case OSC2_ID:
                    case DCVOLT1_ID:
                    case DCVOLT2_ID:
                    case AWG1_ID:
                        ((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->cfg = cfgName;
                        CFGCreateFileName(*((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr]), NULL, sz, sizeof(sz));
                        instrGrp.state = CFGCalSave;
                        break;

                    default:
                       break;
                }
            }
           break;
            
        case CFGCalSave:
            if((retState = IOWriteFile(instrGrp.dFile, vol, sz, *((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr]))) == Idle)
            {
                instrGrp.state = CFGCalSaveNext;
            }
            else if(IsStateAnError(retState))
            {
                instrGrp.state = Idle;
                return(retState);
            }
            break;

        case CFGCalSaveNext:
            instrGrp.iInstr++;
            if(instrGrp.iInstr >= instrGrp.chInstr)
            {
                instrGrp.state = Done;
            }
            else
            {
                instrGrp.state = CFGCalCheckInstr;
            }
            break;

        default:
        case Done:
            instrGrp.state = Idle;
            break;
    }

    return (instrGrp.state);
}

#if DEAD
STATE CFGPrintCalibrationInformation(INSTRGRP& instrGrp) 
{

    switch (instrGrp.state) 
    {

        case Idle:
            if(instrGrp.chInstr == 0)
            {
                return(CFGNoInstrumentsInGroup);
            }
            instrGrp.iInstr = 0;
            instrGrp.state = CFGCalPrint;
            // fall thru

        case CFGCalPrint:

            if(instrGrp.rgUsage[instrGrp.iInstr] == instrIDCal)
            {
                switch(((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->id)
                {
                    case DCVOLT1_ID:
                    case DCVOLT2_ID:
                        Serial.println("DC Instrument Print not implemented");
                        instrGrp.state = CFGCalPrintNext;
                        break;

                    case AWG1_ID:
                        Serial.println("AWG Instrument Print not implemented");
                        instrGrp.state = CFGCalPrintNext;
                        break;

                    case OSC1_ID:
                    case OSC2_ID:
                        instrGrp.state = CFGOSCPrtCalInit;
                        break;

                    default:
                        instrGrp.state = CFGCalPrintNext;
                        break;
                }
            }
            else
            {
                instrGrp.state = CFGCalPrintNext;
            }
            break;

        case CFGCalPrintNext:
            instrGrp.iInstr++;
            if(instrGrp.iInstr >= instrGrp.chInstr)
            {
                instrGrp.state = Done;
            }
            else
            {
                instrGrp.state = CFGCalPrint;
            }
            break;

        case CFGOSCPrtCalInit:
            {
                OSC& osc = *((OSC *) instrGrp.rghInstr[instrGrp.iInstr]);
                osc.comhdr.tStart = 0;     // we are using this as a count variable

                // uVin = A(Dadc) + B(PWM) - C
                Serial.print(rgszInstr[osc.comhdr.idhdr.id]);
                Serial.println(" calibration constants");
                Serial.println("uVin = (A)(Dadc) + (B)(pwm) - (C)");

                instrGrp.state = CFGOSCPrtCal;
            }
            break;

        case CFGOSCPrtCal:
            {
                OSC oscTemplate = OSC(OSC1_ID);
                OSC& osc = *((OSC *) instrGrp.rghInstr[instrGrp.iInstr]);
                char sz[256];

                float idealA = (float) oscTemplate.rgGCal[osc.comhdr.tStart].A;
                float idealB = (float) oscTemplate.rgGCal[osc.comhdr.tStart].B;
                float idealC = (float) oscTemplate.rgGCal[osc.comhdr.tStart].C;

                float actualA = (float) osc.rgGCal[osc.comhdr.tStart].A;
                float actualB = (float) osc.rgGCal[osc.comhdr.tStart].B;
                float actualC = (float) osc.rgGCal[osc.comhdr.tStart].C;

                Serial.print("Parameters for Gx = ");
                Serial.println(osc.comhdr.tStart + 1);
                sprintf(sz, "A: Actual: %8d,     Ideal: %8d,     %%Error %+7.2f", (int) osc.rgGCal[osc.comhdr.tStart].A, (int) oscTemplate.rgGCal[osc.comhdr.tStart].A, (idealA - actualA) * 100 / idealA);
                Serial.println(sz);
                sprintf(sz, "B: Actual: %8d,     Ideal: %8d,     %%Error %+7.2f", (int) osc.rgGCal[osc.comhdr.tStart].B, (int) oscTemplate.rgGCal[osc.comhdr.tStart].B, (idealB - actualB) * 100 / idealB);
                Serial.println(sz);
                sprintf(sz, "C: Actual: %8d,     Ideal: %8d,     %%Error %+7.2f", (int) osc.rgGCal[osc.comhdr.tStart].C, (int) oscTemplate.rgGCal[osc.comhdr.tStart].C, (idealC - actualC) * 100 / idealC);
                Serial.println(sz);
                Serial.println();

                osc.comhdr.tStart++;

                if(osc.comhdr.tStart >= NbrOfADCGains) 
                {
                    Serial.println();
                    osc.comhdr.tStart = 0;
                    instrGrp.state = CFGCalPrintNext;
                }
            }
            break;

        case CFGAWGPrtCalIdeal:
        {
            int i;

            Serial.println("DAC Ideal:");

            for (i = 0; i < SWDACSIZE; i++) 
            {
                int32_t ideal = DACIDEAL(i);

                Serial.print(i);
                Serial.print(": \t");

                Serial.print("Internal value: ");
//                Serial.print(SCALEDDACIDEAL(i));
                Serial.print(DACIDEAL(i));

                Serial.print("; \tVoltage: ");
                Serial.print(ideal);

                Serial.println("mv");
            }

        }
            instrGrp.state = CFGAWGPrtCalStart;
            //                CState = CDone;
            break;

        case CFGAWGPrtCalStart:
            ((AWG *) rgInstr[AWG1_ID])->t1 = 0;
            LATH = ((AWG *) rgInstr[AWG1_ID])->dacMap[0];
            ((AWG *) rgInstr[AWG1_ID])->comhdr.tStart = SYSGetMilliSecond();
            instrGrp.state = CFGAWGPrtCal;
            Serial.println("DAC Calibration:");
            break;

        case CFGAWGPrtCal:
            if (SYSGetMilliSecond() - ((AWG *) rgInstr[AWG1_ID])->comhdr.tStart >= AWG_SETTLING_TIME) {
                int32_t ideal = DACIDEAL(((AWG *) rgInstr[AWG1_ID])->t1);
                int32_t diff = 0;
                int32_t actual = 0;

                // this will take about 140us
                while(!(FBAWGorDCuV(((AWG *) rgInstr[AWG1_ID])->channelFB, &actual) == Idle));
                diff = actual - ideal;
                                
                Serial.print("Index: ");
                Serial.print(((AWG *) rgInstr[AWG1_ID])->t1);

                Serial.print(" \tIdeal: ");
                Serial.print(ideal);

                Serial.print(" mv\tActual: ");
                Serial.print(actual);

                Serial.print(" mv\tDiff: ");
                Serial.print(diff);
                Serial.println(" mv");

                if (++(((AWG *) rgInstr[AWG1_ID])->t1) < SWDACSIZE) {
                    LATH = ((AWG *) rgInstr[AWG1_ID])->dacMap[((AWG *) rgInstr[AWG1_ID])->t1];
                    ((AWG *) rgInstr[AWG1_ID])->comhdr.tStart = SYSGetMilliSecond();
                } else {
                    instrGrp.state = Done;
                    ((AWG *) rgInstr[AWG1_ID])->t1 = 0;
                }
            }
            break;
            
        default:
        case Done:
            instrGrp.state = Idle;
            break;
    }

    return (instrGrp.state);
}
#endif

STATE CFGReadCalibrationInfo(INSTRGRP& instrGrp, VOLTYPE const vol, CFGNAME const cfgName) 
{
    static char sz[128];        // should be less than 32, but we must have the memory   
    static uint32_t rgb[(MAX_INSTR_SIZE + sizeof(uint32_t) - 1) / sizeof(uint32_t)];

    IDHDR * pidhdr = (IDHDR *) rgb;
    STATE   retState = Idle;
    FRESULT fr = FR_OK;
   
    switch (instrGrp.state) 
    {
        case Idle:
            instrGrp.iInstr = 0;

        case CFGCalCheckInstr:
            instrGrp.state = CFGCalReadNext;
            if(instrGrp.rgUsage[instrGrp.iInstr] == instrIDCal)
            {
                IDHDR idHdr = {((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->cbInfo, CALVER, ((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->id, cfgName, macOpenScope};
                
                switch(((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->id)
                {
                    case OSC1_ID:
                    case OSC2_ID:
                    case DCVOLT1_ID:
                    case DCVOLT2_ID:
                    case AWG1_ID:
                        CFGCreateFileName(idHdr, NULL, sz, sizeof(sz));
                        instrGrp.state = CFGCalCkValid;
                        break;

                    default:
                       break;
                }
            }
           break;

        case CFGCalCkValid:
            if((fr = DFATFS::fschdrive(DFATFS::szFatFsVols[vol])) != FR_OK || (fr = DFATFS::fschdir(DFATFS::szRoot)) != FR_OK)
            {
                retState = (fr | STATEError);
            }
            else if((fr = DDIRINFO::fsstat(sz)) != FR_OK)
            {
                retState = (fr | STATEError);
            }
            else if(DDIRINFO::fsgetFileSize() != ((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->cbInfo)
            {
                retState = CFGUnableToReadConfigFile;
            }
            else 
            {
                ASSERT(sizeof(rgb) >= ((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->cbInfo);
                memcpy((void *) pidhdr, instrGrp.rghInstr[instrGrp.iInstr], ((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->cbInfo);
                instrGrp.state = CFGCalRead;
            }
            break;

        case CFGCalRead:
            // need to be very careful not to overwrite the orignal if it is the wrong data on the file
            if((retState = IOReadFile(instrGrp.dFile, vol, sz, *pidhdr)) == Idle)
            {                
                // make sure we have the correct calibration version.
                if((pidhdr->ver != CALVER))
                {
                    retState = CFGUnableToReadConfigFile;
                }
                else
                {
                    memcpy((void *) instrGrp.rghInstr[instrGrp.iInstr], pidhdr, pidhdr->cbInfo);
                    instrGrp.state = CFGCalReadNext;
                }
            }
            break;

        case CFGCalReadNext:
            instrGrp.iInstr++;
            if(instrGrp.iInstr >= instrGrp.chInstr)
            {
                instrGrp.state = Idle;
            }
            else
            {
                instrGrp.state = CFGCalCheckInstr;
            }
            break;

        default:
            retState = CFGUnableToReadConfigFile;
            break;
    }

    if(IsStateAnError(retState))
    {
        instrGrp.state = Idle;
        return(retState);
    }

    return (instrGrp.state);
}

STATE CFGGetCalibrationInfo(INSTRGRP& instrGrp) 
{
    STATE   retState = Idle;

    switch (instrGrp.state2) 
    {
        case Idle:
            if(pjcmd.iCal.state.parsing != Idle)
            {
                return(CFGCalibrating);
            }
            pjcmd.iCal.state.parsing = JSPARCalibrationLoading;
            instrGrp.state2 = CFGCalReadUser;

        case CFGCalReadUser:
            if((retState = CFGReadCalibrationInfo(instrGrp, USER_CALIBRATION)) == Idle)
            {
                instrGrp.state2 = Idle;
                pjcmd.iCal.state.parsing = Idle;
            }
            else if(IsStateAnError(retState))
            {
                instrGrp.state2 = CFGCalReadFactory;
            }
            break;

        case CFGCalReadFactory:
            if((retState = CFGReadCalibrationInfo(instrGrp, FACTORY_CALIBRATION)) == Idle)
            {
                instrGrp.state2 = Idle;
                pjcmd.iCal.state.parsing = Idle;
            }
            else if(IsStateAnError(retState))
            {
                instrGrp.state2 = CFGCalReadIdeal;
            }
            break;

        // this can not fail
        case CFGCalReadIdeal:

            for(instrGrp.iInstr = 0;  instrGrp.iInstr < instrGrp.chInstr; instrGrp.iInstr++)
            {
                if(instrGrp.rgUsage[instrGrp.iInstr] == instrIDCal)
                {
                    switch(((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->id)
                    {
                        case DCVOLT1_ID:
                            {
                                DCVOLT dcv(DCVOLT1_ID);
                                memcpy((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr], &dcv, sizeof(dcv));
                            }
                            break;

                        case DCVOLT2_ID:
                            {
                                DCVOLT dcv(DCVOLT2_ID);
                                memcpy((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr], &dcv, sizeof(dcv));
                            }
                            break;

                        case OSC1_ID:
                            {
                                OSC osc(OSC1_ID);
                                memcpy((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr], &osc, sizeof(osc));
                            }
                            break;

                        case OSC2_ID:
                            {
                                OSC osc(OSC2_ID);
                                memcpy((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr], &osc, sizeof(osc));
                            }
                            break;

                        case AWG1_ID:
                            {
                                AWG awg(AWG1_ID);
                                memcpy((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr], &awg, sizeof(awg));
                            }
                            break;

                        default:
                            break;
                    }
                    memcpy((void *) ((IDHDR *) instrGrp.rghInstr[instrGrp.iInstr])->mac.u8,  macOpenScope.u8, sizeof(MACADDR));
                }
            }
            instrGrp.state2 = Idle;
            pjcmd.iCal.state.parsing = Idle;
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);                      // we should never get here
            instrGrp.state2 = Idle;
            return(CFGUnableToReadConfigFile);
            break;
    }

    return (instrGrp.state2);
}



