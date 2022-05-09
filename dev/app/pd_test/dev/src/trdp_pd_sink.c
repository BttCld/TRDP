/**********************************************************************************************************************/
/**
 * @file            trdp-pd-test.c
 *
 * @brief           Test application for TRDP PD
 *
 * @details
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Petr Cvachou?ek, UniControls
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright UniControls, 2013. All rights reserved.
 *
 * $Id: trdp-pd-test.c 2205 2020-08-20 12:20:43Z bloehr $
 *
 *      AÖ 2020-05-04: Ticket #330: Extend TRDP_PDTest with TSN support
 *      AÖ 2019-11-11: Ticket #290: Add support for Virtualization on Windows
 *      BL 2018-06-20: Ticket #184: Building with VS 2015: WIN64 and Windows threads (SOCKET instead of INT32)
 *      BL 2018-03-06: Ticket #101 Optional callback function on PD send
 *      BL 2017-06-30: Compiler warnings, local prototypes added
 *
 */

#include <stdio.h>
#include <string.h>

#include "api/trdp_if_light.h"

#if defined (POSIX)
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <time.h>
#endif

#include "vos/api/vos_utils.h"
#include "vos/api/vos_thread.h"

#include "common.h"

#define UNICAST_EN   0
#define MULTICAST_EN 1

/* --- globals ---------------------------------------------------------------*/

TRDP_MEM_CONFIG_T     memcfg;
TRDP_APP_SESSION_T    apph;
TRDP_PD_CONFIG_T      pdcfg;
TRDP_PROCESS_CONFIG_T proccfg;

/* default addresses - overriden from command line */
TRDP_IP_ADDR_T srcip;
TRDP_IP_ADDR_T dstip;
TRDP_IP_ADDR_T mcast;

int      _size[3]   = {0, 80, TRDP_MAX_PD_DATA_SIZE}; /* small/medium/big dataset */
int      _period[4] = {100, 250, 500, 1000}  ;         /* msec */

Port     _vsPort[64];                                  /* array of ports          */
unsigned _uiNports = 0;                                /* number of ports         */

/***********************************************************************************************************************
 * PROTOTYPES
 */
static void gen_push_ports_slave (UINT32 comid, UINT32 echoid);


/* --- main ------------------------------------------------------------------*/
int main (int argc, char* argv[])
{
   TRDP_ERR_T err;
   unsigned tick = 0;

   printf("TRDP process data test program, version r178\n");

   if (argc < 4)
   {
      printf("usage: %s <localip> <remoteip> <mcast> <logfile>\n", argv[0]);
      printf("  <localip>  .. own IP address (ie. 10.2.24.1)\n");
      printf("  <remoteip> .. remote peer IP address (ie. 10.2.24.2)\n");
      printf("  <mcast>    .. multicast group address (ie. 239.2.24.1)\n");
      printf("  <logfile>  .. file name for logging (ie. test.txt)\n");

      return 1;
   }

   srcip = vos_dottedIP(argv[1]);
   dstip = vos_dottedIP(argv[2]);
   mcast = vos_dottedIP(argv[3]);

   if (!srcip || !dstip || (mcast >> 28) != 0xE)
   {
      printf("invalid input arguments\n");
      return 1;
   }

   memset(&memcfg,  0, sizeof(memcfg));
   memset(&proccfg, 0, sizeof(proccfg));

   if (argc >= 5)
   {
      LogOpen(argv[4]);
   }

   /* initialize TRDP protocol library */
   err = tlc_init(LogPrint, NULL, &memcfg);
   if (err != TRDP_NO_ERR)
   {
      printf("tlc_init() failed, err: %d\n", err);
      return 1;
   }

   pdcfg.pfCbFunction  = NULL;
   pdcfg.pRefCon       = NULL;
   pdcfg.sendParam.qos = 5;
   pdcfg.sendParam.ttl = 64;
   pdcfg.flags         = TRDP_FLAGS_NONE;
   pdcfg.timeout       = VOS_TIME_SEC(10);
   pdcfg.toBehavior    = TRDP_TO_SET_TO_ZERO;
   pdcfg.port          = 17224;

   /* open session */
   err = tlc_openSession(&apph, srcip, 0, NULL, &pdcfg, NULL, &proccfg);
   if (err != TRDP_NO_ERR)
   {
      printf("tlc_openSession() failed, err: %d\n", err);
      return 1;
   }

   /* generate ports configuration */
   gen_push_ports_slave(10000, 20000);

   // populate ports vector
   setup_ports(apph, _vsPort, _uiNports);

   vos_threadDelay(VOS_TIME_SEC(2));

   {
      VOS_FDS_T   vsFD;
      INT32       i32MaxFD = 0;
      TRDP_TIME_T sTNow;
      TRDP_TIME_T sTNext;
      TRDP_TIME_T sTOut = {0, VOS_TIME_MSEC(10)};
      INT32       i32RetLoc;

      vos_getTime(&sTNow);

      /* main test loop */
      for(;;)
      {
         sTNext.tv_sec  = 0;
         sTNext.tv_usec = VOS_TIME_MSEC(500);

         vos_addTime(&sTNow, &sTNext);
         sTNext = sTNow;

         FD_ZERO(&vsFD);

         tlc_getInterval(apph, &sTOut, &vsFD, &i32MaxFD);

         i32RetLoc = vos_select(i32MaxFD + 1, &vsFD, NULL, NULL, &sTOut);

         /* drive TRDP communications */
         tlc_process(apph, &vsFD, &i32RetLoc);

         /* poll (receive) data */
         //poll_data(apph, _vsPort, _uiNports);

         /* process data every 500 msec */
         //vos_getTime(&sTNow);
         //if (timercmp(&sTNow, &sTNext, >))
         //   process_data(apph, _vsPort, _uiNports, PORT_SINK);

         /* wait 10 msec  */
         //vos_threadDelay(10000);
      }
   }

   return 0;
}

/* --- generate PUSH ports ---------------------------------------------------*/

void gen_push_ports_slave (UINT32 comid, UINT32 echoid)
{
   Port snk;
   int num = _uiNports;
   UINT32 a, sz, per;

   printf("- generating PUSH ports (slave side) ... ");

   memset(&snk, 0, sizeof(snk));

   snk.type = PORT_SINK;

  #if MULTICAST_EN
   //for (a = 0; a < 2; ++a)
   a = 1; // multicast
   {
      /* for all dataset sizes */
      //for (sz = 1; sz < 3; ++sz)
      sz = 1;
      {
         /* for all cycle periods */
         //for (per = 0; per < 2; ++per)
         per = 3;
         {
            snk.comid   = comid  + 100 * a + 40 * (per + 1) + 3 * (sz + 1);
            snk.size    = (UINT32)_size[sz];
            snk.cycle   = (UINT32)(1000 * _period[per]);
            snk.dst     = mcast;
            //snk.src     = dstip;
            snk.timeout = 4 * snk.cycle;

            /* add ports to config */
            _vsPort[_uiNports++] = snk;
         }
      }
   }
  #endif

   printf("%u ports created\n", _uiNports - num);
}
