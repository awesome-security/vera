#ifndef __CONSTS__
#define __CONSTS__

//
//      This file contains definition and consts shared among all debugger clients and servers
//
//

#define TIMEOUT         (1000/25)       // in milliseconds, timeout for polling

// flags for start_process::flags
#define DBG_PROC_IS_DLL 0x01  // database contains a dll (not exe)
#define DBG_PROC_IS_GUI 0x02  // using gui version of ida
#define INFINITY -1

// the idarpc_stream_struct_t structure is not defined.
// it is used as an opaque type provided by the transport level.
// the transport level defines its own local type for it.
typedef struct idarpc_stream_struct_t idarpc_stream_t;

// bidirectional codes (client <-> server)
#define RPC_OK    0      // response: function call succeeded
#define RPC_UNK   1      // response: unknown function code
#define RPC_MEM   2      // response: no memory

#define RPC_OPEN  3      // server->client: i'm ready, the very first packet

#define RPC_EVENT 4      // server->client: debug event ready, followed by debug_event
#define RPC_EVOK  5      // client->server: event processed (in response to RPC_EVENT)
// we need EVOK to handle the situation when the debug
// event was detected by the server during polling and
// was sent to the client using RPC_EVENT but client has not received it yet
// and requested GET_DEBUG_EVENT. In this case we should not
// call remote_get_debug_event() but instead force the client
// to use the event sent by RPC_EVENT.
// In other words, if the server has sent RPC_EVENT but has not
// received RPC_EVOK, it should fail all GET_DEBUG_EVENTS.

// client->server codes
#define RPC_INIT                      10
#define RPC_TERM                      11
#define RPC_GET_PROCESS_INFO          12
#define RPC_DETACH_PROCESS            13
#define RPC_START_PROCESS             14
#define RPC_GET_DEBUG_EVENT           15
#define RPC_ATTACH_PROCESS            16
#define RPC_PREPARE_TO_PAUSE_PROCESS  17
#define RPC_EXIT_PROCESS              18
#define RPC_CONTINUE_AFTER_EVENT      19
#define RPC_STOPPED_AT_DEBUG_EVENT    20
#define RPC_TH_SUSPEND                21
#define RPC_TH_CONTINUE               22
#define RPC_TH_SET_STEP               23
#define RPC_READ_REGS                 24
#define RPC_GET_MEMORY_INFO           25
#define RPC_READ_MEMORY               26
#define RPC_WRITE_MEMORY              27
#define RPC_ISOK_BPT                  28
#define RPC_ADD_BPT                   29
#define RPC_DEL_BPT                   30
#define RPC_WRITE_REG                 31
#define RPC_GET_SREG_BASE             32
#define RPC_SET_EXCEPTION_INFO        33

#define RPC_OPEN_FILE                 34
#define RPC_CLOSE_FILE                35
#define RPC_READ_FILE                 36
#define RPC_WRITE_FILE                38
#define RPC_IOCTL                     39 // both client and the server may send this packet
#define RPC_UPDATE_CALL_STACK         40
#define RPC_APPCALL                   41
#define RPC_CLEANUP_APPCALL           42

// server->client codes
#define RPC_SET_DEBUG_NAMES           50
#define RPC_SYNC_STUB                 51
#define RPC_ERROR                     52
#define RPC_MSG                       53
#define RPC_WARNING                   54

// Polling flags
#define PRF_DONT_POLL  0x00 // Do not poll for events
#define PRF_POLL       0x01 // May poll for events
#define PRF_LOGIN      0x02 // Do not process client requests
#define PRF_DONT_BLOCK 0x04 // Do not block reads

#pragma pack(push, 1)

struct PACKED rpc_packet_t
{                        // fields are always sent in the network order
  uint32 length;         // length of the packet (do not count length & code)
  uchar code;            // function code
};
CASSERT(sizeof(rpc_packet_t) == 5);
#pragma pack(pop)

// Compile time defined error reporting functions
AS_PRINTF(2, 0) void    dmsg(void *, const char *format, va_list va);
AS_PRINTF(2, 0) void    derror(void *, const char *format, va_list va);
AS_PRINTF(2, 0) void    dwarning(void *, const char *format, va_list va);
AS_PRINTF(3, 0) ssize_t dvmsg(int code, void *ud, const char *format, va_list va);

// We use this to declare reporting functions with a given user data
#define DECLARE_UD_REPORTING(fnc, ud) \
  AS_PRINTF(2, 3) void d##fnc(const char *format, ...) \
  { \
    va_list va; \
    va_start(va, format); \
    ::d##fnc(ud, format, va); \
    va_end(va); \
  }

// IDC function name that is exported by a debugger module
// to allow scripts to send debugger commands
#define IDC_SENDDBG_CMD "SendDbgCommand"

#endif
