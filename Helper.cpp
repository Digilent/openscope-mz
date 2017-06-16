/************************************************************************/
/*                                                                      */
/*    Helper.cpp                                                        */
/*                                                                      */
/*    Helper functions to deal with date and time                       */
/*    and MAC and Number printing                                       */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2013, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*    2/1/2013(KeithV): Created                                         */
/************************************************************************/
#include <OpenScope.h>

/***    int GetIP(IPv4& ip, sz)
 *
 *    Parameters:
 *          ip, the IP to print
 *          sz - a pointer to a buffer long enough to take the number
 *
 *    Return Values:
 *          nbr or char in the string
 *
 *    Description: 
 *    
 *      A simple routine to print the IP address out
 *      on the serial monitor.
 * ------------------------------------------------------------ */
int GetIP(IPv4& ip, char * sz)
{
    int cch = 0; 

    strcpy(sz, "IP: ");
    cch = strlen(sz);

    return(cch + GetNumb(ip.u8, 4, '.', &sz[cch]));
}

/***    int GetMAC(MAC& mac, sz)
 *
 *    Parameters:
 *          mac, the MAC to print
 *          sz - a pointer to a buffer long enough to take the number
 *              
 *    Return Values:
 *          nbr or char in the string
 *
 *    Description: 
 *    
 *      A simple routine to print the MAC address out
 *      on the serial monitor.
 * ------------------------------------------------------------ */
int GetMAC(MACADDR& mac, char * sz)
{
    int cch = 0; 

    strcpy(sz, "MAC: ");
    cch = strlen(sz);

    return(cch + GetNumb(mac.u8, 6, ':', &sz[cch]));
}

/***    int GetNumb(byte * rgb, int cb, char chDelim, sz)
 *
 *    Parameters:
 *          rgb - a pointer to a MAC or IP, or any byte string to print
 *          cb  - the number of bytes in rgb
 *          chDelim - the delimiter to use between bytes printed
 *          sz - a pointer to a buffer long enough to take the number
 *              
 *    Return Values:
 *          nbr or char in the string
 *
 *    Description: 
 *    
 *      A simple routine to print to the serial monitor bytes in either HEX or DEC
 *      If the delimiter is a ':' the output will be in HEX
 *      All other delimiters will be in DEC.
 *      Only HEX numbers get leading 0
 *      DEC number have no leading zeros on the number.
 * ------------------------------------------------------------ */
int GetNumb(uint8_t * rgb, int cb, char chDelim, char * sz)
{
    int cch = 0;

    for(int i=0; i<cb; i++)
    {
        if(chDelim == ':' && rgb[i] < 16)
        {
            sz[cch++] = '0';
        }
    
        if(chDelim == ':')
        {
            itoa(rgb[i], &sz[cch], 16);
        }
        else
        {
            itoa(rgb[i], &sz[cch], 10);
        }  
        cch += strlen(&sz[cch]);
   
        if(i < cb-1)
        {
            sz[cch++] = chDelim;  
        }
    }
    sz[cch] = 0;

    return(cch);
}

/***    void GetDayAndTime()
 *
 *    Parameters:
 *          epochTimeT  - the epoch time to format to UTC time.
 *          sz - a pointer to a buffer long enough to take the number (min of 29, but leave 32 bytes)
 *              
 *    Return Values:
 *          nbr or char in the string
 *
 *    Description: 
 *    
 *      This illistrates how to use the Ethernet.secondsSinceEpoch()
 *      method to get the current time and display it.
 *
 *      In order for this call to work you must have a valid 
 *      DNS server so the time servers can be located 
 * ------------------------------------------------------------ */
int GetDayAndTime(unsigned int epochTimeT, char * sz)
{
    // Epoch is 1/1/1970; I guess that is when computers became real?
    // There are 365 days/year, every 4 years is leap year, every 100 years skip leap year. Every 400 years, do not skip the leap year. 2000 did not skip the leap year
    static const unsigned int secPerMin = 60;
    static const unsigned int secPerHour = 60 * secPerMin;
    static const unsigned int secPerDay  = 24 * secPerHour;
    static const unsigned int secPerYear = 365 * secPerDay;
    static const unsigned int secPerLeapYearGroup = 4 * secPerYear + secPerDay;
    static const unsigned int secPerCentury = 25 * secPerLeapYearGroup - secPerDay;
    static const unsigned int secPer400Years = 4 * secPerCentury + secPerDay;;
    static const int daysPerMonth[] = {31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 29}; // Feb has 29, we must allow for leap year.
    static const char * szMonths[] = {"Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", "Jan", "Feb"}; 
    
    int cch = 0;
 
    // go ahead and adjust to a leap year, and to a century boundary
    // at Mar 2000 we have 30 years (From 1970) + 8 leap days (72,76,80,84,88,92,96, and Feb 2000 do not skip leap year) and Jan (31) + Feb(28)
    unsigned int secSinceMar2000 = epochTimeT - 30 * secPerYear - (8 + 31 + 28) * secPerDay;
 
    unsigned int nbr400YearGroupsFromMar2000 = secSinceMar2000 / secPer400Years;
    unsigned int secInThis400YearGroups = secSinceMar2000 % secPer400Years;
    
    // now we are aligned so the weirdness for the not skiping of a leap year is the very last day of the 400 year group.
    // because of this extra day in the 400 year group, it is possible to have 4 centries and a day.
    unsigned int nbrCenturiesInThis400YearGroup = secInThis400YearGroups / secPerCentury;
    unsigned int secInThisCentury = secInThis400YearGroups % secPerCentury;

    // if we come up with 4 centries, then we must be on leap day that we don't skip at the end of the 400 year group
    // so add the century back on as this  Century is the extra day in this century.
    if(nbrCenturiesInThis400YearGroup == 4)
    {
        nbrCenturiesInThis400YearGroup = 3;   // This can be a max of 3 years
        secInThisCentury += secPerCentury;    // go ahead and add the century back on to our time in this century
    }

    // This is going to work out just fine
    // either this is a normal century and the last leap year group is going to be a day short,
    // or this is at the end of the 400 year group and the last 4 year leap year group will work out to have 29 days as in a normal
    // 4 year leap year group.  
    unsigned int nbrLeapYearGroupsInThisCentury = secInThisCentury / secPerLeapYearGroup;
    unsigned int secInThisLeapYearGroup = secInThisCentury % secPerLeapYearGroup;
 
    // if this is at the end of the leap year group, there could be an extra day
    // which could cause us to come up with 4 years in this leap year group.
    unsigned int nbrYearsInThisLeapYearGroup = secInThisLeapYearGroup / secPerYear;
    unsigned int secInThisYear = secInThisLeapYearGroup % secPerYear;

    // are we on a leap day?
    if(nbrYearsInThisLeapYearGroup == 4)
    {
        nbrYearsInThisLeapYearGroup = 3;    // that is the max it can be.
        secInThisYear += secPerYear;        // add back the year we just took off the leap year group
    }
  
    int nbrOfDaysInThisYear = (int) (secInThisYear / secPerDay); // who cares if there is an extra day for leap year
    int secInThisDay = (int) (secInThisYear % secPerDay);
 
    int nbrOfHoursInThisDay = secInThisDay / secPerHour;
    int secInThisHours = secInThisDay % secPerHour;
 
    int nbrMinInThisHour = secInThisHours / secPerMin;
    int secInThisMin = secInThisHours % secPerMin;
    
    int monthCur = 0;
    int dayCur = nbrOfDaysInThisYear;
    int yearCur = 2000 + 400 * nbr400YearGroupsFromMar2000 + 100 * nbrCenturiesInThis400YearGroup + 4 * nbrLeapYearGroupsInThisCentury + nbrYearsInThisLeapYearGroup;
  
    // this will walk us past the current month as the dayCur can go negative.
    // we made the leap day the very last day in array, so if this is leap year, we will be able to
    // handle the 29th day.
    for(monthCur = 0, dayCur = nbrOfDaysInThisYear; dayCur >= 0; monthCur++)
    {
      dayCur -= daysPerMonth[monthCur];
    }
     
    // since we know we went past, we can back up a month
    monthCur--;
    dayCur += daysPerMonth[monthCur]; // put the last months days back to go positive on days
     
    // We did zero based days in a month, but we read 1 based days in a month.
    dayCur++;

    // we have one remaining issue
    // if this is Jan or Feb, we are really into the next year. Remember we started our year in Mar, not Jan
    // so if this is Jan or Feb, then add a year to the year
    if(monthCur >= 10)
    {
        yearCur++;
    }
     

    // local
    strcpy(sz, "UTC: ");
    cch += strlen(sz);

    // month
    strcpy(&sz[cch], szMonths[monthCur]);
    cch += strlen(szMonths[monthCur]);
    sz[cch++] = ' ';

    // day
    itoa(dayCur, &sz[cch], 10);
    cch += strlen(&sz[cch]);
    sz[cch++] = ',';
    sz[cch++] = ' ';
    
    // year
    itoa(yearCur, &sz[cch], 10);
    cch += strlen(&sz[cch]);
    sz[cch++] = ' ';
    sz[cch++] = '@';
    sz[cch++] = ' ';

    // hour
    itoa(nbrOfHoursInThisDay, &sz[cch], 10);
    cch += strlen(&sz[cch]);
    sz[cch++] = ':';
    
    // min
    if(nbrMinInThisHour < 10)
    {
        sz[cch++] = '0';
    }
    itoa(nbrMinInThisHour, &sz[cch], 10);
    cch += strlen(&sz[cch]);
    sz[cch++] = ':';


    // sec
    if(secInThisMin < 10)
    {
        sz[cch++] = '0';
    }
    itoa(secInThisMin, &sz[cch], 10);
    cch += strlen(&sz[cch]);
    sz[cch] = 0;

    return(cch);
 }
 
#ifdef __cplusplus
extern "C" {
#endif
/* ------------------------------------------------------------ */
/*				Soft reset routine           					*/
/* ------------------------------------------------------------ */
void __attribute__((noreturn)) _softwareReset(void)
{
    uint32_t status = 0;
	volatile int * p = (volatile int *)&RSWRST;

    // disable interrupts
	asm volatile("di    %0" : "=r"(status));

	// Unlock the system
	SYSKEY = 0;
	SYSKEY = 0xAA996655;
	SYSKEY = 0x556699AA;

	// Perform the software reset
	RSWRSTSET=_RSWRST_SWRST_MASK;
	*p;

	// Wait for the rest to take place
	while(1);
}

#ifdef __cplusplus
}
#endif

