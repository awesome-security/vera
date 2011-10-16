#ifndef __RPC_CLIENT__
#define __RPC_CLIENT__

#include "rpc_engine.h"

class rpc_client_t: public rpc_engine_t
{
protected:
  debug_event_t pending_event;

public:
  virtual qstring perform_request(const rpc_packet_t *rp);
  virtual int poll_events(bool idling);
  rpc_client_t(SOCKET rpc_socket);
};

#endif
