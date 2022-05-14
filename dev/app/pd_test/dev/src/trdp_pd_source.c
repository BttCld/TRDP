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
#include "vos/api/vos_utils.h"

#if defined (POSIX)
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <time.h>
#endif

#include "vos/api/vos_thread.h"


#include "common.h"

#define UNICAST_EN   0
#define MULTICAST_EN 1

#ifdef SOURCE
#define SOURCE_EN    1
#else
#define SOURCE_EN    0
#endif

#ifdef SINK
#define SINK_EN      1
#else
#define SINK_EN      0
#endif

/* --- globals ---------------------------------------------------------------*/

TRDP_MEM_CONFIG_T     memcfg;
TRDP_APP_SESSION_T    apph;
TRDP_PD_CONFIG_T      pdcfg;
TRDP_PROCESS_CONFIG_T proccfg;

/* default addresses - overriden from command line */
TRDP_IP_ADDR_T srcip;
TRDP_IP_ADDR_T dstip;
TRDP_IP_ADDR_T mcast;

static const char* _types[] =
{"Pd ->", "Pp ->", "Pr ->", "   <-", "   <-"};

int      _size[3]   = {0, 80, TRDP_MAX_PD_DATA_SIZE}; /* small/medium/big dataset */
int      _period[4] = {100, 250, 500, 1000}  ;         /* msec */
unsigned _uiCycle   = 0;

Port     _vsPort[64];                                  /* array of ports          */
unsigned _uiNports = 0;                                /* number of ports         */

#ifdef TSN_SUPPORT
#define PORT_FLAGS TRDP_FLAGS_TSN
#else
#define PORT_FLAGS TRDP_FLAGS_NONE
#endif

/***********************************************************************************************************************
 * PROTOTYPES
 */
void gen_push_ports_master(UINT32 comid, UINT32 echoid);



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
   gen_push_ports_master(10000, 20000);

   setup_ports(apph, _vsPort, _uiNports);

   vos_threadDelay(VOS_TIME_SEC(2));

   /* main test loop */
   for(;;)
   {
      /* drive TRDP communications */
      tlc_process(apph, NULL, NULL);

      /* poll (receive) data */
      poll_data(apph, _vsPort, _uiNports);

      /* process data every 500 msec */
      if (!(++tick % 50))
         process_data(apph, _vsPort, _uiNports, PORT_PUSH);

      /* wait 10 msec  */
      vos_threadDelay(VOS_TIME_MSEC(10));
   }

   return 0;
}

/* --- generate PUSH ports ---------------------------------------------------*/

void gen_push_ports_master (UINT32 comid, UINT32 echoid)
{
   Port    src;
   int     num = _uiNports;
   UINT32  a, sz, per;

   printf("- generating PUSH ports (master side) ... ");

   memset(&src, 0, sizeof(src));

   //src.type = PORT_PUSH;
   src.type = PORT_PULL;

  #if UNICAST_EN
   /* for unicast/multicast address */
   //for (a = 0; a < 2; ++a)
   a = 0;  // unicast
   {   /* for all dataset sizes */
      for (sz = 1; sz < 3; ++sz)
      {   /* for all cycle periods */
         for (per = 0; per < 2; ++per)
         {   /* comid  */
            src.comid = comid + 100u * a + 40 * (per + 1) + 3 * (sz + 1);
            snk.comid = echoid + 100u * a + 40 * (per + 1) + 3 * (sz + 1);
            /* dataset size */
            src.size = snk.size = (UINT32)size[sz];
            /* period [usec] */
            src.cycle = (UINT32)1000u * (UINT32)period[per];
            /* addresses */

            {   /* unicast address */
               src.dst = snk.src = dstip;
               src.src = snk.dst = srcip;
            }

            src.link = -1;
            /* add ports to config */
            _vPorts[_uiNports++] = src;
            _vPorts[_uiNports++] = snk;
         }
      }
   }
  #endif

  #if MULTICAST_EN
   /* for unicast/multicast address */
   //for (a = 0; a < 2; ++a)
   a = 1;  // multicast
   {
      /* for all dataset sizes */
      //for (sz = 1; sz < 3; ++sz)
      sz = 1; // only 256
      {
         /* for all cycle periods */
         //for (per = 0; per < 2; ++per)
         per = 3;
         {
            /* comid  */
            src.comid = comid + 100u * a + 40 * (per + 1) + 3 * (sz + 1);

            /* dataset size */
            src.size = (UINT32)_size[sz];

            /* period [usec] */
            src.cycle = (UINT32)1000u * (UINT32)_period[per];

            /* addresses */
            src.dst  = mcast;
            src.src  = srcip;
            src.link = -1;

            /* add ports to config */
            _vsPort[_uiNports++] = src;

           #if SOURCE_ECHO_EN
            {
               Port snk;

               memset(&snk, 0, sizeof(snk));
               snk.type    = PORT_SINK;
               snk.timeout = 4000000;         /* 4 secs timeout*/

               // ATTENZIONE si usa comid del source mentre esempio usava quello di input
               snk.comid = src.comid;

               snk.size  = src.size;
               snk.dst   = src.dst;
               snk.src   = dstip;

               _vPorts[_uiNports++] = snk;
            }
           #endif
         }
      }
   }
  #endif

   printf("%u ports created\n", _uiNports - num);
}

