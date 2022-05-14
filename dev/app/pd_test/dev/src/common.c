#include <stdio.h>
#include <string.h>

#include "consolle.h"

#include "common.h"

static const char* _get_result_string (int ret);

const char* g_PdTypes[] = {"Ppush  ->", "Ppull  ->", "Preq   ->", "Psink  <-", "Psinkp <-"};

static unsigned _uiCycle = 0;

static void _PortPrintData (Port* p, UINT8* pData, UINT32 dataSize);
static void _cbPortSink    (void* pRefCon,
                            TRDP_APP_SESSION_T    appHandle,
                            const TRDP_PD_INFO_T* pMsg,
                            UINT8*                pData,
                            UINT32                dataSize);

/* --- setup ports -----------------------------------------------------------*/

void setup_ports (TRDP_APP_SESSION_T apph, Port vsPort[], unsigned int uiNports)
{
   unsigned i;

   printf("- setup ports:\n");
   /* setup ports one-by-one */
   for (i = 0; i < uiNports; ++i)
   {
      Port* p = &vsPort[i];
      TRDP_COM_PARAM_T comPrams = TRDP_PD_DEFAULT_SEND_PARAM;
     #if PORT_FLAGS == TRDP_FLAGS_TSN
      comPrams.vlan = 1;
      comPrams.tsn = TRUE;
     #endif

      printf("  %3d: <%d> / %s / %4d / %3d ... ",
             i, p->comid, g_PdTypes[p->type], p->size, p->cycle / 1000);

         /* depending on port type */
      switch (p->type)
      {
         case PORT_PUSH:
            p->err = tlp_publish(apph,               /* session handle */
                                 &p->ph,             /* publish handle */
                                 NULL, NULL,
                                 0u,                 /* serviceId        */
                                 p->comid,           /* comid            */
                                 0,                  /* topo counter     */
                                 0,
                                 p->src,             /* source address   */
                                 p->dst,             /* destination address */
                                 p->cycle,           /* cycle period   */
                                 0,                  /* redundancy     */
                                 PORT_FLAGS,         /* flags          */
                                 &comPrams,          /* default send parameters */
                                 p->data,            /* data           */
                                 p->size);           /* data size      */

            if (p->err != TRDP_NO_ERR)
               printf("tlp_publish() failed, err: %d\n", p->err);
            else
               printf("ok\n");
            break;

         case PORT_PULL:
            p->err = tlp_publish(apph,               /* session handle */
                                 &p->ph,             /* publish handle */
                                 NULL, NULL,
                                 0u,                 /* serviceId        */
                                 p->comid,           /* comid            */
                                 0,                  /* topo counter     */
                                 0,
                                 p->src,             /* source address   */
                                 p->dst,             /* destination address */
                                 p->cycle,           /* cycle period   */
                                 0,                  /* redundancy     */
                                 TRDP_FLAGS_NONE,    /* flags          */
                                 NULL,               /* default send parameters */
                                 p->data,            /* data           */
                                 p->size);           /* data size      */

            if (p->err != TRDP_NO_ERR)
               printf("tlp_publish() failed, err: %d\n", p->err);
            else
               printf("ok\n");
            break;

         case PORT_REQUEST:
            p->err = tlp_request(apph,                     /* session handle */
                                 vsPort[p->link].sh,       /* related subscribe handle */
                                 0u,                       /* serviceId        */
                                 p->comid,                 /* comid          */
                                 0,                        /* topo counter   */
                                 0,
                                 p->src,                   /* source address */
                                 p->dst,                   /* destination address */
                                 0,                        /* redundancy     */
                                 TRDP_FLAGS_NONE,          /* flags          */
                                 NULL,                     /* default send parameters */
                                 p->data,                  /* data           */
                                 p->size,                  /* data size      */
                                 p->repid,                 /* reply comid    */
                                 p->rep);                  /* reply ip address  */

            if (p->err != TRDP_NO_ERR)
               printf("tlp_request() failed, err: %d\n", p->err);
            else
               printf("ok\n");
            break;

         case PORT_SINK:
            p->err = tlp_subscribe(apph,                   /* session handle   */
                                   &p->sh,                 /* subscribe handle */
                                   p,                      /* user ref         */
                                   _cbPortSink,            /* callback funktion */
                                   0u,                     /* serviceId        */
                                   p->comid,               /* comid            */
                                   0,                      /* topo counter     */
                                   0,
                                   p->src,                 /* source address   */
                                   VOS_INADDR_ANY,
                                   p->dst,                 /* destination address */
                                   TRDP_FLAGS_CALLBACK,    // callback is available
                                   NULL,                   /*    Receive params */
                                   p->timeout,             /* timeout [usec]   */
                                   TRDP_TO_SET_TO_ZERO,    /* timeout behavior */
                                   p->size);

            if (p->err != TRDP_NO_ERR)
               printf("tlp_subscribe() failed, err: %d\n", p->err);
            else
               printf("ok\n");
            break;

         case PORT_SINK_PUSH:
            p->err = tlp_subscribe(apph,                   /* session handle   */
                                   &p->sh,                 /* subscribe handle */
                                   NULL,                   /* user ref         */
                                   NULL,                   /* callback funktion */
                                   0u,                     /* serviceId        */
                                   p->comid,               /* comid            */
                                   0,                      /* topo counter     */
                                   0,
                                   p->src,                 /* source address   */
                                   VOS_INADDR_ANY,
                                   p->dst,                 /* destination address    */
                                   PORT_FLAGS,             /* No flags set     */
                                   &comPrams,              /*    Receive params */
                                   p->timeout,             /* timeout [usec]   */
                                   TRDP_TO_SET_TO_ZERO,    /* timeout behavior */
                                   p->size);

            if (p->err != TRDP_NO_ERR)
               printf("tlp_subscribe() failed, err: %d\n", p->err);
            else
               printf("ok\n");
            break;
      }
   }
}

/* --- test data processing --------------------------------------------------*/

void process_data (TRDP_APP_SESSION_T apph, Port vsPort[], unsigned int uiNports, Type eType)
{
   static int w = 80;
   unsigned i;
   unsigned n;
   TRDP_COM_PARAM_T comPrams = TRDP_PD_DEFAULT_SEND_PARAM;
  #if PORT_FLAGS == TRDP_FLAGS_TSN
   comPrams.vlan = 1;
   comPrams.tsn = TRUE;
  #endif

  #if 0
    /* get terminal size */
   if (get_term_size(&_w, &_h) == 0)
   {   /* changed width? */
      if (_w != w || !_uiCycle)
         clear_screen();
      else
         cursor_home();
      w = _w;
   }
   else
   {
      if (!_uiCycle)
         clear_screen();
      else
         cursor_home();
   }
  #endif

   /* go through ports one-by-one */
   for (i = 0; i < uiNports; ++i)
   {
      Port* p = &vsPort[i];

      /* write port data */
      if (p->type == PORT_PUSH || p->type == PORT_PULL)
      {
         if (p->link == -1)
         {
            /* data generator */
            int iHeaderLen;
            int iBodyLen;
            int iWOffset;

            iHeaderLen = snprintf((char*)p->data, p->size,
                                   "<%s/%d.%d.%d.%d->%d.%d.%d.%d/%dms/%dB:%03d>",
                                   p->type == PORT_PUSH ? "Pd" : "Pp",
                                   (p->src >> 24) & 0xff, (p->src >> 16) & 0xff,
                                   (p->src >> 8) & 0xff, p->src & 0xff,
                                   (p->dst >> 24) & 0xff, (p->dst >> 16) & 0xff,
                                   (p->dst >> 8) & 0xff, p->dst & 0xff,
                                   p->cycle / 1000, p->size, _uiCycle);
            iBodyLen = p->size - iHeaderLen - 1;
            memset(&p->data[iHeaderLen], ' ', iBodyLen);
            iWOffset = (_uiCycle % iBodyLen);
            p->data[iHeaderLen + iWOffset] = '0' + (iWOffset % 10);
            p->data[p->size - 1] = 0;
         }
         else
         {   /* echo data from incoming port, replace all '_' by '~' */
            unsigned char* src = vsPort[p->link].data;
            unsigned char* dst = p->data;
            for (n = p->size; n; --n, ++src, ++dst)
               *dst = (*src == '_') ? '~' : *src;
         }

        #if PORT_FLAGS == TRDP_FLAGS_TSN
         if (p->type == PORT_PUSH)
         {
            p->err = tlp_putImmediate(apph, p->ph, p->data, p->size, 0);
         }
         else
         {
            p->err = tlp_put(apph, p->ph, p->data, p->size);
         }
        #else
         p->err = tlp_put(apph, p->ph, p->data, p->size);
        #endif
      }
      else if (p->type == PORT_REQUEST)
      {
         unsigned o = _uiCycle % 128;
         memset(p->data, '_', p->size);
         if (o < p->size)
         {
            snprintf((char*)p->data + o, p->size - o,
                     "<Pr/%d.%d.%d.%d->%d.%d.%d.%d/%dms/%db:%d>",
                     (p->src >> 24) & 0xff, (p->src >> 16) & 0xff,
                     (p->src >> 8) & 0xff, p->src & 0xff,
                     (p->dst >> 24) & 0xff, (p->dst >> 16) & 0xff,
                     (p->dst >> 8) & 0xff, p->dst & 0xff,
                     p->cycle / 1000, p->size, _uiCycle);
         }

         p->err = tlp_request(apph, vsPort[p->link].sh, 0u, p->comid, 0u, 0u,
                              p->src, p->dst, 0, PORT_FLAGS, &comPrams, p->data, p->size,
                              p->repid, p->rep);
      }

      /* print port data */
      if (p->type != PORT_SINK)
         _PortPrintData(p, p->data, 0);
   }

   /* increment _uiCycle counter  */
   ++_uiCycle;
}

static void _PortPrintData (Port* p, UINT8* pData, UINT32 dataSize)
{
   char* pszEscColor;
   char* pszEscColorDef = "[0m";

   printf("%s : ", vos_getTimeStamp());

   if (vos_isMulticast(p->dst) || vos_isMulticast(p->src))
      pszEscColor = "[0;1;34m";
   else
      pszEscColor = pszEscColorDef;

   printf("\033%s%5d ", pszEscColor , p->comid);

   printf("\033%s%s ", pszEscColorDef, g_PdTypes[p->type]);

   if (p->err != TRDP_NO_ERR)
      pszEscColor = "[0;1;31m";
   else
      pszEscColor = "[0;1;32m";

   printf("\033%s( %3d) ", pszEscColor, p->err);

   if (p->err == TRDP_NO_ERR)
   {
      if (pData != NULL)
         printf("\033%s[%s]\n", pszEscColorDef, pData);
      else
         printf("\033%s[NULL]\n", pszEscColorDef);
   }
   else
   {
      printf("\033%s[%s]\n", pszEscColorDef, _get_result_string(p->err));
   }

   fflush(stdout);
}


#include <assert.h>
static void _cbPortSink (      void*              pRefCon,
                               TRDP_APP_SESSION_T appHandle,
                         const TRDP_PD_INFO_T*    pMsg,
                               UINT8*             pData,
                               UINT32             dataSize)
{
   static int err = 0;
   Port* psPort;

   assert(pMsg != NULL);
   if (pMsg != NULL)
   {
      psPort = (Port*)pMsg->pUserRef;
      psPort->err = pMsg->resultCode;

      if (pMsg->resultCode != TRDP_TIMEOUT_ERR)
      {
         if (psPort->src != 0)
         {
            if (psPort->src != pMsg->srcIpAddr)
            {
               fprintf(stderr, "Warning, different ip source file "
                               "prev: %d.%d.%d.%d - new: %d.%d.%d.%d\n",
                               (psPort->src      >> 24) & 0xff, (psPort->src     >> 16) & 0xff,
                               (psPort->src      >>  8) & 0xff,  psPort->src            & 0xff,
                               (pMsg->srcIpAddr  >> 24) & 0xff, (pMsg->srcIpAddr >> 16) & 0xff,
                               (pMsg->srcIpAddr  >>  8) & 0xff,  pMsg->srcIpAddr        & 0xff);
            }
         }
         psPort->src = pMsg->srcIpAddr;
      }

      switch (pMsg->resultCode)
      {
         case TRDP_NO_ERR:
            // not needed tlp_get can be used to access buffer
            // memcpy(psPort->data, pData, dataSize);
            // 
            // TODO : Send data to the circular buffer
           #if 0
            xoCBuff_Alloc(psPort->psCbuff, pData, dataSize);
            xoCBuff_Send()
           #endif
            err--;
            break;

         case TRDP_PD_SIZE_ERR:
            fprintf(stderr, "Error, size expected: %d - received %d\n",
                            psPort->size, dataSize);
            break;

         case TRDP_TIMEOUT_ERR:
            break;

         default:
            err++;
            break;
      }

      _PortPrintData(psPort, pData, dataSize);
   }
}


/* --- poll received data ------------------------------------------------------------- */
void poll_data (TRDP_APP_SESSION_T apph, Port vsPort[], unsigned int uiNports)
{
   TRDP_PD_INFO_T pdi;
   unsigned i;

   /* go through ports one-by-one */
   for (i = 0; i < uiNports; ++i)
   {
      Port* p = &vsPort[i];
      UINT32 size = p->size;
      if (p->type == PORT_SINK || p->type == PORT_SINK_PUSH)
      {
         p->err = tlp_get(apph, p->sh, &pdi, p->data, &size);
      }
   }
}

/* ------------------------------------------------------------------------------------ */

static FILE* _pLogFile = NULL;

const TRDP_PRINT_DBG_T LogOpen (const char* pszName)
{
   _pLogFile = fopen(pszName, "w+");
   if (_pLogFile != NULL)
      return LogPrint;
   return NULL;
}

void LogPrint (void*        pRefCon,
               VOS_LOG_T    category,
               const CHAR8* pTime,
               const CHAR8* pFile,
               UINT16       line,
               const CHAR8* pMsgStr)
{
   const CHAR8* pFileName;
   const char*  pLev;

   if (_pLogFile != NULL)
   {
     #ifdef _WIN32
      pFileName = strrchr(pFile, '\\');
     #else
      pFile = strrchr(pFile, '/');
     #endif
      if (pFileName != NULL)
      {
         pFileName++;
         pFile = pFileName;
      }

      switch (category)
      {
         case     VOS_LOG_ERROR:   pLev = "{E}"; break;
         case     VOS_LOG_WARNING: pLev = " W "; break;
         case     VOS_LOG_INFO:    pLev = " I "; break;
         default:                  pLev = " D ";
      }

      fprintf(_pLogFile, "%s %-15s(%5d) %s",
              pLev, pFile, (int)line, pMsgStr);

      fflush(_pLogFile);
   }
}

/* --- convert trdp error code to string -------------------------------------*/

static const char* _get_result_string (int ret)
{
   static char buf[128];

   switch (ret)
   {
      case TRDP_NO_ERR:
         return "TRDP_NO_ERR (no error)";
      case TRDP_PARAM_ERR:
         return "TRDP_PARAM_ERR (parameter missing or out of range)";
      case TRDP_INIT_ERR:
         return "TRDP_INIT_ERR (call without valid initialization)";
      case TRDP_NOINIT_ERR:
         return "TRDP_NOINIT_ERR (call with invalid handle)";
      case TRDP_TIMEOUT_ERR:
         return "TRDP_TIMEOUT_ERR (timeout)";
      case TRDP_NODATA_ERR:
         return "TRDP_NODATA_ERR (non blocking mode: no data received)";
      case TRDP_SOCK_ERR:
         return "TRDP_SOCK_ERR (socket error / option not supported)";
      case TRDP_IO_ERR:
         return "TRDP_IO_ERR (socket IO error, data can't be received/sent)";
      case TRDP_MEM_ERR:
         return "TRDP_MEM_ERR (no more memory available)";
      case TRDP_SEMA_ERR:
         return "TRDP_SEMA_ERR semaphore not available)";
      case TRDP_QUEUE_ERR:
         return "TRDP_QUEUE_ERR (queue empty)";
      case TRDP_QUEUE_FULL_ERR:
         return "TRDP_QUEUE_FULL_ERR (queue full)";
      case TRDP_MUTEX_ERR:
         return "TRDP_MUTEX_ERR (mutex not available)";
      case TRDP_NOSESSION_ERR:
         return "TRDP_NOSESSION_ERR (no such session)";
      case TRDP_SESSION_ABORT_ERR:
         return "TRDP_SESSION_ABORT_ERR (Session aborted)";
      case TRDP_NOSUB_ERR:
         return "TRDP_NOSUB_ERR (no subscriber)";
      case TRDP_NOPUB_ERR:
         return "TRDP_NOPUB_ERR (no publisher)";
      case TRDP_NOLIST_ERR:
         return "TRDP_NOLIST_ERR (no listener)";
      case TRDP_CRC_ERR:
         return "TRDP_CRC_ERR (wrong CRC)";
      case TRDP_WIRE_ERR:
         return "TRDP_WIRE_ERR (wire error)";
      case TRDP_TOPO_ERR:
         return "TRDP_TOPO_ERR (invalid topo count)";
      case TRDP_COMID_ERR:
         return "TRDP_COMID_ERR (unknown comid)";
      case TRDP_STATE_ERR:
         return "TRDP_STATE_ERR (call in wrong state)";
      case TRDP_APP_TIMEOUT_ERR:
         return "TRDP_APPTIMEOUT_ERR (application timeout)";
      case TRDP_PD_SIZE_ERR:
         return "TRDP_PD_SIZE_ERR (unexpected size)";
      case TRDP_UNKNOWN_ERR:
         return "TRDP_UNKNOWN_ERR (unspecified error)";
   }
   sprintf(buf, "unknown error (%d)", ret);
   return buf;
}

