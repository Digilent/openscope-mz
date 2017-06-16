/************************************************************************/
/*                                                                      */
/*    Version.c                                                         */
/*                                                                      */
/*    Header file containing the OpenScope Build Version number         */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    2/1/2017 (KeithV): Created                                        */
/************************************************************************/

// verison
#define VER_MAJOR 1
#define VER_MINOR 6
#define VER_PATCH 0

#define MKSTR2(a) #a
#define MKSTR(a) MKSTR2(a)

const char szEnumVersion[] = "\"major\":" MKSTR(VER_MAJOR) ",\"minor\":" MKSTR(VER_MINOR) ",\"patch\":" MKSTR(VER_PATCH);
const char szProgVersion[] = MKSTR(VER_MAJOR) "." MKSTR(VER_MINOR) "." MKSTR(VER_PATCH); 
