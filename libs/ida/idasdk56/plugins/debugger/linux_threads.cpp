/*
        Thread support for IDA debugger under Linux.
*/

static bool tdb_inited = false;

//--------------------------------------------------------------------------
#define COMPLAIN_IF_FAILED(func, err)          \
  do if ( err != TD_OK )                       \
    msg("%s: %s\n", func, tdb_strerr(err)); \
  while ( 0 )

#define DIE_IF_FAILED(func, err)               \
  do if ( err != TD_OK )                       \
  {                                            \
    error("%s: %s\n", func, tdb_strerr(err));  \
    INTERR();                                  \
  }                                            \
  while ( 0 )

static const char *tdb_strerr(td_err_e err)
{
  static char buf[64];
  switch ( err )
  {
    case TD_OK:          return "ok";
    case TD_ERR:         return "generic error";
    case TD_NOTHR:       return "no thread to satisfy query";
    case TD_NOSV:        return "no sync handle to satisfy query";
    case TD_NOLWP:       return "no LWP to satisfy query";
    case TD_BADPH:       return "invalid process handle";
    case TD_BADTH:       return "invalid thread handle";
    case TD_BADSH:       return "invalid synchronization handle";
    case TD_BADTA:       return "invalid thread agent";
    case TD_BADKEY:      return "invalid key";
    case TD_NOMSG:       return "no event message for getmsg";
    case TD_NOFPREGS:    return "FPU register set not available";
    case TD_NOLIBTHREAD: return "application not linked with libthread";
    case TD_NOEVENT:     return "requested event is not supported";
    case TD_NOCAPAB:     return "capability not available";
    case TD_DBERR:       return "debugger service failed";
    case TD_NOAPLIC:     return "operation not applicable to";
    case TD_NOTSD:       return "no thread-specific data for this thread";
    case TD_MALLOC:      return "malloc failed";
    case TD_PARTIALREG:  return "only part of register set was written/read";
    case TD_NOXREGS:     return "X register set not available for this thread";
#ifdef TD_TLSDEFER
    case TD_TLSDEFER:    return "thread has not yet allocated TLS for given module";
#endif
#ifdef TD_VERSION
    case TD_VERSION:     return "versions of libpthread and libthread_db do not match";
#endif
#ifdef TD_NOTLS
    case TD_NOTLS:       return "there is no TLS segment in the given module";
#endif
    default:
      qsnprintf(buf, sizeof (buf), "tdb error %d", err);
      return buf;
  }
}

//--------------------------------------------------------------------------
// Debug print functions
#ifdef LDEB
static const char *tdb_event_name(int ev)
{
  static const char *const names[] =
  {
    "READY",       //  1
    "SLEEP",       //  2
    "SWITCHTO",    //  3
    "SWITCHFROM",  //  4
    "LOCK_TRY",    //  5
    "CATCHSIG",    //  6
    "IDLE",        //  7
    "CREATE",      //  8
    "DEATH",       //  9
    "PREEMPT",     // 10
    "PRI_INHERIT", // 11
    "REAP",        // 12
    "CONCURRENCY", // 13
    "TIMEOUT",     // 14
  };
  if ( ev > 0 && ev <= qnumber(names) )
    return names[ev-1];

  static char buf[16];
  qsnprintf(buf, sizeof(buf), "%u", ev);
  return buf;
}

//--------------------------------------------------------------------------
static char *get_thr_events_str(const td_thr_events_t &set)
{
  static char buf[MAXSTR];
  char *ptr = buf;
  char *end = buf + sizeof(buf);
  for ( int i=TD_MIN_EVENT_NUM; i <= TD_MAX_EVENT_NUM; i++ )
  {
    if ( td_eventismember(&set, i) )
    {
      if ( ptr != buf )
        APPCHAR(ptr, end, ' ');
      APPEND(ptr, end, tdb_event_name(i));
    }
  }
  return buf;
}

//--------------------------------------------------------------------------
static const char *get_sigset_str(const sigset_t &set)
{
  static char buf[MAXSTR];
  char *ptr = buf;
  char *end = buf + sizeof(buf);
  for ( int i=0; i <= 32; i++ )
  {
    if ( sigismember(&set, i) )
    {
      if ( ptr != buf )
        APPCHAR(ptr, end, ' ');
      ptr += qsnprintf(ptr, end-ptr, "%d", i);
    }
  }
  return buf;
}

//--------------------------------------------------------------------------
static const char *get_thread_state_name(td_thr_state_e state)
{
  static const char *const names[] =
  {
    "ANY_STATE",      //  0
    "UNKNOWN",        //  1
    "STOPPED",        //  2
    "RUN",            //  3
    "ACTIVE",         //  4
    "ZOMBIE",         //  5
    "SLEEP",          //  6
    "STOPPED_ASLEEP"  //  7
  };
  if ( state >= 0 && state < qnumber(names) )
    return names[state];

  static char buf[16];
  qsnprintf(buf, sizeof(buf), "%u", state);
  return buf;
}

//--------------------------------------------------------------------------
static const char *get_thread_type_name(td_thr_type_e type)
{
  static const char *const names[] =
  {
    "ANY_STATE",      //  0
    "USER",           //  1
    "SYSTEM",         //  2
  };
  if ( type >= 0 && type < qnumber(names) )
    return names[type];

  static char buf[16];
  qsnprintf(buf, sizeof(buf), "%u", type);
  return buf;
}

//--------------------------------------------------------------------------
static void display_thrinfo(const td_thrinfo_t &thi)
{
  msg("  tid         : %lx\n", thi.ti_tid);
  msg("  tls         : %lx\n", (size_t)thi.ti_tls);
  msg("  entry       : %lx\n", (size_t)thi.ti_startfunc);
  msg("  stackbase   : %lx\n", (size_t)thi.ti_stkbase);
  msg("  stacksize   : %lx\n", thi.ti_stksize);
  msg("  state       : %s\n", get_thread_state_name(thi.ti_state));
  msg("  suspended   : %d\n", thi.ti_db_suspended);
  msg("  type        : %s\n", get_thread_type_name(thi.ti_type));
  msg("  priority    : %d\n", thi.ti_pri);
  msg("  kernel pid  : %d\n", thi.ti_lid); // lwpid_t
  msg("  signal mask : %lx\n", *(size_t*)&thi.ti_sigmask);
  msg("  traceme     : %d\n", thi.ti_traceme);
  msg("  pending sg  : %s\n", get_sigset_str(thi.ti_pending));
  msg("  enabled ev  : %s\n", get_thr_events_str(thi.ti_events));
}

//--------------------------------------------------------------------------
void linux_debmod_t::display_thrinfo(thid_t tid)
{
  msg("tid=%d\n", tid);
  td_thrhandle_t th;
  td_err_e err = td_ta_map_lwp2thr(ta, tid, &th);
  COMPLAIN_IF_FAILED("td_ta_map_lwp2thr2", err);

  if ( err == 0 )
  {
    td_thrinfo_t thi;
    memset(&thi, 0 ,sizeof(thi));
    err = td_thr_get_info(&th, &thi);
    COMPLAIN_IF_FAILED("td_thr_get_info2", err);

    if ( err == 0 )
      ::display_thrinfo(thi);
  }
}

//--------------------------------------------------------------------------
static int display_thread_cb(const td_thrhandle_t *th_p, void *data)
{
  td_thrinfo_t ti;
  td_err_e err = td_thr_get_info(th_p, &ti);
  DIE_IF_FAILED("td_thr_get_info", err);

  if ( ti.ti_state == TD_THR_UNKNOWN || ti.ti_state == TD_THR_ZOMBIE )
    return 0;

  display_thrinfo(ti);
  return 0;
}

void linux_debmod_t::display_all_threads()
{
  if ( ta != NULL )
  {
    td_err_e err = td_ta_thr_iter(ta, display_thread_cb, NULL,
                                  TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
                                  TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
    COMPLAIN_IF_FAILED("td_ta_thr_iter", err);
  }
}

#endif // end of debug print functions


//--------------------------------------------------------------------------
// Helper functions for thread_db
// (it requires ps_... functions to be defined in the debugger)
//--------------------------------------------------------------------------
static linux_debmod_t *find_debugger(ps_prochandle *ph)
{
  struct find_debugger_t : public debmod_visitor_t
  {
    int pid;
    linux_debmod_t *found;
    find_debugger_t(int p) : pid(p), found(NULL) {}
    int visit(debmod_t *debmod)
    {
      linux_debmod_t *ld = (linux_debmod_t *)debmod;
      if ( ld->process_handle == pid )
      {
        found = ld;
        return 1; // stop
      }
      return 0; // continue
    }
  };
  find_debugger_t fd(ph->pid);
  for_all_debuggers(fd);
//  msg("prochandle: %x, looking for the debugger, found: %x\n", ph, fd.found);
  return fd.found;
}

//--------------------------------------------------------------------------
extern "C"
{
ps_err_e ps_pglobal_lookup(
        ps_prochandle *ph,
        const char *obj,
	const char *name,
        psaddr_t *sym_addr)
{
  linux_debmod_t *ld = find_debugger(ph);
  if ( ld == NULL )
    return PS_BADPID;

  ld->enum_names(obj); // update the name list

  ea_t ea = ld->find_pending_name(name);
  if ( ea == BADADDR )
  {
#ifdef LDEB
    msg("FAILED TO FIND name '%s'\n", name);
#endif
    return PS_NOSYM;
  }
  *sym_addr = (void*)ea;
#ifdef LDEB
  msg("ps_pglobal_lookup('%s') => %a\n", name, ea);
#endif
  return PS_OK;
}

//--------------------------------------------------------------------------
ps_err_e ps_pdread(
        ps_prochandle *ph,
        psaddr_t addr,
	void *buf,
        size_t size)
{
#ifdef LDEB
  msg("ps_pdread(%a, %d)\n", size_t(addr), size);
#endif
  linux_debmod_t *ld = find_debugger(ph);
  if ( ld == NULL )
  {
#ifdef LDEB
    msg("\t=> bad pid\n");
#endif
    return PS_BADPID;
  }
  if ( ld->_read_memory(size_t(addr), buf, size, false) <= 0 )
  {
#ifdef LDEB
    msg("\t=> read error\n");
#endif
    return PS_ERR;
  }
#ifdef LDEB
  msg("\t=> read OK\n");
#endif
  return PS_OK;
}

//--------------------------------------------------------------------------
ps_err_e ps_pdwrite(
        ps_prochandle *ph,
        psaddr_t addr,
	void *buf,
        size_t size)
{
  linux_debmod_t *ld = find_debugger(ph);
  if ( ld == NULL )
    return PS_BADPID;
  if ( ld->_write_memory(size_t(addr), buf, size, false) <= 0 )
    return PS_ERR;
  return PS_OK;
}

//--------------------------------------------------------------------------
ps_err_e ps_lgetregs(ps_prochandle *ph, lwpid_t lwpid, prgregset_t gregset)
{
  if ( qptrace(PTRACE_GETREGS, lwpid, 0, gregset) != 0 )
    return PS_ERR;
  return PS_OK;
}

//--------------------------------------------------------------------------
ps_err_e ps_lsetregs(ps_prochandle *ph, lwpid_t lwpid, const prgregset_t gregset)
{
  if ( qptrace(PTRACE_SETREGS, lwpid, 0, (void*)gregset) != 0 )
    return PS_ERR;
  return PS_OK;
}

//--------------------------------------------------------------------------
ps_err_e ps_lgetfpregs(ps_prochandle *ph, lwpid_t lwpid, prfpregset_t *fpregset)
{
  if ( qptrace(PTRACE_GETFPREGS, lwpid, 0, fpregset) != 0 )
    return PS_ERR;
  return PS_OK;
}

//--------------------------------------------------------------------------
ps_err_e ps_lsetfpregs(ps_prochandle *ph, lwpid_t lwpid, const prfpregset_t *fpregset)
{
  if ( qptrace(PTRACE_SETFPREGS, lwpid, 0, (void*)fpregset) != 0 )
    return PS_ERR;
  return PS_OK;
}

//--------------------------------------------------------------------------
pid_t ps_getpid(ps_prochandle *ph)
{
  return ph->pid;
}

//--------------------------------------------------------------------------
ps_err_e ps_get_thread_area(const struct ps_prochandle *ph, lwpid_t lwpid, int idx, void **base)
{
#ifdef __X64__
  // from <sys/reg.h>
  #define LINUX_FS 25
  #define LINUX_GS 26

  /* The following definitions come from prctl.h, but may be absent
     for certain configurations.  */
  #ifndef ARCH_GET_FS
  #define ARCH_SET_GS 0x1001
  #define ARCH_SET_FS 0x1002
  #define ARCH_GET_FS 0x1003
  #define ARCH_GET_GS 0x1004
  #endif

  switch ( idx )
  {
    case LINUX_FS:
      if ( ptrace (PTRACE_ARCH_PRCTL, lwpid, base, ARCH_GET_FS) != 0 )
        return PS_ERR;
      break;
    case LINUX_GS:
      if ( ptrace (PTRACE_ARCH_PRCTL, lwpid, base, ARCH_GET_GS) != 0 )
        return PS_ERR;
      break;
    default:
      return PS_BADADDR;
  }

#else
  #ifndef PTRACE_GET_THREAD_AREA
  #define PTRACE_GET_THREAD_AREA __ptrace_request(25)
  #endif
    unsigned int desc[4];
    if ( ptrace(PTRACE_GET_THREAD_AREA, lwpid, (void *)idx, (size_t)&desc) < 0 )
      return PS_ERR;

    *(int *)base = desc[1];
#endif

  return PS_OK;
}

} // extern "C"  (end of thread_db helper functions)

//--------------------------------------------------------------------------
// High level interface for the rest of the debugger module
//--------------------------------------------------------------------------
void tdb_init(void)
{
  if ( !tdb_inited )
  {
    if ( td_init() == TD_OK )
      tdb_inited = true;
  }
}

//--------------------------------------------------------------------------
void tdb_term(void)
{
  // no way to uninitialize thread_db
}

//--------------------------------------------------------------------------
struct thrinfo_t : public td_thrinfo_t
{
  const td_thrhandle_t *th_p;
};
typedef qvector<thrinfo_t> thrinfovec_t;

static int update_threads_cb(const td_thrhandle_t *th_p, void *data)
{
  thrinfovec_t &newlist = *(thrinfovec_t *)data;

  thrinfo_t ti;
  td_err_e err = td_thr_get_info(th_p, &ti);
  DIE_IF_FAILED("td_thr_get_info", err);

  if ( ti.ti_state == TD_THR_UNKNOWN || ti.ti_state == TD_THR_ZOMBIE )
    return 0;

  if ( ti.ti_tid != 0 )
  {
    td_thr_events_t events;
    td_event_emptyset(&events);
    td_event_addset(&events, TD_CREATE);
    td_event_addset(&events, TD_DEATH);
  //  td_event_addset(&events, TD_CATCHSIG);
    err = td_thr_set_event(th_p, &events);
    DIE_IF_FAILED("td_thr_set_event", err);

    err = td_thr_event_enable(th_p, 1);
    DIE_IF_FAILED("td_thr_event_enable", err);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGSTOP);
    td_thr_setsigpending(th_p, 1, &set);
  }

  ti.th_p = th_p;
#ifdef LDEB
  msg("update_threads_cb:\n");
  td_thr_get_info(th_p, &ti);
  display_thrinfo(ti);
#endif

  newlist.push_back(ti);
  return 0;
}

//--------------------------------------------------------------------------
void linux_debmod_t::tdb_update_threads(void)
{
  if ( ta != NULL )
  {
    thrinfovec_t newlist;
    td_err_e err = td_ta_thr_iter(ta, update_threads_cb, &newlist,
                                  TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
                                  TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
    COMPLAIN_IF_FAILED("td_ta_thr_iter", err);
    if ( err != TD_OK )
      return;

    // generate THREAD_EXIT events
    for ( threads_t::iterator p=threads.begin(); p != threads.end(); ++p )
    {
      int i;
      int tid = p->first;
      for ( i=0; i < newlist.size(); i++ )
        if ( newlist[i].ti_lid == tid )
          break;
      if ( i == newlist.size() ) // not found
      {
        debug_event_t ev;
        ev.eid     = THREAD_EXIT;
        ev.pid     = process_handle;
        ev.tid     = tid;
        ev.ea      = BADADDR;
        ev.handled = true;
        ev.exit_code = 0; // ???
        enqueue_event(ev, IN_BACK);
      }
    }

    // generate THREAD_START events
    for ( int i=0; i < newlist.size(); i++ )
    {
      int tid = newlist[i].ti_lid;
//      display_thrinfo(tid);
      threads_t::iterator p = threads.find(tid);
      if ( p == threads.end() ) // not found
      {
        debug_event_t ev;
        ev.eid     = THREAD_START;
        ev.pid     = process_handle;
        ev.tid     = tid;
        ev.ea      = birth_bpt.getaddr(); // just to show something
        ev.handled = true;
        add_thread(tid);
        enqueue_event(ev, IN_FRONT);
        // attach to the thread and make is ready for debugging
        qptrace(PTRACE_ATTACH, tid, 0, 0);
        int status;
        int tid2 = waitpid(tid, &status, __WCLONE); // (must succeed) consume SIGSTOP
        QASSERT(tid2 != -1 && WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);
        get_thread(tid)->waiting_sigstop = false;

      }
      get_thread(tid)->thr = newlist[i].th_p;
    }
  }
}

//--------------------------------------------------------------------------
void linux_debmod_t::tdb_enable_event(td_event_e event, internal_bpt *bp)
{
  td_notify_t notify;
  td_err_e err = td_ta_event_addr(ta, event, &notify);
  DIE_IF_FAILED("td_ta_event_addr", err);
  bool ok = add_internal_bp(*bp, size_t(notify.u.bptaddr));
  debdeb("%a: added BP for thread event %s\n", bp->getaddr(), event == TD_CREATE ? "TD_CREATE" : "TD_DEATH");
  QASSERT(ok);
}

//--------------------------------------------------------------------------
// returns true: multithreaded application has been detected
bool linux_debmod_t::tdb_new(int pid)
{
  if ( ta == NULL )
  {
#ifdef LDEB
    msg("checking pid %d with thread_db\n", pid);
#endif
    prochandle.pid = pid;
    td_err_e err = td_ta_new(&prochandle, &ta);
    // the call might fail the first time if libc is not loaded yet
    // so don't show misleading message to the user
    // COMPLAIN_IF_FAILED("td_ta_new", err);
    if ( err != TD_OK )
    {
      ta = NULL;
      return false;
    }

    td_thrhandle_t th;
    err = td_ta_map_lwp2thr(ta, pid, &th);
    COMPLAIN_IF_FAILED("td_ta_map_lwp2thr", err);
    if ( err != TD_OK )
      return false;

    err = td_thr_event_enable(&th, TD_CREATE);
    DIE_IF_FAILED("td_thr_event_enable(TD_CREATE)", err);
    err = td_thr_event_enable(&th, TD_DEATH);
    DIE_IF_FAILED("td_thr_event_enable(TD_DEATH)", err);

    // set breakpoints for thread birth/death
    td_thr_events_t events;
    td_event_emptyset(&events);
    td_event_addset(&events, TD_CREATE);
    td_event_addset(&events, TD_DEATH);
    err = td_ta_set_event(ta, &events);
    DIE_IF_FAILED("td_ta_set_event", err);

    tdb_enable_event(TD_CREATE, &birth_bpt);
    tdb_enable_event(TD_DEATH, &death_bpt);
#ifdef LDEB
    msg("thread support has been detected, birth_bpt=%a death_bpt=%a\n", birth_bpt.getaddr(), death_bpt.getaddr());
#endif

/*    // hack: enable thread_td debug. later: it turned out to be useless debug print.
    td_log();
    uchar *ptr = (uchar *)td_log;
    QASSERT(ptr[0] == 0xFF && ptr[1] == 0x25);
    ptr = **(uchar***)(ptr+2);
    ptr += 0x7158 - 0x1120;
    *ptr = 1; // enable
*/
    tdb_update_threads();
  }
  return true;
}
