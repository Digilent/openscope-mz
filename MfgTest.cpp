/************************************************************************/
/*                                                                      */
/*   MfgTest.cpp                                                        */
/*                                                                      */
/*    These are undocumented Manufacturing Tests                        */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*    2/27/2017(KeithV): Created                                        */
/*    2/27/2017(TommyK): Manufacturing Test Code                        */
/************************************************************************/
#include    <OpenScope.h>

/************************************************************************/
//    Manufacturing Test JSON parsing                                                      
//
//  command format:
//
//  {
//      "test":[
//          {
//              "command":"run",
//              "testNbr": <number>
//          }
//      ]
//  }
//
//
//  reponse format:
//
//  {
//      "test":[
//          {
//              "command":"run",
//              "statusCode": 0,
//              "wait": 0,
//              "returnNbr": <number>
//          }
//      ]
//  }
//
//
//
//  Multi-commands are permitted, such as ....
//  {
//      "test":[
//          {
//              "command":"run",
//              "testNbr": 1
//          },
//          {
//              "command":"run",
//              "testNbr": 2
//          }
//      ]
//  }
//
//
//  Multi-command reponse format:
//
//  {
//      "test":[
//          {
//              "command":"run",
//              "statusCode": 0,
//              "wait": 0,
//              "returnNbr": 1
//          },
//          {
//              "command":"run",
//              "statusCode": 0,
//              "wait": 0,
//              "returnNbr": 2
//          }
//      ]
//  }
//
//
/************************************************************************/  


/***    uint32_t MfgTest(uint32_t testNbr)
 *
 *    Parameters:
 *          testNbr - The test number to run
 *              
 *    Return Values:
 *          uint32_t        - the result of the test, this can be an error code, bit string, anything that can be expressed as a 32 bit number
 *
 *    Description: 
 *    
 *      Executes the Manufacturing test inline with the JSON parsing, each test should not take more than 20-30 msec, if more time is 
 *          needed, it should be broken up into multiple tests.
 *    
 * ------------------------------------------------------------ */
uint32_t MfgTest(uint32_t testNbr)
{   

    return(testNbr+1);
}

