
#include "trdp_if_light.h"

#ifdef TSN_SUPPORT
#define PORT_FLAGS TRDP_FLAGS_TSN
#else
#define PORT_FLAGS TRDP_FLAGS_NONE
#endif

typedef enum
{
   PORT_PUSH,                      /* outgoing port ('Pd'/push)   (TSN suppport)*/
   PORT_PULL,                      /* outgoing port ('Pp'/pull)   */
   PORT_REQUEST,                   /* outgoing port ('Pr'/request)*/
   PORT_SINK,                      /* incoming port               */
   PORT_SINK_PUSH,                 /* incoming port for pushed messages (TSN suppport)*/
} Type;


typedef struct
{
   Type           type;                             /* port type */
   TRDP_ERR_T     err;                              /* put/get status */
   TRDP_PUB_T     ph;                               /* publish handle */
   TRDP_SUB_T     sh;                               /* subscribe handle */
   UINT32         comid;                            /* comid            */
   UINT32         repid;                            /* reply comid (for PULL requests) */
   UINT32         size;                             /* size                            */
   TRDP_IP_ADDR_T src;                              /* source ip address               */
   TRDP_IP_ADDR_T dst;                              /* destination ip address          */
   TRDP_IP_ADDR_T rep;                              /* reply ip address (for PULL requests) */
   UINT32         cycle;                            /* cycle                                */
   UINT32         timeout;                          /* timeout (for SINK ports)             */
   unsigned       char data[TRDP_MAX_PD_DATA_SIZE]; /* data buffer                          */
   int            link;                             /* index of linked port (echo or subscribe) */
} Port;


extern const char* g_PdTypes[];



const TRDP_PRINT_DBG_T LogOpen (const char* pszName);

void LogPrint (void*        pRefCon,
               VOS_LOG_T    category,
               const CHAR8* pTime,
               const CHAR8* pFile,
               UINT16       line,
               const CHAR8* pMsgStr);


void setup_ports  (TRDP_APP_SESSION_T apph, Port vsPort[], unsigned int uiNports);
void process_data (TRDP_APP_SESSION_T apph, Port vsPort[], unsigned int uiNports, Type eType);
void poll_data    (TRDP_APP_SESSION_T apph, Port vsPort[], unsigned int uiNports);
