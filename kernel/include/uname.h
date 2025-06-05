#ifndef __UNAME_H
#define __UNAME_H

#include "defs.h"

#define UTSNAME_LENGTH 65
#define SYSNAME "rexvapor"
#define NODENAME "none"
#define RELEASE "5.0"
#define VERSION "0.1"
#define MACHINE "QEMU"
#define DOMAINNAME "none"

struct utsname
{
    char sysname[UTSNAME_LENGTH] ;
    char nodename[UTSNAME_LENGTH];
    char release[UTSNAME_LENGTH];
    char version[UTSNAME_LENGTH];
    char machine[UTSNAME_LENGTH];
    char domainname[UTSNAME_LENGTH];
};

#define INIT_UTS(u) \
    do { \
        strncpy((u).sysname, SYSNAME, strlen(SYSNAME) + 1); \
        strncpy((u).nodename, NODENAME, strlen(NODENAME) + 1); \
        strncpy((u).release, RELEASE, strlen(RELEASE) + 1); \
        strncpy((u).version, VERSION, strlen(VERSION) + 1); \
        strncpy((u).machine, MACHINE, strlen(MACHINE) + 1); \
        strncpy((u).domainname, DOMAINNAME, strlen(DOMAINNAME) + 1); \
    } while (0)
#endif