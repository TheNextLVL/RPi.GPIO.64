/*
Copyright (c) 2012-2016 Ben Croston

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Revisions:
// 02 Ene 2019:
//    Added suppoort to 64bit OS
//    Added new way to identify the rpi model copied from wiringPi library

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <byteswap.h>
#include "cpuinfo.h"

// Definition of all data to identify the rpi models

const char *piModelNames [16] =
{
  "Model A",	//  0
  "Model B",	//  1
  "Model A+",	//  2
  "Model B+",	//  3
  "Pi 2",	//  4
  "Alpha",	//  5
  "CM",		//  6
  "Unknown07",	// 07
  "Pi 3",	// 08
  "Pi Zero",	// 09
  "CM3",	// 10
  "Unknown11",	// 11
  "Pi Zero-W",	// 12
  "Pi 3+",	// 13
  "Unknown14",	// 14
  "Unknown15",	// 15
} ;

// Changed to use the same that the original pl_revision
const int piRevision [16] =
{
  2,  //	"Model A",	   0
  2,  //	"Model B",	   1
  3,  //	"Model A+",	   2
  3,  // "Model B+",	   3
  3,  //	"Pi 2",	      4
  3,  // "Alpha",	      5
  0,  // "CM",		      6
  3,  // "Unknown07",  07
  3,  // "Pi 3",	     08
  3,  // "Pi Zero",	  09
  3,  // "CM3",	     10
  3,  // "Unknown11",  11
  3,  // "Pi Zero-W",  12
  3,  // "Pi 3+",	     13
  3,  // "Unknown14",  14
  3,  // "Unknown15",  15
} ;

const char *piMakerNames [16] =
{
  "Sony",	//	 0
  "Egoman",	//	 1
  "Embest",	//	 2
  "Unknown",	//	 3
  "Embest",	//	 4
  "Unknown05",	//	 5
  "Unknown06",	//	 6
  "Unknown07",	//	 7
  "Unknown08",	//	 8
  "Unknown09",	//	 9
  "Unknown10",	//	10
  "Unknown11",	//	11
  "Unknown12",	//	12
  "Unknown13",	//	13
  "Unknown14",	//	14
  "Unknown15",	//	15 
} ;

const char *piMemorySize [8] =
{
  "256M",		//	 0
  "512M",		//	 1
 "1024M",		//	 2
    "0M",		//	 3
    "0M",		//	 4
    "0M",		//	 5
    "0M",		//	 6
    "0M",		//	 7
} ;

const char *piProcesorNames [16] =
{
   "BCM2835",     //  0
   "BCM2836",     //  1
   "BCM2837",     //  2
   "Unknown03",	//	 3
   "Unknown04",	//	 4   
   "Unknown05",	//	 5
   "Unknown06",	//	 6
   "Unknown07",	//	 7
   "Unknown08",	//	 8
   "Unknown09",	//	 9
   "Unknown10",	//	10
   "Unknown11",	//	11
   "Unknown12",	//	12
   "Unknown13",	//	13
   "Unknown14",	//	14
   "Unknown15",	//	15
};


int get_rpi_info(rpi_info *info)
{
   FILE *fp;
   char revision[1024];
      
   #ifdef __aarch64__
      // 64bit processor -> new method
      uint32_t rpi_rev;
      int bRev, bType, bProc, bMfg, bMem, bWarranty ;
  
      fp = fopen("/proc/device-tree/system/linux,revision", "r");
      if (!fp)
      {
        // Let's try to read the /proc/device-tree/model from aarchOS
        fp = fopen("/proc/device-tree/model", "r");
        if (!fp){
          return -1;
        } else {
          
          fread(revision, 1, 1024, fp);
          if (strstr(revision, "Raspberry Pi 3 Model B+")!= NULL)
            rpi_rev = 0xa020d3;
          else
            return -1;
        }
      } else {
        fread(&rpi_rev, sizeof(uint32_t), 1, fp);
        #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
          rpi_rev = bswap_32(rpi_rev);  // linux,revision appears to be in big endian
        #endif
      }
      
      if ((rpi_rev &  (1 << 23)) != 0)	// New way
      {
         bRev      = (rpi_rev & (0x0F <<  0)) >>  0 ;
         bType     = (rpi_rev & (0xFF <<  4)) >>  4 ;
         bProc     = (rpi_rev & (0x0F << 12)) >> 12 ;	
         bMfg      = (rpi_rev & (0x0F << 16)) >> 16 ;
         bMem      = (rpi_rev & (0x07 << 20)) >> 20 ;
         bWarranty = (rpi_rev & (0x03 << 24)) != 0 ;  // Not used for now.
         
         sprintf(revision,"%x",rpi_rev);
         
         info->p1_revision  = piRevision[bRev];
         info->ram          = piMemorySize[bMem];
         info->manufacturer = piMakerNames[bMfg];
         info->processor    = piProcesorNames[bProc];
         info->type         = piModelNames[bType];
         strcpy(info->revision,revision);
                  
      } else {
         return -1;
      }
   #else
      // No 64 bit processor --> old method
      char buffer[1024];
      char hardware[1024];
      char *rev;
      int found = 0;
      int len;
      
      if ((fp = fopen("/proc/cpuinfo", "r")) == NULL)
         return -1;
      while(!feof(fp) && fgets(buffer, sizeof(buffer), fp)) {
         sscanf(buffer, "Hardware	: %s", hardware);
         if (strcmp(hardware, "BCM2708") == 0 ||
             strcmp(hardware, "BCM2709") == 0 ||
             strcmp(hardware, "BCM2835") == 0 ||
             strcmp(hardware, "BCM2836") == 0 ||
             strcmp(hardware, "BCM2837") == 0 ) {
            found = 1;
         }
         sscanf(buffer, "Revision	: %s", revision);
      }
      fclose(fp);

      if (!found)
         return -1;

      if ((len = strlen(revision)) == 0)
         return -1;

      if (len >= 6 && strtol((char[]){revision[len-6],0}, NULL, 16) & 8) {
         // new scheme
         //info->rev = revision[len-1]-'0';
         strcpy(info->revision, revision);
         switch (revision[len-2]) {
           case '0': info->type = "Model A"; info->p1_revision = 2; break;
           case '1': info->type = "Model B"; info->p1_revision = 2; break;
           case '2': info->type = "Model A+"; info->p1_revision = 3; break;
           case '3': info->type = "Model B+"; info->p1_revision = 3; break;
           case '4': info->type = "Pi 2 Model B"; info->p1_revision = 3; break;
           case '5': info->type = "Alpha"; info->p1_revision = 3; break;
           case '6': info->type = "Compute"; info->p1_revision = 0; break;
                     case '8': info->type = "Pi 3 Model B"; info->p1_revision = 3; break;
                     case '9': info->type = "Zero"; info->p1_revision = 3; break;
           default : info->type = "Unknown"; info->p1_revision = 3; break;
        }
        switch (revision[len-4]) {
           case '0': info->processor = "BCM2835"; break;
           case '1': info->processor = "BCM2836"; break;
                     case '2': info->processor = "BCM2837"; break;
           default : info->processor = "Unknown"; break;
        }
        switch (revision[len-5]) {
           case '0': info->manufacturer = "Sony"; break;
           case '1': info->manufacturer = "Egoman"; break;
           case '2': info->manufacturer = "Embest"; break;
           case '4': info->manufacturer = "Embest"; break;
           default : info->manufacturer = "Unknown"; break;
        }
        switch (strtol((char[]){revision[len-6],0}, NULL, 16) & 7) {
           case 0: info->ram = "256M"; break;
           case 1: info->ram = "512M"; break;
           case 2: info->ram = "1024M"; break;
           default: info->ram = "Unknown"; break;
         }
      } else {
         // old scheme
         info->ram = "Unknown";
         info->manufacturer = "Unknown";
         info->processor = "Unknown";
         info->type = "Unknown";
         strcpy(info->revision, revision);

         // get last four characters (ignore preceeding 1000 for overvolt)
         if (len > 4)
            rev = (char *)&revision+len-4;
         else
            rev = revision;

         if ((strcmp(rev, "0002") == 0) ||
             (strcmp(rev, "0003") == 0)) {
            info->type = "Model B";
            info->p1_revision = 1;
            info->ram = "256M";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "0004") == 0) {
            info->type = "Model B";
            info->p1_revision = 2;
            info->ram = "256M";
            info->manufacturer = "Sony";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "0005") == 0) {
            info->type = "Model B";
            info->p1_revision = 2;
            info->ram = "256M";
            info->manufacturer = "Qisda";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "0006") == 0) {
            info->type = "Model B";
            info->p1_revision = 2;
            info->ram = "256M";
            info->manufacturer = "Egoman";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "0007") == 0) {
            info->type = "Model A";
            info->p1_revision = 2;
            info->ram = "256M";
            info->manufacturer = "Egoman";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "0008") == 0) {
            info->type = "Model A";
            info->p1_revision = 2;
            info->ram = "256M";
            info->manufacturer = "Sony";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "0009") == 0) {
            info->type = "Model A";
            info->p1_revision = 2;
            info->ram = "256M";
            info->manufacturer = "Qisda";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "000d") == 0) {
            info->type = "Model B";
            info->p1_revision = 2;
            info->ram = "512M";
            info->manufacturer = "Egoman";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "000e") == 0) {
            info->type = "Model B";
            info->p1_revision = 2;
            info->ram = "512M";
            info->manufacturer = "Sony";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "000f") == 0) {
            info->type = "Model B";
            info->p1_revision = 2;
            info->ram = "512M";
            info->manufacturer = "Qisda";
            info->processor = "BCM2835";
         } else if ((strcmp(rev, "0011") == 0) ||
                    (strcmp(rev, "0014") == 0)) {
            info->type = "Compute Module";
            info->p1_revision = 0;
            info->ram = "512M";
            info->processor = "BCM2835";
         } else if (strcmp(rev, "0012") == 0) {
            info->type = "Model A+";
            info->p1_revision = 3;
            info->ram = "256M";
            info->processor = "BCM2835";
         } else if ((strcmp(rev, "0010") == 0) ||
                    (strcmp(rev, "0013") == 0)) {
            info->type = "Model B+";
            info->p1_revision = 3;
            info->ram = "512M";
            info->processor = "BCM2835";
         } else {  // don't know - assume revision 3 p1 connector
            info->p1_revision = 3;
         }
      }
   #endif

   return 0;
}

/*
For Pi v2 and subsequent models - e.g. the Zero:
32 bits
[USER:8] [NEW:1] [MEMSIZE:3] [MANUFACTURER:4] [PROCESSOR:4] [TYPE:8] [REV:4]
NEW          23: will be 1 for the new scheme, 0 for the old scheme
MEMSIZE      20: 0=256M 1=512M 2=1G
MANUFACTURER 16: 0=SONY 1=EGOMAN 2=EMBEST
PROCESSOR    12: 0=2835 1=2836
TYPE         04: 0=MODELA 1=MODELB 2=MODELA+ 3=MODELB+ 4=Pi2 MODEL B 5=ALPHA 6=CM
REV          00: 0=REV0 1=REV1 2=REV2

--------------------

pi2 = 1<<23 | 2<<20 | 1<<12 | 4<<4 = 0xa01040

--------------------

SRRR MMMM PPPP TTTT TTTT VVVV

S scheme (0=old, 1=new)
R RAM (0=256, 1=512, 2=1024)
M manufacturer (0='SONY',1='EGOMAN',2='EMBEST',3='UNKNOWN',4='EMBEST')
P processor (0=2835, 1=2836 2=2837)
T type (0='A', 1='B', 2='A+', 3='B+', 4='Pi 2 B', 5='Alpha', 6='Compute Module')
V revision (0-15)

*/
