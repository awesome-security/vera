#ifndef __RPC_SERVER__
#define __RPC_SERVER__

#define VERBOSE_ENABLED
#include "rpc_engine.h"

class rpc_server_t: public rpc_engine_t
{
private:
  debug_event_t ev;
  debug_event_t pending_event;
  debmod_t *dbg_mod;
  FILE *channels[16];
protected:
  void close_all_channels();
  void clear_channels();
  int find_free_channel();
  int pass_debug_names(const ea_t *ea, const char *const *names, int qty);
public:
  void set_debugger_instance(debmod_t *instance);
  debmod_t *get_debugger_instance();
  bool rpc_sync_stub(const char *server_stub_name, const char *ida_stub_name);
  virtual qstring perform_request(const rpc_packet_t *rp);
  virtual int poll_events(bool idling);
  virtual ~rpc_server_t();
  rpc_server_t(SOCKET rpc_socket);
};

// defined only in the single threaded version of the server:
extern rpc_server_t *g_global_server;

#endif
