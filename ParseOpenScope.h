/************************************************************************/
/*                                                                      */
/*    LexOpenScope.h                                                    */
/*                                                                      */
/*    Header for the OpenScope token parser                             */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*    7/11/2016(KeithV): Created                                        */
/************************************************************************/

#ifndef LexOpenScope_h
#define LexOpenScope_h

#include <LexJSON.h>

#ifdef __cplusplus

class OSPAR : public JSON
{
public:

    typedef GCMD::ACTION (OSPAR::* FNWRITEDATA)(char const pchWrite[], int32_t cbWrite, int32_t& cWritten);
    typedef GCMD::ACTION (OSPAR::* FNREADDATA)(int32_t iOData, uint8_t const *& pbRead, int32_t& cbRead);

    typedef struct _STRU32
    {
        char const * const  szToken;
        const uint32_t      u32;
    } STRU32;

    typedef struct _ODATA
    {
        INSTR_ID    id;
        STATE *     pLockState;
        FNREADDATA  ReadData;
        uint32_t    iOut;
        uint32_t    cb;
        uint8_t  *  pbOut;
    } ODATA;

    typedef struct _IDATA
    {
        STATE *     pLockState;
        FNWRITEDATA WriteData;       
        int32_t     cb;
        uint32_t    iBinary;
    } IDATA;

    typedef enum
    {
        NONE,
        JSON,
        OSJB
    } OSCMD;

    typedef enum
    {
        ICDNone,
        ICDStart,
        ICDEnd
    } ICD;

private:
    STATE           stateOSJB;
    STATE           stateOSJBNextWhite;
    STATE           stateOSJBNextNewLine;
    STATE           stateOSJBNextChunk;

    STATE           state;
    STATE           stateNameSep;
    STATE           stateArray;
    STATE           stateValueSep;
    STATE           stateEndArray;
    STATE           stateEndObject;
    STATE           stateOutLock;
    STRU32 const *  rgStrU32;
    uint32_t        cStrU32;

    bool            fError;
    bool            fDoneReadingJSON;
    bool            fWrite;

    uint32_t        tStartCmd;
    uint32_t        tLastCmd;

    int32_t         cbStreamInception;
    int32_t         iStream;
    int32_t         cbConsumed;

    int32_t         iBinaryDone;
    int32_t         iBinary;
    FNWRITEDATA     WriteData; 

    int32_t         cbChunk;
    int32_t         iChunkStart;
    int32_t         iChunk;

    int32_t         iOSJBCount;
    char            szOSJBCount[128];

    char            pchJSONRespBuff[0x4000];    // 16384 bytes for output OSJB

    // JSON callback routine; Must be supplied
    STATE ParseToken(char const * szToken, uint32_t cbToken, JSONTOKEN jsonToken); 

    uint32_t Uint32FromStr(STRU32 const * const rgStrU32L, uint32_t cStrU32L, char const * const sz, uint32_t cb, STATE defaultState=OSPARSyntaxError);
    GCMD::ACTION ReadJSONResp(int32_t iOData, uint8_t const *& pbRead, int32_t& cbRead);
    GCMD::ACTION ReadFile(int32_t iOData, uint8_t const *& pbRead, int32_t& cbRead);
    GCMD::ACTION ReadLogFile(int32_t iOData, uint8_t const *& pbRead, int32_t& cbRead);
 
public:
    bool            fLocked;
    int32_t         iOData;
    int32_t         cOData;
    int32_t         cIData;
    ODATA           odata[4];
    IDATA           idata[4];

    uint8_t const * pbOutput;
    int32_t         cbOutput;

    OSPAR() : tStartCmd(0), tLastCmd(0)
    {
        Init(ICDNone);
    }

    void Init(ICD icd)
    {
        JSON::Init();    
        stateOSJB           = Idle;
        stateOSJBNextWhite  = Idle;
        stateOSJBNextChunk  = Idle;
        stateOSJBNextNewLine = Idle;

        state               = Idle;
        stateNameSep        = OSPARSyntaxError;
        stateArray          = OSPARSyntaxError;
        stateValueSep       = OSPARSyntaxError;
        stateEndArray       = OSPARSyntaxError;
        stateEndObject      = OSPARSyntaxError;
        rgStrU32            = NULL;
        cStrU32             = 0;

        fError              = false;
        fWrite              = false;

        cbStreamInception   = 0;
        iStream             = 0;
        cbConsumed          = 0;

        iBinary             = 0;
        iBinaryDone         = 0;
        WriteData           = NULL;

        cbChunk             = 0;
        iChunkStart         = 0;
        iChunk              = 0;
        iOSJBCount          = 0;

        fLocked             = false;
        iOData              = 0;
        cOData              = 1;
        cIData              = 0;

        pbOutput            = NULL;
        cbOutput            = 0;

        memset(idata, 0, sizeof(idata));
        memset(odata, 0, sizeof(odata));

        stateOutLock        = LOCKAvailable;
        odata[0].id         = NULL_ID;
        odata[0].pbOut      = (uint8_t *) pchJSONRespBuff;
        odata[0].cb         = 0;
        odata[0].pLockState = &stateOutLock;
        odata[0].ReadData   = &OSPAR::ReadJSONResp;

        switch(icd)
        {
            case ICDStart:
                tStartCmd = ReadCoreTimer();
                break;

            case ICDEnd:
                tLastCmd = (ReadCoreTimer() - tStartCmd) / CORE_TMR_TICKS_PER_USEC;
                break;

            default:
            case ICDNone:
                break;
        }
    }

    OSCMD IsOSCmdStart(char ch)
    {
        if(ch == '{') return(OSPAR::JSON);
        else if(('0' <= ch && ch <= '9') || ('a' <= ch && ch <= 'f') || ('A' <= ch && ch <= 'F')) return(OSPAR::OSJB);

        return(OSPAR::NONE);
    }

    GCMD::ACTION StreamOS(char const * szStream, int32_t cbStream);
    GCMD::ACTION WriteOSJBFile(char const pchWrite[], int32_t cbWrite, int32_t& cbWritten);
};
#endif // c++
#endif