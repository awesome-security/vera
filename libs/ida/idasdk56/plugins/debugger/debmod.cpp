#ifdef __NT__
#include <windows.h>
#endif

#include "debmod.h"
#include <err.h>
#include <diskio.hpp>
#include <typeinf.hpp>
#include <kernwin.hpp>

//--------------------------------------------------------------------------
debmod_t::debmod_t(void)
  :
#ifdef __X64__
    addrsize(8),
#else
    addrsize(4),
#endif
    dmsg_ud(NULL),
    debug_debugger(false),
    sp_idx(-1),
    pc_idx(-1),
    nregs(0)
{
}

//--------------------------------------------------------------------------
bool debmod_t::same_as_oldmemcfg(const meminfo_vec_t &areas)
{
  return old_areas == areas;
}

//--------------------------------------------------------------------------
void debmod_t::save_oldmemcfg(const meminfo_vec_t &areas)
{
  old_areas = areas;
}

//--------------------------------------------------------------------------
bool debmod_t::check_input_file_crc32(uint32 orig_crc)
{
  // take this opportunity to check that the derived class initialized
  // register related fields correctly
  QASSERT(sp_idx != -1 && pc_idx != -1 && nregs > 0);

  if ( orig_crc == 0 )
    return true; // the database has no crc

  linput_t *li = open_linput(input_file_path.c_str(), false);
  if ( li == NULL )
    return false;

  uint32 crc = calc_file_crc32(li);
  close_linput(li);
  return crc == orig_crc;
}

//--------------------------------------------------------------------------
const exception_info_t *debmod_t::find_exception(int code)
{
  const exception_info_t *ei = &exceptions[0];
  for ( int i=(int)exceptions.size()-1; i >=0 ; ++ei, i-- )
  {
    if ( ei->code == (uint)code )
      return ei;
  }
  return NULL;
}

//--------------------------------------------------------------------------
bool debmod_t::get_exception_name(int code, char *buf, size_t bufsize)
{
  const exception_info_t *ei = find_exception(code);
  if ( ei != NULL )
  {
    qstrncpy(buf, ei->name.c_str(), bufsize);
    return true;
  }
  qsnprintf(buf, bufsize, "%08X", code);
  return false;
}

//----------------------------------------------------------------------
int idaapi debmod_t::get_system_specific_errno(void) const
{
  // this code must be acceptable by winerr()
#ifdef __NT__
  return GetLastError();
#else
  return errno;
#endif
}

//----------------------------------------------------------------------
// Display a system error message. This function always returns false
bool debmod_t::deberr(const char *format, ...)
{
  if ( !debug_debugger )
    return false;

  int code = get_system_specific_errno();
  va_list va;
  va_start(va, format);
  dvmsg(0, dmsg_ud, format, va);
  va_end(va);
  dmsg(": %s\n", winerr(code));
  return false;
}

//--------------------------------------------------------------------------
// used to debug the debugger
void debmod_t::debdeb(const char *format, ...)
{
  if (!debug_debugger)
    return;

  va_list va;
  va_start(va, format);
  dvmsg(0, dmsg_ud, format, va);
  va_end(va);
}

//--------------------------------------------------------------------------
void debmod_t::cleanup(void)
{
  input_file_path.qclear();
  old_areas.qclear();
  exceptions.qclear();
  clear_debug_names();
}

//--------------------------------------------------------------------------
void idaapi debmod_t::dbg_set_exception_info(const exception_info_t *table, int qty)
{
  exceptions.qclear();
  for ( int i=0; i < qty; i++ )
    exceptions.push_back(*table++);
}

//--------------------------------------------------------------------------
char *debmod_t::debug_event_str(const debug_event_t *ev)
{
  static char buf[MAXSTR];
  return debug_event_str(ev, buf, sizeof(buf));
}

//--------------------------------------------------------------------------
static const char *get_event_name(event_id_t id)
{
  switch ( id )
  {
    case NO_EVENT:       return "NO_EVENT";
    case THREAD_START:   return "THREAD_START";
    case STEP:           return "STEP";
    case SYSCALL:        return "SYSCALL";
    case WINMESSAGE:     return "WINMESSAGE";
    case PROCESS_DETACH: return "PROCESS_DETACH";
    case PROCESS_START:  return "PROCESS_START";
    case PROCESS_ATTACH: return "PROCESS_ATTACH";
    case PROCESS_SUSPEND: return "PROCESS_SUSPEND";
    case LIBRARY_LOAD:   return "LIBRARY_LOAD";
    case PROCESS_EXIT:   return "PROCESS_EXIT";
    case THREAD_EXIT:    return "THREAD_EXIT";
    case BREAKPOINT:     return "BREAKPOINT";
    case EXCEPTION:      return "EXCEPTION";
    case LIBRARY_UNLOAD: return "LIBRARY_UNLOAD";
    case INFORMATION:    return "INFORMATION";
    default:             return "???";
  }
}

//--------------------------------------------------------------------------
char *debmod_t::debug_event_str(const debug_event_t *ev, char *buf, size_t bufsize)
{
  char *ptr = buf;
  char *end = buf + bufsize;
  ptr += qsnprintf(ptr, end-ptr, "%s ea=%a",
    get_event_name(ev->eid),
    ev->ea);
  switch ( ev->eid )
  {
  case PROCESS_START:  // New process started
  case PROCESS_ATTACH: // Attached to running process
  case LIBRARY_LOAD:   // New library loaded
    ptr += qsnprintf(ptr, end-ptr, " base=%a size=%a rebase=%a name=%s",
      ev->modinfo.base,
      ev->modinfo.size,
      ev->modinfo.rebase_to,
      ev->modinfo.name);
    break;
  case PROCESS_EXIT:   // Process stopped
  case THREAD_EXIT:    // Thread stopped
    ptr += qsnprintf(ptr, end-ptr, " exit_code=%d", ev->exit_code);
    break;
  case BREAKPOINT:     // Breakpoint reached
    ptr += qsnprintf(ptr, end-ptr, " hea=%a kea=%a", ev->bpt.hea, ev->bpt.kea);
    break;
  case EXCEPTION:      // Exception
    ptr += qsnprintf(ptr, end-ptr, " code=%x can_cont=%d ea=%a info=%s",
      ev->exc.code,
      ev->exc.can_cont,
      ev->exc.ea,
      ev->exc.info);
    break;
  case LIBRARY_UNLOAD: // Library unloaded
  case INFORMATION:    // User-defined information
    APPCHAR(ptr, end, ' ');
    APPEND(ptr, end, ev->info);
    break;
  default:
    break;
  }
  qsnprintf(ptr, end-ptr, " pid=%d tid=%d handled=%d",
    ev->pid,
    ev->tid,
    ev->handled);
  return buf;
}

//--------------------------------------------------------------------------
name_info_t *debmod_t::get_debug_names()
{
  return &dn_names;
}

//--------------------------------------------------------------------------
void debmod_t::clear_debug_names()
{
  if ( dn_names.addrs.empty() )
    return;

  typedef qvector<char *> charptr_vec_t;
  charptr_vec_t::iterator it_end = dn_names.names.end();
  for ( charptr_vec_t::iterator it=dn_names.names.begin();it!=it_end;++it )
    qfree(*it);

  dn_names.names.clear();
  dn_names.addrs.clear();
}

//--------------------------------------------------------------------------
void debmod_t::save_debug_name(ea_t ea, const char *name)
{
  dn_names.addrs.push_back(ea);
  dn_names.names.push_back(qstrdup(name));
}

//--------------------------------------------------------------------------
bool debmod_t::continue_after_last_event(bool handled)
{
  last_event.handled = handled;
  return dbg_continue_after_event(&last_event) == 1;
}

//--------------------------------------------------------------------------
bool debmod_t::should_stop_appcall(thid_t, const debug_event_t *event, ea_t ea)
{
  switch ( event->eid )
  {
    case EXCEPTION:
      // exception on the stack (non-executable stack?)
      if ( event->exc.ea == ea )
        return true;
      // no break
    case BREAKPOINT:
      // reached the control breakpoint?
      if ( event->ea == ea )
        return true;
      break;
  }
  return false;
}

//--------------------------------------------------------------------------
ea_t debmod_t::dbg_appcall(
  ea_t func_ea,
  thid_t tid,
  const struct func_type_info_t *fti,
  int /*nargs*/,
  const struct regobjs_t *regargs,
  struct relobj_t *stkargs,
  struct regobjs_t *retregs,
  qstring *errbuf,
  debug_event_t *event,
  int options)
{
  enum
  {
    E_OK,          // Success
    E_READREGS,    // Failed to read registers
    E_REG_USED,    // The calling convention refers to reserved registers
    E_ARG_ALLOC,   // Failed to allocate memory for stack arguments
    E_WRITE_ARGS,  // Failed to setup stack arguments
    E_WRITE_REGS,  // Failed to setup register arguments
    E_INTERRUPTED, // User interrupted the AppCall
    E_DEBUG_EVENT, // Could not get debug events
    E_QUIT,        // Program exited
    E_RESUME,      // Failed to resume the application
    E_EXCEPTION,   // An exception has occured
    E_APPCALL_FROM_EXC, // Cannot issue an AppCall if last event was an exception
  };

  static const char *const errstrs[] =
  {
    "success",
    "failed to read registers",
    "the calling convention refers to reserved registers",
    "failed to allocate memory for stack arguments",
    "failed to setup stack arguments",
    "failed to setup register arguments",
    "user interrupt",
    "could not get debug events",
    "program exited",
    "failed to resume the application",
    "an exception has occured",
    "last event was an exception. cannot perform an appcall."
  };

  // Save registers
  regval_t rv;

  bool brk = false;
  int err = E_OK;

  call_context_t &ctx = appcalls[tid].push_back();

  regval_map_t call_regs;
  ea_t args_sp = BADADDR;
  do
  {
    // In Win32, when WaitForDebugEvent() returns an exception
    // it seems that the OS remembers the exception context so that
    // the next call to ContinueDebugEvent() will work with the last exception context.
    // Now if we carry an AppCall when an exception was just reported:
    // - Appcall will change context
    // - Appcall's control bpt will generate an exception thus overriding the last exception context saved by the OS
    // - After Appcall, IDA kernel cannot really continue from the first exception because it was overwritten
    // Currently we will disallow Appcalls if last event is an exception
    if ( last_event.eid == EXCEPTION )
    {
      err = E_APPCALL_FROM_EXC;
      break;
    }
    // Save registers
    ctx.saved_regs.resize(nregs);
    if ( dbg_thread_read_registers(tid, &ctx.saved_regs[0], nregs) != 1 )
    {
      err = E_READREGS;
      break;
    }

    // Get SP value
    ea_t org_sp = ea_t(ctx.saved_regs[sp_idx].ival);

    // Stack contents
    bytevec_t stk;

    // Prepare control address
    // We will generate a BP code ptrsz aligned and push unto the stack
    // as the first argument. This is where we will set the control bpt.
    // Since the bpt is on the stack, two possible scenarios:
    // - BPT exception
    // - Access violation: trying to execute from NX page
    // In both cases we will catch an exception and learn what address was
    // involved.

    // - Save the ctrl address
    ea_t ctrl_ea = org_sp - addrsize;

    // - Compute the pointer where arguments will be allocated on the stack
    size_t stkargs_size = align_up(stkargs->size(), addrsize);
    args_sp = ctrl_ea - stkargs_size;

    // align the stack pointer to 16 byte boundary (gcc compiled programs require it)
    args_sp &= ~15;
    ctx.ctrl_ea = args_sp + stkargs_size;

    // Relocate the stack arguments
    if ( !stkargs->relocate(args_sp, false) )
    {
      err = E_ARG_ALLOC;
      break;
    }

    // Prepare the stack.
    // The memory layout @SP before transfering to the function:
    // R = ret addr
    // A = args

    // - Append the return address (its value is the value of the ctrl code address)
    stk.append(&ctx.ctrl_ea, addrsize);

    // - Append the stack args
    stk.append(stkargs->begin(), stkargs->size());
    stk.resize(addrsize+stkargs_size); // align up
    ctx.sp = args_sp - addrsize;

    int delta = finalize_appcall_stack(ctx, call_regs, stk);
    ctx.sp += delta;

    // Write the stack
    int nwrite = stk.size() - delta;
    if ( nwrite > 0 )
    {
      if ( dbg_write_memory(ctx.sp, stk.begin()+delta, nwrite) != nwrite )
      {
        err = E_WRITE_ARGS;
        break;
      }
      //show_hex(stk.begin()+delta, nwrite, "Written stack bytes to %a:\n", ctx.sp);
    }

    // ask the debugger to set a breakpoint
    dbg_add_bpt(BPT_SOFT, ctx.ctrl_ea, -1);

    // Copy arg registers to call_regs
    for ( size_t i=0; i < regargs->size(); i++ )
    {
      const regobj_t &ri = regargs->at(i);
      int reg_idx = ri.regidx;
      if ( reg_idx == sp_idx || reg_idx == pc_idx )
      {
        brk = true;
        err = E_REG_USED;
        break;
      }

      // Copy the register value
      memcpy(&rv, &ri.value[0], ri.size());
      if ( ri.relocate )
        rv.ival += args_sp;
      call_regs[reg_idx] = rv;
    }
    if ( brk )
      break;

    // Set the stack pointer
    rv.ival = ctx.sp;
    call_regs[sp_idx] = rv;

    // Set the instruction pointer
    rv.ival = func_ea;
    call_regs[pc_idx] = rv;

    // Change all the registers in question
    for ( regval_map_t::iterator it = call_regs.begin();
      it != call_regs.end();
      ++it )
    {
      if ( dbg_thread_write_register(tid, it->first, &it->second) != 1 )
      {
        err = E_WRITE_REGS;
        brk = true;
        break;
      }
      // Mark that we changed the regs already
      ctx.regs_spoiled = true;
    }
    if ( brk )
      break;

    // For manual appcall, we have done everything, just return now
    if ( (options & APPCALL_MANUAL) != 0 )
    {
      err = E_OK;
      break;
    }

    // Resume the application
    // Since no* exception last occured**, we can safely say that the
    // debugger actually handled the exception.
    // * : We disallow appcalls if an exception last occured
    // **: Actually if an AppCall was issued then last event is an exception
    //     but we will mask it by calling continue_after_event(handled_by_debugger = true)
    if ( !continue_after_last_event(true) )
    {
      err = E_RESUME;
      break;
    }

    // We use this list to accumulate the events
    // We will give back the events at the end of the loop
    eventlist_t appcall_events;
    debug_event_t tmp;
    if ( event == NULL )
      event = &tmp;

    while ( true )
    {
      // Wait for debug events
      int r = dbg_get_debug_event(event, false);
      if ( r == 0 )
        continue;
      // Error getting debug event?
      // (Network error, etc...)
      else if ( r == -1 )
      {
        err = E_DEBUG_EVENT;
        break;
      }

      debdeb("Got event: %s\n", debug_event_str(event));

      // We may get three possible events related to our control breakpoint:
      // - Access violation type: because we try to execute non-executable code
      // - Or a BPT exception if the stack page happens to be executable
      // - Process exit
      if ( event->eid == PROCESS_EXIT || should_stop_appcall(tid, event, ctx.ctrl_ea) )
      {
        // queue the exit process event so that the debugger module reports
        // it back to the kernel
        if ( event->eid == PROCESS_EXIT )
          appcall_events.enqueue(*event, IN_BACK);

        last_event.eid = NO_EVENT;
        err = E_OK;
        break;
      }
      // Any other exception?
      else if ( event->eid == EXCEPTION )
      {
        if ( (options & APPCALL_DEBEV) == 0 )
          *errbuf = event->exc.info; // Copy exception text to the user
        err = E_EXCEPTION;
        // When an exception happens during the appcall, we want to mask
        // the exception, because:
        // 1. we reset the EIP to its original location
        // 2. there is no exception handler for the appcall so we cannot really pass as unhandled
        // FIXME
        last_event.eid = NO_EVENT;
        last_event.handled = true;
        brk = true;
        break;
      }

      // Queue other events for later delivery
      // FIXME: we should not continue to queue events like this
      //        but immediately report an error at the first event
      appcall_events.enqueue(*event, IN_BACK);

      // Continue event
      dbg_continue_after_event(event);
    }

    // Dequeue appcall events and queue them back in the debugger module's
    // event list
    while ( appcall_events.retrieve(&tmp) )
    {
      // Queue the event for later delivery
      events.enqueue(tmp, IN_BACK);
    }

    if ( brk || err != E_OK )
      break;

    // write the argument vector back because it could be spoiled by the application
    size_t nbytes = fti->stkargs;
    if ( nbytes > 0
      && dbg_write_memory(args_sp, stkargs->begin(), nbytes) != ssize_t(nbytes) )
    {
      err = E_WRITE_ARGS;
      break;
    }

    // Retrieve the return value
    if ( retregs != NULL && !retregs->empty() )
    {
      regvals_t retr;
      retr.resize(nregs);
      if ( dbg_thread_read_registers(tid, &retr[0], nregs) <= 0 )
      {
        err = E_READREGS;
        break;
      }
      for ( size_t i=0; i < retregs->size(); i++ )
      {
        regobj_t &r = retregs->at(i);
        regval_t &v = retr[r.regidx];
        memcpy(r.value.begin(), &v.ival, r.value.size());
      }
    }
  } while ( false );

  if ( err != E_OK )
  {
    if ( err != E_EXCEPTION )
      *errbuf = errstrs[err];
    dbg_cleanup_appcall(tid);
    args_sp = BADADDR;
  }

  return args_sp;
}

//--------------------------------------------------------------------------
// Cleanup after appcall()
// The debugger module must keep the stack blob in the memory until this function
// is called. It will be called by the kernel for each successful call_app_func()
int idaapi debmod_t::dbg_cleanup_appcall(thid_t tid)
{
  call_contexts_t &calls = appcalls[tid];
  if ( calls.empty() )
    return 0;

  // remove the return breakpoint
  call_context_t &ctx = calls.back();
  if ( !preprocess_appcall_cleanup(tid, ctx) )
    return 0;

  dbg_del_bpt(ctx.ctrl_ea, bpt_code.begin(), bpt_code.size());
  if ( ctx.regs_spoiled )
  {
    if ( !thread_write_registers(tid, 0, ctx.saved_regs.size(), ctx.saved_regs.begin()) )
    {
      dmsg("Failed to restore registers!\n");
      return 0;
    }
  }

  calls.pop();
  return 1;
}
