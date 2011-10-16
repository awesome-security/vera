#include <pro.h>
#include <err.h>
#include <typeinf.hpp>
#include "rpc_server.h"

//--------------------------------------------------------------------------
// another copy of this function (for local debugging) is defined in common_local_impl.cpp
int send_ioctl(
  void *ud,
  int fn,
  const void *buf,
  size_t size,
  void **poutbuf,
  ssize_t *poutsize)
{
  rpc_engine_t *srv = (rpc_engine_t *)ud;
  return srv->send_ioctl(fn, buf, size, poutbuf, poutsize);
}

//--------------------------------------------------------------------------
ssize_t dvmsg(int code, void *ud, const char *format, va_list va)
{
  if ( code == 0 )
    code = RPC_MSG;
  else if ( code >  0 )
    code = RPC_WARNING;
  else
    code = RPC_ERROR;

  rpc_server_t *srv = (rpc_server_t *)ud;

  qstring cmd = prepare_rpc_packet((uchar)code);

  char buf[MAXSTR];
  qvsnprintf(buf, sizeof(buf), format, va);
  append_str(cmd, buf);

  qfree(srv->process_request(cmd));
  if (code < 0)
  {
    exit(1);
    /*
    inline void serror(const char *format, ...)
    {
      va_list va;
      va_start(va, format);
      rpc_svmsg(-1, format, va);
      va_end(va);
      exit(1);
    }
    */
  }
  return strlen(buf);
}

//--------------------------------------------------------------------------
debmod_t *rpc_server_t::get_debugger_instance()
{
  return dbg_mod;
}

//--------------------------------------------------------------------------
rpc_server_t::~rpc_server_t()
{
  delete dbg_mod;
  clear_channels();
}

//--------------------------------------------------------------------------
void rpc_server_t::set_debugger_instance(debmod_t *instance)
{
  dbg_mod = instance;
  dbg_mod->dmsg_ud = (void *)this;
}

//--------------------------------------------------------------------------
void rpc_server_t::close_all_channels()
{
  for ( int i=0; i < qnumber(channels); i++ )
    if ( channels[i] != NULL )
      qfclose(channels[i]);

  clear_channels();
}

//--------------------------------------------------------------------------
void rpc_server_t::clear_channels()
{
  memset(channels, 0, sizeof(channels));
}

//--------------------------------------------------------------------------
int rpc_server_t::find_free_channel()
{
  for (int i=0; i < qnumber(channels); i++)
    if ( channels[i] == NULL )
      return i;
  return -1;
}

//--------------------------------------------------------------------------
static int ioctl_serv_handler(
    rpc_engine_t *rpc,
    int fn,
    const void *buf,
    size_t size,
    void **poutbuf,
    ssize_t *poutsize)
{
  rpc_server_t *serv = (rpc_server_t *)rpc;
  return serv->get_debugger_instance()->handle_ioctl(fn, buf, size, poutbuf, poutsize);
}

//--------------------------------------------------------------------------
rpc_server_t::rpc_server_t(SOCKET rpc_socket): rpc_engine_t(rpc_socket)
{
  dbg_mod = NULL;
  poll_required = true;
  clear_channels();
  memset(&ev, 0, sizeof(debug_event_t));
  ioctl_handler = ioctl_serv_handler;
}

//--------------------------------------------------------------------------
// debug names are transfered remotely
int rpc_server_t::pass_debug_names(const ea_t *ea, const char *const *names, int qty)
{
  if ( qty == 0 )
    return RPC_OK;

  qstring cmd, buf;

  const ea_t *pea = ea;
  const char *const *pnames = names;

  const size_t SZPACKET = 1300;

  while ( qty > 0 )
  {
    buf.qclear();

    ea_t old = 0;
    const char *optr = "";

    // Start appending names and EAs
    int i = 0;
    while ( i < qty )
    {
      adiff_t diff = *pea - old;
      bool neg = diff < 0;
      if ( neg )
        diff = -diff;

      append_ea(buf, diff);
      append_long(buf, neg);

      old = *pea;
      const char *nptr = *pnames;
      int len = 0;

      while ( nptr[len] != '\0' && nptr[len] == optr[len] )
        len++;

      append_long(buf, len);
      append_str(buf, nptr+len);
      optr = nptr;
      pea++;
      pnames++;
      i++;

      if ( buf.size() > SZPACKET )
        break;
    }
    qty -= i;

    cmd = prepare_rpc_packet(RPC_SET_DEBUG_NAMES);
    append_long(cmd, i);
    cmd.append(&buf[0], buf.size());

    // should return a qty as much as sent...if not probably network error!
    if ( i != process_long(cmd) )
      return RPC_UNK;
  }

  return RPC_OK;
}

//--------------------------------------------------------------------------
// performs requests on behalf of a remote client
// client -> server
qstring rpc_server_t::perform_request(const rpc_packet_t *rp)
{
  const uchar *ptr = (const uchar *)(rp + 1);
  const uchar *end = ptr + rp->length;
  qstring cmd = prepare_rpc_packet(RPC_OK);
  switch ( rp->code )
  {
    case RPC_INIT:
      {
        bool debug_debugger = extract_long(&ptr, end);
        verbose = debug_debugger;

        int result = dbg_mod->dbg_init(debug_debugger);
        verb(("init(debug_debugger=%d) => %d\n", debug_debugger, result));
        append_long(cmd, result);
      }
      break;

    case RPC_TERM:
      dbg_mod->dbg_term();
      verb(("term()\n"));
      break;

    case RPC_GET_PROCESS_INFO:
      {
        process_info_t info;
        int n = extract_long(&ptr, end);
        char *input = NULL;
        if ( n == 0 )
          input = extract_str(&ptr, end);
        bool result = dbg_mod->dbg_process_get_info(n, input, &info);
        append_long(cmd, result);
        if ( result )
          append_process_info(cmd, &info);
        verb(("get_process_info(n=%d) => %d\n", n, result));
      }
      break;

    case RPC_DETACH_PROCESS:
      {
        bool result = dbg_mod->dbg_detach_process();
        append_long(cmd, result);
        verb(("detach_process() => %d\n", result));
      }
      break;

    case RPC_START_PROCESS:
      {
        char *path = extract_str(&ptr, end);
        char *args = extract_str(&ptr, end);
        char *sdir = extract_str(&ptr, end);
        int flags  = extract_long(&ptr, end);
        char *input= extract_str(&ptr, end);
        uint32 crc32= extract_long(&ptr, end);
        int result = dbg_mod->dbg_start_process(path, args, sdir, flags, input, crc32);
        verb(("start_process(path=%s args=%s flags=%s%s\n"
          "              sdir=%s\n"
          "              input=%s crc32=%x) => %d\n",
          path, args,
          flags & DBG_PROC_IS_DLL ? " is_dll" : "",
          flags & DBG_PROC_IS_GUI ? " under_gui" : "",
          sdir,
          input, crc32,
          result));
        append_long(cmd, result);
      }
      break;

    case RPC_GET_DEBUG_EVENT:
      {
        bool ida_is_idle = extract_long(&ptr, end);
        int result = has_pending_event ? 0 : dbg_mod->dbg_get_debug_event(&ev, ida_is_idle);
        append_long(cmd, result);
        if ( result == 1 )
        {
          append_debug_event(cmd, &ev);
          verb(("got event: %s\n", dbg_mod->debug_event_str(&ev)));
        }
        else if ( !has_pending_event )
          poll_debug_events = true;
        //        verb(("get_debug_event(ida_is_idle=%d) => %d (has_pending=%d, poll=%d)\n", ida_is_idle, result, has_pending_event, poll_debug_events));
        verbev(("get_debug_event(ida_is_idle=%d) => %d (has_pending=%d, poll=%d)\n", ida_is_idle, result, has_pending_event, poll_debug_events));
      }
      break;

    case RPC_ATTACH_PROCESS:
      {
        pid_t pid = extract_long(&ptr, end);
        int event_id = extract_long(&ptr, end);
        bool result = dbg_mod->dbg_attach_process(pid, event_id);
        verb(("attach_process(pid=%u, evid=%d) => %d\n", pid, event_id, result));
        append_long(cmd, result);
      }
      break;

    case RPC_PREPARE_TO_PAUSE_PROCESS:
      {
        bool result = dbg_mod->dbg_prepare_to_pause_process();
        verb(("prepare_to_pause_process() => %d\n", result));
        append_long(cmd, result);
      }
      break;

    case RPC_EXIT_PROCESS:
      {
        bool result = dbg_mod->dbg_exit_process();
        verb(("exit_process() => %d\n", result));
        append_long(cmd, result);
      }
      break;

    case RPC_CONTINUE_AFTER_EVENT:
      {
        extract_debug_event(&ptr, end, &ev);
        int result = dbg_mod->dbg_continue_after_event(&ev);
        verb(("continue_after_event(...) => %d\n", result));
        append_long(cmd, result);
      }
      break;

    case RPC_STOPPED_AT_DEBUG_EVENT:
      {
        dbg_mod->dbg_stopped_at_debug_event();
        name_info_t *ni = dbg_mod->get_debug_names();
        int err = RPC_OK;
        if (ni != NULL)
          err = pass_debug_names(&ni->addrs[0], &ni->names[0], (int)ni->addrs.size());
        verb(("stopped_at_debug_event => %s\n", get_rpc_name(err)));
        break;
      }

    case RPC_TH_SUSPEND:
      {
        thid_t tid = extract_long(&ptr, end);
        bool result = dbg_mod->dbg_thread_suspend(tid);
        verb(("thread_suspend(tid=%d) => %d\n", tid, result));
        append_long(cmd, result);
      }
      break;

    case RPC_TH_CONTINUE:
      {
        thid_t tid = extract_long(&ptr, end);
        bool result = dbg_mod->dbg_thread_continue(tid);
        verb(("thread_continue(tid=%d) => %d\n", tid, result));
        append_long(cmd, result);
      }
      break;

    case RPC_TH_SET_STEP:
      {
        thid_t tid = extract_long(&ptr, end);
        bool result = dbg_mod->dbg_thread_set_step(tid);
        verb(("thread_set_step(tid=%d) => %d\n", tid, result));
        append_long(cmd, result);
      }
      break;

    case RPC_READ_REGS:
      {
        thid_t tid = extract_long(&ptr, end);
        int nregs = extract_long(&ptr, end);
        regval_t *values = new regval_t[nregs];
        if ( values == NULL ) goto nomem;
        bool result = dbg_mod->dbg_thread_read_registers(tid, values, nregs);
        verb(("thread_read_regs(tid=%d) => %d\n", tid, result));
        append_long(cmd, result);
        append_regvals(cmd, values, nregs);
        delete values;
      }
      break;

    case RPC_WRITE_REG:
      {
        thid_t tid = extract_long(&ptr, end);
        int reg_idx = extract_long(&ptr, end);
        regval_t value;
        extract_regvals(&ptr, end, &value, 1);
        bool result = dbg_mod->dbg_thread_write_register(tid, reg_idx, &value);
        verb(("thread_write_reg(tid=%d) => %d\n", tid, result));
        append_long(cmd, result);
      }
      break;

    case RPC_GET_SREG_BASE:
      {
        thid_t tid = extract_long(&ptr, end);
        int sreg_value = extract_long(&ptr, end);
        ea_t ea;
        bool result = dbg_mod->dbg_thread_get_sreg_base(tid, sreg_value, &ea);
        verb(("get_thread_sreg_base(tid=%d, %d) => %a\n", tid, sreg_value, result ? ea : BADADDR));
        append_long(cmd, result);
        if ( result )
          append_ea(cmd, ea);
      }
      break;

    case RPC_SET_EXCEPTION_INFO:
      {
        int qty = extract_long(&ptr, end);
        exception_info_t *extable = extract_exception_info(&ptr, end, qty);
        dbg_mod->dbg_set_exception_info(extable, qty);
        verb(("set_exception_info(qty=%u)\n", qty));
      }
      break;

    case RPC_GET_MEMORY_INFO:
      {
        meminfo_vec_t areas;
        int result = dbg_mod->dbg_get_memory_info(areas);
        int qty = areas.size();
        verb(("get_memory_info() => %d (qty=%d)\n", result, qty));
        append_long(cmd, result);
        if ( result > 0 )
        {
          append_long(cmd, qty);
          for ( int i=0; i < qty; i++ )
            append_memory_info(cmd, &areas[i]);
        }
      }
      break;

    case RPC_READ_MEMORY:
      {
        ea_t ea = extract_ea(&ptr, end);
        size_t size = extract_long(&ptr, end);
        uchar *buf = new uchar[size];
        if ( buf == NULL ) goto nomem;
        ssize_t result = dbg_mod->dbg_read_memory(ea, buf, size);
        verb(("read_memory(ea=%a size=%ld) => %ld", ea, size, result));
        if ( result && size == 1 )
          verb((" (0x%02X)\n", *buf));
        else
          verb(("\n"));
        append_long(cmd, uint32(result));
        append_memory(cmd, buf, size);
        delete buf;
      }
      break;

    case RPC_WRITE_MEMORY:
      {
        ea_t ea = extract_ea(&ptr, end);
        size_t size = extract_long(&ptr, end);
        uchar *buf = new uchar[size];
        if ( buf == NULL ) goto nomem;
        extract_memory(&ptr, end, buf, size);
        ssize_t result = dbg_mod->dbg_write_memory(ea, buf, size);
        verb(("write_memory(ea=%a size=%ld) => %ld", ea, size, result));
        if ( result && size == 1 )
          verb((" (0x%02X)\n", *buf));
        else
          verb(("\n"));
        append_long(cmd, uint32(result));
        delete buf;
      }
      break;

    case RPC_ISOK_BPT:
      {
        bpttype_t type = extract_long(&ptr, end);
        ea_t ea        = extract_ea(&ptr, end);
        int len        = extract_long(&ptr, end);
        int result  = dbg_mod->dbg_is_ok_bpt(type, ea, len);
        verb(("isok_bpt(type=%d ea=%a len=%d) => %d\n", type, ea, len, result));
        append_long(cmd, result);
      }
      break;

    case RPC_ADD_BPT:
      {
        bpttype_t type = extract_long(&ptr, end);
        ea_t ea        = extract_ea(&ptr, end);
        int len        = extract_long(&ptr, end);
        bool result = dbg_mod->dbg_add_bpt(type, ea, len);
        verb(("add_bpt(type=%d ea=%a len=%d) => %d\n", type, ea, len, result));
        append_long(cmd, result);
      }
      break;

    case RPC_DEL_BPT:
      {
        ea_t ea  = extract_ea(&ptr, end);
        int size = extract_long(&ptr, end);
        uchar *buf = NULL;
        if ( size != 0 )
        {
          buf = new uchar[size];
          if ( buf == NULL ) goto nomem;
          extract_memory(&ptr, end, buf, size);
        }
        bool result = dbg_mod->dbg_del_bpt(ea, buf, size);
        verb(("del_bpt(ea=%a) => %d\n", ea, result));
        append_long(cmd, result);
        delete buf;
      }
      break;

    case RPC_OPEN_FILE:
      {
        char *file = extract_str(&ptr, end);
        bool readonly = extract_long(&ptr, end);
        uint32 fsize = 0;
        int fn = find_free_channel();
        if ( fn != -1 )
        {
          channels[fn] = (readonly ? fopenRB : fopenWB)(file);
          if ( channels[fn] == NULL )
            fn = -1;
          else if ( readonly )
            fsize = efilelength(channels[fn]);
        }
        verb(("open_file('%s', %d) => %d %d\n", file, readonly, fn, fsize));
        append_long(cmd, fn);
        if ( fn != -1 )
          append_long(cmd, fsize);
        else
          append_long(cmd, qerrcode());
      }
      break;

    case RPC_CLOSE_FILE:
      {
        int fn = extract_long(&ptr, end);
        if ( fn >= 0 && fn < qnumber(channels) )
        {
#ifdef __UNIX__
          // set mode 0755 for unix applications
          fchmod(fileno(channels[fn]), S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
#endif
          qfclose(channels[fn]);
          channels[fn] = NULL;
        }
        verb(("close_file(%d)\n", fn));
      }
      break;

    case RPC_READ_FILE:
      {
        char *buf  = NULL;
        int fn     = extract_long(&ptr, end);
        int32 off  = extract_long(&ptr, end);
        int32 size = extract_long(&ptr, end);
        int32 s2 = 0;
        if ( size > 0 )
        {
          buf = new char[size];
          if ( buf == NULL )
            goto nomem;
          qfseek(channels[fn], off, SEEK_SET);
          s2 = qfread(channels[fn], buf, size);
        }
        append_long(cmd, s2);
        if ( size != s2 )
          append_long(cmd, qerrcode());
        if ( s2 > 0 )
          append_memory(cmd, buf, s2);
        delete[] buf;
        verb(("read_file(%d, 0x%X, %d) => %d\n", fn, off, size, s2));
      }
      break;

    case RPC_WRITE_FILE:
      {
        char *buf  = NULL;
        int fn      = extract_long(&ptr, end);
        uint32 off  = extract_long(&ptr, end);
        uint32 size = extract_long(&ptr, end);
        if ( size > 0 )
        {
          buf = new char[size];
          if ( buf == NULL )
            goto nomem;
          extract_memory(&ptr, end, buf, size);
        }
        qfseek(channels[fn], off, SEEK_SET);
        uint32 s2 = qfwrite(channels[fn], buf, size);
        append_long(cmd, size);
        if ( size != s2 )
          append_long(cmd, qerrcode());
        delete buf;
        verb(("write_file(%d, 0x%X, %d) => %d\n", fn, off, size, s2));
      }
      break;

    case RPC_EVOK:
      has_pending_event = false;
      cmd = "";
      verbev(("got evok, clearing has_pending_event\n"));
      break;

    case RPC_IOCTL:
      {
        int code = handle_ioctl_packet(cmd, ptr, end);
        if ( code != RPC_OK )
          return prepare_rpc_packet((uchar)code);
      }
      break;

    case RPC_UPDATE_CALL_STACK:
      {
        call_stack_t trace;
        thid_t tid = extract_long(&ptr, end);
        bool ok = dbg_mod->dbg_update_call_stack(tid, &trace);
        append_long(cmd, ok);
        if ( ok )
          append_call_stack(cmd, trace);
      }
      break;

    case RPC_APPCALL:
      {
        ea_t func_ea = extract_ea(&ptr, end);
        thid_t tid   = extract_long(&ptr, end);
        int nargs    = extract_long(&ptr, end);
        int flags    = extract_long(&ptr, end);

        func_type_info_t fti;
        regobjs_t regargs, retregs;
        relobj_t stkargs;
        extract_appcall(&ptr, end, &fti, &regargs, &stkargs, &retregs);

        qstring errbuf;
        debug_event_t event;
        ea_t sp = dbg_mod->dbg_appcall(func_ea, tid, &fti, nargs, &regargs, &stkargs,
                                        &retregs, &errbuf, &event, flags);
        append_ea(cmd, sp);
        if ( sp == BADADDR )
        {
          if ( (flags & APPCALL_DEBEV) != 0 )
            append_debug_event(cmd, &event);
          append_str(cmd, errbuf);
        }
        else
        {
          append_regobjs(cmd, retregs, true);
        }
      }
      break;

    case RPC_CLEANUP_APPCALL:
      {
        thid_t tid = extract_long(&ptr, end);
        int code = dbg_mod->dbg_cleanup_appcall(tid);
        append_long(cmd, code);
      }
      break;

    default:
      return prepare_rpc_packet(RPC_UNK);
nomem:
      return prepare_rpc_packet(RPC_MEM);
  }
  return cmd;
}

//--------------------------------------------------------------------------
// poll for events from the debugger module
int rpc_server_t::poll_events(bool idling)
{
  int code = 0;
  if ( !has_pending_event )
  {
    has_pending_event = dbg_mod->dbg_get_debug_event(&pending_event, idling);
    if ( has_pending_event )
    {
      poll_debug_events = false;
      verbev(("got event, sending it, poll will be 0 now\n"));
      qstring cmd = prepare_rpc_packet(RPC_EVENT);
      append_debug_event(cmd, &pending_event);
      code = send_request(cmd);
    }
  }
  return code;
}

//--------------------------------------------------------------------------
// this function runs on the server side
// an rpc_client sends an RPC_SYNC request and the server must give the stub to the client
bool rpc_server_t::rpc_sync_stub(const char *server_stub_name, const char *ida_stub_name)
{
  bool ok = false;
  int32 crc32 = -1;
  linput_t *li = open_linput(server_stub_name, false);
  if ( li != NULL )
  {
    crc32 = calc_file_crc32(li);
    close_linput(li);
  }

  qstring stub = prepare_rpc_packet(RPC_SYNC_STUB);
  append_str(stub, ida_stub_name);
  append_long(stub, crc32);
  rpc_packet_t *rp = process_request(stub);

  if (rp == NULL)
    return ok;

  const uchar *answer = (uchar *)(rp+1);
  const uchar *end = answer + rp->length;
  size_t size = extract_long(&answer, end);
  if (size == 1)
  {
    ok = true;
  }
  else if (size != 0)
  {
    FILE *fp = fopenWB(server_stub_name);
    if ( fp != NULL )
    {
      ok = qfwrite(fp, answer, size) == size;
      dmsg("Updated kernel debugger stub: %s\n", ok ? "success" : "failed");
      qfclose(fp);
    }
    else
    {
      dwarning("Could not update the kernel debugger stub.\n%s", qerrstr());
    }
  }
  qfree(rp);

  return ok;
}
