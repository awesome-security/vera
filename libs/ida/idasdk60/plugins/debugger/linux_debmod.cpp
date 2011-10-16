/*
*  This is a userland linux debugger module
*
*  Functions unique for Linux
*
*  It can be compiled by gcc
*
*/

//#define LDEB            // enable debug print in this module

#define CHECKED_BUILD
#include <link.h>
#include <sys/syscall.h>
#include <pthread.h>

#include <pro.h>
#include <prodir.h>
#include <fpro.h>
#include <err.h>
#include <ida.hpp>
#include <idp.hpp>
#include <idd.hpp>
#include <name.hpp>
#include <bytes.hpp>
#include <loader.hpp>
#include <diskio.hpp>
#include "symelf.hpp"
#include "consts.h"

#include <algorithm>
#include <deque>
#include <map>
#include <set>

using std::pair;
using std::make_pair;
typedef uint32 DWORD;

#ifdef __ARM__
#define user_regs_struct user_regs
#define user_fpregs_struct user_fpregs
#endif

// Program counter register
#if defined(__ARM__)
#  define SPREG uregs[13]
#  define PCREG uregs[15]
#  define PCREG_IDX 15
#  define CLASS_OF_INTREGS ARM_RC_GENERAL
#else
#  define CLASS_OF_INTREGS (X86_RC_GENERAL|X86_RC_SEGMENTS)
#  if defined(__X64__)
#    define SPREG rsp
#    define PCREG rip
#    define PCREG_IDX   R_EIP
#    define XMM_STRUCT  i387
#    define TAGS_REG    ftw
#    define CLASSES_STORED_IN_FPREGS (X86_RC_FPU|X86_RC_MMX|X86_RC_XMM) // fpregs keeps xmm&fpu
#  else
#    define SPREG esp
#    define PCREG eip
#    define PCREG_IDX   R_EIP
#    define XMM_STRUCT  x387
#    define TAGS_REG    twd
#    define CLASSES_STORED_IN_FPREGS (X86_RC_FPU|X86_RC_MMX)            // fpregs keeps only fpu
#  endif
#endif

#include "linux_debmod.h"

// class which sends an empty message to client every 3 seconds to prevent timing out
// FIXME: this class is not used anymore (it always introduces a delay of ~3 seconds)
//        (we explicitly send an empty message to ida every 10000 names)
class long_runner_t
{
  pthread_t thread;
  bool stop; // FIXME: probably should use mutex and condition
  debmod_t *pdbg;
  static void *thread_function(void *arg)
  {
#ifdef LDEB
    msg("long_runner_t::thread_function start\n");
#endif
    long_runner_t *lr = (long_runner_t*)arg;
    while ( !lr->stop )
    {
      sleep(3);
      //msg("<keepalive>\n");
      lr->pdbg->dmsg("");
    }
#ifdef LDEB
    msg("long_runner_t::thread_function end\n");
    pthread_exit(NULL);
#endif
  }

public:
  long_runner_t(debmod_t *_pdbg): pdbg(_pdbg)
  {
    //msg("long_runner_t()\n");
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    stop = false;
    int code = pthread_create(&thread, &attr, thread_function, (void *)this);
    //msg("pthread_create: %d\n", code);
    pthread_attr_destroy(&attr);
  }
  ~long_runner_t()
  {
    //msg("~long_runner_t()\n");
    stop = true;
    void *status;
    int rc = pthread_join(thread, &status);
    //msg("pthread_join: %d\n", rc);
  }
};

static char getstate(int tid);

//--------------------------------------------------------------------------
#ifndef WCONTINUED
#define WCONTINUED 8
#endif

linux_debmod_t::linux_debmod_t(void) :
  wait_flags(__WALL | WCONTINUED),
  mapfp(NULL),
  ta(NULL)
{
}

/* This definition comes from prctl.h, but some kernels may not have it.  */
#ifndef PTRACE_ARCH_PRCTL
#define PTRACE_ARCH_PRCTL      __ptrace_request(30)
#endif

//--------------------------------------------------------------------------
static const char *get_ptrace_name(__ptrace_request request)
{
  switch ( request )
  {
  case PTRACE_TRACEME:    return "PTRACE_TRACEME";   /* Indicate that the process making this request should be traced.
                                                     All signals received by this process can be intercepted by its
                                                     parent, and its parent can use the other `ptrace' requests.  */
  case PTRACE_PEEKTEXT:   return "PTRACE_PEEKTEXT";  /* Return the word in the process's text space at address ADDR.  */
  case PTRACE_PEEKDATA:   return "PTRACE_PEEKDATA";  /* Return the word in the process's data space at address ADDR.  */
  case PTRACE_PEEKUSER:   return "PTRACE_PEEKUSER";  /* Return the word in the process's user area at offset ADDR.  */
  case PTRACE_POKETEXT:   return "PTRACE_POKETEXT";  /* Write the word DATA into the process's text space at address ADDR.  */
  case PTRACE_POKEDATA:   return "PTRACE_POKEDATA";  /* Write the word DATA into the process's data space at address ADDR.  */
  case PTRACE_POKEUSER:   return "PTRACE_POKEUSER";  /* Write the word DATA into the process's user area at offset ADDR.  */
  case PTRACE_CONT:       return "PTRACE_CONT";      /* Continue the process.  */
  case PTRACE_KILL:       return "PTRACE_KILL";      /* Kill the process.  */
  case PTRACE_SINGLESTEP: return "PTRACE_SINGLESTEP";/* Single step the process. This is not supported on all machines.  */
  case PTRACE_GETREGS:    return "PTRACE_GETREGS";   /* Get all general purpose registers used by a processes. This is not supported on all machines.  */
  case PTRACE_SETREGS:    return "PTRACE_SETREGS";   /* Set all general purpose registers used by a processes. This is not supported on all machines.  */
  case PTRACE_GETFPREGS:  return "PTRACE_GETFPREGS"; /* Get all floating point registers used by a processes. This is not supported on all machines.  */
  case PTRACE_SETFPREGS:  return "PTRACE_SETFPREGS"; /* Set all floating point registers used by a processes. This is not supported on all machines.  */
  case PTRACE_ATTACH:     return "PTRACE_ATTACH";    /* Attach to a process that is already running. */
  case PTRACE_DETACH:     return "PTRACE_DETACH";    /* Detach from a process attached to with PTRACE_ATTACH.  */
  case PTRACE_GETFPXREGS: return "PTRACE_GETFPXREGS";/* Get all extended floating point registers used by a processes. This is not supported on all machines.  */
  case PTRACE_SETFPXREGS: return "PTRACE_SETFPXREGS";/* Set all extended floating point registers used by a processes. This is not supported on all machines.  */
  case PTRACE_SYSCALL:    return "PTRACE_SYSCALL";   /* Continue and stop at the next (return from) syscall.  */
  case PTRACE_ARCH_PRCTL: return "PTRACE_ARCH_PRCTL";
  }
  return "?";
}

//--------------------------------------------------------------------------
static long qptrace(__ptrace_request request, pid_t pid, void *addr, void *data)
{
  long code = ptrace(request, pid, addr, data);
  if ( request != PTRACE_PEEKTEXT
    && request != PTRACE_PEEKUSER
    && (request != PTRACE_POKETEXT
    && request != PTRACE_POKEDATA
    && request != PTRACE_SETREGS
    && request != PTRACE_GETREGS
    && request != PTRACE_SETFPREGS
    && request != PTRACE_GETFPREGS
    && request != PTRACE_SETFPXREGS
    && request != PTRACE_GETFPXREGS
    || code != 0) )
  {
    int saved_errno = errno;
    //;!debdeb("%s(%u, 0x%X, 0x%X) => 0x%X", get_ptrace_name(request), pid, addr, data, code);
    if ( code == -1 )
    {
      //;!deberr("");
    }
    else
    {
      //;!debdeb("\n");
    }
    errno = saved_errno;
  }
  return code;
}

//--------------------------------------------------------------------------
static int qkill(int pid, int signo)
{
#ifdef LDEB
  msg("%d: sending signal %d\n", pid, signo);
#endif
  errno = 0;
  static bool tkill_failed = false;
  if ( !tkill_failed )
  {
    int ret = syscall(__NR_tkill, pid, signo);
    if ( errno != ENOSYS )
      return ret;
    errno = 0;
    tkill_failed = true;
  }
  return kill(pid, signo);
}

//--------------------------------------------------------------------------
#ifdef LDEB
static void log(thread_info_t *ti, const char *format, ...)
{
  if ( ti != NULL )
  {
    const char *name = "?";
    switch ( ti->state )
    {
      case RUNNING:        name = "RUN "; break;
      case STOPPED:        name = "STOP"; break;
      case PENDING_SIGNAL: name = "PEND"; break;
      case DYING:          name = "DYIN"; break;
      case DEAD:           name = "DEAD"; break;
    }
    msg("%d: state=%s %s ", ti->tid, name, ti->waiting_sigstop ? "WAIT" : "    ");
  }
  va_list va;
  va_start(va, format);
  vmsg(format, va);
  va_end(va);
}
#else
#define log(ti, format, args...)
#endif

//--------------------------------------------------------------------------
inline thread_info_t *linux_debmod_t::get_thread(thid_t tid)
{
  threads_t::iterator p = threads.find(tid);
  if ( p == threads.end() )
    return NULL;
  return &p->second;
}

//--------------------------------------------------------------------------
static ea_t get_ip(thid_t tid)
{
  const size_t pcreg_off = qoffsetof(user, regs) + qoffsetof(user_regs_struct, PCREG);
  return qptrace(PTRACE_PEEKUSER, tid, (void *)pcreg_off, 0);
}

#include "linux_threads.cpp"

//--------------------------------------------------------------------------
static unsigned long get_dr(thid_t tid, int idx)
{
  uchar *offset = (uchar *)qoffsetof(user, u_debugreg) + idx*sizeof(unsigned long int);
  unsigned long value = qptrace(PTRACE_PEEKUSER, tid, (void *)offset, 0);
  // msg("dr%d => %a\n", idx, value);
  return value;
}

//--------------------------------------------------------------------------
static bool set_dr(thid_t tid, int idx, unsigned long value)
{
  uchar *offset = (uchar *)qoffsetof(user, u_debugreg) + idx*sizeof(unsigned long int);

  if ( value == (unsigned long)(-1) )
    value = 0;          // linux does not accept too high values
  // msg("dr%d <= %a\n", idx, value);
  return qptrace(PTRACE_POKEUSER, tid, offset, (void *)value) == 0;
}

//--------------------------------------------------------------------------
pid_t linux_debmod_t::qwait(int pid, int *status, bool hang)
{
  int flags = hang ? 0 : WNOHANG;
  while ( true )
  {
    pid_t tid = waitpid(pid, status, flags | wait_flags);
    if ( tid == -1 )
    {
      switch ( errno )
      {
        case EINVAL:
          // msg("waitpid returned EINVAL, status=0x%02x\n", *status);
          // probably the OS does not support WCONTINUED or __WALL, try without them
          if ( (wait_flags & WCONTINUED) != 0 )
          {
            wait_flags &= ~WCONTINUED; // do not use WCONTINUED anymore
            continue;
          }
          if ( (wait_flags & __WALL) != 0 )
          {
            wait_flags &= ~__WALL; // do not use __WALL anymore
            continue;
          }
          break;
        case ECHILD:
          // msg("waitpid returned ECHILD, status=0x%02x\n", *status);
          tid = waitpid(pid, status, flags | __WCLONE);
          break;
        case EINTR:
          // msg("waitpid returned EINTR, status=0x%02x\n", *status);
          continue;
        default:
          // if ( errno != 0 )
          //   msg("waitpid failed with errno=%d, status=0x%02x\n", errno, *status);
          break;
      }
    }
    return tid;
  }
}

//--------------------------------------------------------------------------
bool linux_debmod_t::del_pending_event(event_id_t id, const char *module_name)
{
  for ( eventlist_t::iterator p=events.begin(); p != events.end(); ++p )
  {
    if ( p->eid == id && strcmp(p->modinfo.name, module_name) == 0 )
    {
      events.erase(p);
      return true;
    }
  }
  return false;
}

//--------------------------------------------------------------------------
void linux_debmod_t::enqueue_event(const debug_event_t &ev, queue_pos_t pos)
{
  if ( ev.eid != NO_EVENT )
  {
    events.enqueue(ev, pos);
    may_run = false;
    log(get_thread(ev.tid), "enqueued event, may not run!\n");
  }
}

//--------------------------------------------------------------------------
// we got a signal that does not belong to our thread. find the target thread
// and store the signal in its 'pending_signals'
static void store_pending_signal(int pid, int status)
{
  struct linux_signal_storer_t : public debmod_visitor_t
  {
    int pid;
    int status;
    linux_signal_storer_t(int p, int s) : pid(p), status(s) {}
    int visit(debmod_t *debmod)
    {
      linux_debmod_t *ld = (linux_debmod_t *)debmod;
      if ( ld->threads.find(pid) != ld->threads.end() )
      {
        ld->pending_signals.push_back(pid);
        ld->pending_signals.push_back(status);
        return 1; // stop
      }
      return 0; // continue
    }
  };
  linux_signal_storer_t lss(pid, status);
  if ( !for_all_debuggers(lss) ) // uses lock_begin(), lock_end() to protect common data
  {
    // maybe it comes from a zombie?
    // if we terminate the process, there might be some zombie threads remaining(?)
    if ( !WIFSIGNALED(status) )
    {
      msg("%d: failed to store pending status %x, killing unknown thread\n", pid, status);
      qptrace(PTRACE_KILL, pid, 0, 0);
    }
  }
}

//--------------------------------------------------------------------------
// check if 'pending_signals' has anything
bool linux_debmod_t::retrieve_pending_signal(pid_t *pid, int *status)
{
  if ( pending_signals.empty() )
    return false;

  bool has_pending_signal;
  lock_begin();
  {
    has_pending_signal = 1 < pending_signals.size();
    if ( has_pending_signal )
    {
      *pid = pending_signals[0];
      *status = pending_signals[1];
      intvec_t::iterator p = pending_signals.begin();
      pending_signals.erase(p, p+2);
    }
  }
  lock_end();
  if ( has_pending_signal )
    log(get_thread(*pid), "got pending signal status %x for pid %d\n", *status, *pid);
  return has_pending_signal;
}

//--------------------------------------------------------------------------
bool linux_debmod_t::emulate_retn(int tid)
{
  struct user_regs_struct regs;
  qptrace(PTRACE_GETREGS, tid, 0, &regs);
#ifdef __ARM__
  // emulate BX LR
  int tbit = regs.uregs[14] & 1;
  regs.PCREG = regs.uregs[14] & ~1;    // PC <- LR
  setflag(regs.uregs[16], 1<<5, tbit); // Set/clear T bit in PSR
#else
  if ( _read_memory(tid, regs.SPREG, &regs.PCREG, sizeof(regs.PCREG), false) != sizeof(regs.PCREG) )
  {
    log(NULL, "%d: reading return address from %a failed\n", tid, ea_t(regs.SPREG));
    if ( _read_memory(process_handle, regs.SPREG, &regs.PCREG, sizeof(regs.PCREG), false) != sizeof(regs.PCREG) )
    {
      log(NULL, "%d: reading return address from %a failed (2)\n", process_handle, ea_t(regs.SPREG));
      return false;
    }
  }
  regs.SPREG += sizeof(regs.PCREG);
  log(NULL, "%d: retn to %a\n", tid, ea_t(regs.PCREG));
#endif
  qptrace(PTRACE_SETREGS, tid, 0, &regs);
  return true;
}

//--------------------------------------------------------------------------
bool linux_debmod_t::gen_library_events(int tid)
{
  int s = events.size();
  r_debug rd;
  if ( r_debug_ea != 0
    && _read_memory(tid, r_debug_ea, &rd, sizeof(rd), false) == sizeof(rd) )
  {
    // if the state is not consistent, just return
    if ( rd.r_state != r_debug::RT_CONSISTENT )
      return false;

    // retrieve library names
    meminfo_vec_t miv;
    for ( link_map *ptr=rd.r_map; ptr != NULL; )
    {
      link_map map;
      char name[QMAXPATH];
      if ( _read_memory(tid, ea_t(ptr), &map, sizeof(map), false) != sizeof(map) )
        break;
      name[0] = '\0';
      _read_memory(tid, ea_t(map.l_name), name, sizeof(name), false);
//      dmsg("%a %s next=%x\n", map.l_addr, name, map.l_next);
      memory_info_t &mi = miv.push_back();
      mi.startEA = map.l_addr;
      mi.bitness = 1;
      mi.name = name;
      mi.perm = 0;
      mi.endEA = 0;
      ptr = map.l_next;
    }
    handle_dll_movements(miv);
  }
  return events.size() != s;
}

//--------------------------------------------------------------------------
bool linux_debmod_t::handle_hwbpt(debug_event_t *event)
{
#ifndef __ARM__
  uint32 dr6 = get_dr(event->tid, 6);
  for ( int i=0; i < MAX_BPT; i++ )
  {
    if ( dr6 & (1<<i) )  // Hardware breakpoint 'i'
    {
      if ( hwbpt_ea[i] == get_dr(event->tid, i) )
      {
        event->eid     = BREAKPOINT;
        event->bpt.hea = hwbpt_ea[i];
        event->bpt.kea = BADADDR;
        set_dr(event->tid, 6, 0); // Clear the status bits
        return true;
      }
    }
  }
#endif
  return false;
}

//--------------------------------------------------------------------------
inline ea_t calc_bpt_event_ea(const debug_event_t *event)
{
#ifdef __ARM__
  if ( event->exc.code == SIGTRAP )
    return event->ea;
#else
  if ( event->exc.code == SIGTRAP
   /* || event->exc.code == SIGSEGV */ ) // NB: there was a bug in 2.6.10 when int3 was reported as SIGSEGV instead of SIGTRAP
  {
    return event->ea - 1;               // x86 reports the address after the bpt
  }
#endif
  return BADADDR;
}

//--------------------------------------------------------------------------
// timeout in microseconds
// 0 - no timeout, return immediately
// -1 - wait forever
// returns: 1-ok, 0-failed
int linux_debmod_t::get_debug_event(const char *from, debug_event_t *event, int timeout_ms, bool autoresume)
{
  pid_t pid;
  int status;
  while ( true )
  {
    if ( retrieve_pending_signal(&pid, &status) )
      break;
    pid = qwait(-1, &status, timeout_ms == -1);
    if ( timeout_ms > 0 && pid <= 0 )
    {
      usleep(timeout_ms * 1000);
      pid = qwait(-1, &status, false);
    }
    if ( pid <= 0 )
      return false;
    log(get_thread(pid), " => waitpid returned status %x, pid=%d\n", status, pid);
    if ( threads.find(pid) != threads.end() )
      break;
    // we are not interested in this pid
    log(get_thread(pid), "storing status %d\n", status);
    store_pending_signal(pid, status);
    timeout_ms = 0;
  }

  thread_info_t *ti = get_thread(pid);
  if ( ti == NULL )
  {
    // not our thread?!
    debdeb("EVENT FOR UNKNOWN THREAD %d, IGNORED...\n", pid);
    int sig = WIFSTOPPED(status) ? WSTOPSIG(status) : 0;
    qptrace(PTRACE_CONT, pid, 0, (void*)(sig));
    return false;
  }
  QASSERT(ti->state != STOPPED || exited);

  event->pid     = process_handle;
  event->tid     = pid;
  event->ea      = get_ip(event->tid);
  event->handled = true;
  if ( WIFSTOPPED(status) )
  {
    ea_t proc_ip;
    bool suspend;
    const exception_info_t *ei;
    int code = WSTOPSIG(status);
    log(ti, "%s (stopped), nstopped=%d\n", strsignal(code), nstopped);
    thstate_t oldstate = ti->state;
    if ( oldstate == PENDING_SIGNAL )
    {
      if ( ti->waiting_sigstop )
        oldstate = RUNNING;
    }
    ti->state = STOPPED;
    nstopped++;
    QASSERT(nstopped <= threads.size());
    event->eid = EXCEPTION;
    event->exc.code     = code;
    event->exc.can_cont = true;
    event->exc.ea       = BADADDR;
    if ( code == SIGSTOP && ti->waiting_sigstop )
    {
      log(ti, "got pending SIGSTOP!\n");
      ti->waiting_sigstop = false;
      if ( ti->suspend_count == 0 )
        goto RESUME; // silently resume
      event->eid = NO_EVENT;
      return true;
    }
    ei = find_exception(code);
    if ( ei != NULL )
    {
      qsnprintf(event->exc.info, sizeof(event->exc.info), "got %s signal (%s)", ei->name.c_str(), ei->desc.c_str());
      suspend = ei->break_on();
      if ( ei->handle() )
        code = 0;               // mask the signal
      else
        suspend = false;        // do not stop if the signal will be handled by the app
    }
    else
    {
      qsnprintf(event->exc.info, sizeof(event->exc.info), "got unknown signal #%d", code);
      suspend = true;
    }
    proc_ip = calc_bpt_event_ea(event);
    if ( proc_ip != BADADDR )
    {
      if ( proc_ip == shlib_bpt.bpt_addr && shlib_bpt.bpt_addr != 0 )
      {
        log(ti, "got shlib bpt %a\n", proc_ip);
        // emulate return from function
        suspend_all_threads("shlib_bpt");
        if ( !emulate_retn(pid) )
        {
          msg("%a: could not return from the shlib breakpoint!\n", proc_ip);
          return true;
        }
        if ( !gen_library_events(pid) ) // something has changed in shared libraries?
        { // no, nothing has changed
          log(ti, "nothing has changed in dlls\n");
RESUME:

          if ( !in_event && !has_pending_events() )
          {
            log(ti, "autoresuming, oldstate=%s...\n", oldstate == RUNNING ? "RUN" : oldstate == STOPPED ? "STP" : oldstate == PENDING_SIGNAL ? "PND" : "???");
            //if ( !may_run )
              resume_app_if_no_pending_events();
            //else
            //  qresume_thread(*ti);
            if ( oldstate == PENDING_SIGNAL )
              oldstate = RUNNING;
            ti->state = oldstate;
            return false;
          }
          log(ti, "app may not run, keeping it suspended\n");
          return false;
        }
        log(ti, "gen_library_events ok\n");
        event->eid = NO_EVENT;
      }
      else if ( proc_ip == birth_bpt.bpt_addr && birth_bpt.bpt_addr != 0
             || proc_ip == death_bpt.bpt_addr && death_bpt.bpt_addr != 0 )
      {
        log(ti, "got thread bpt %a (%s)\n", proc_ip, proc_ip == birth_bpt.bpt_addr ? "birth" : "death");
        size_t s = events.size();
        thread_handle = pid; // for ps_pdread
        // NB! if we don't do this, some running threads can interfere with thread_db
        suspend_all_threads("thread_bpt");
        tdb_handle_messages(pid);
        // emulate return from function
        if ( !emulate_retn(pid) )
        {
          msg("%a: could not return from the thread breakpoint!\n", proc_ip);
          return true;
        }
        if ( s == events.size() )
        {
          log(ti, "resuming after thread_bpt\n");
          goto RESUME;
        }
        event->eid = NO_EVENT;
      }
      else
      {
        if ( !handle_hwbpt(event) )
        {
          if ( bpts.find(proc_ip) != bpts.end() )
          {
            event->eid     = BREAKPOINT;
            event->bpt.hea = BADADDR;
            event->bpt.kea = BADADDR;
            event->ea      = proc_ip;
          }
          else if ( ti->single_step )
          {
            event->eid = STEP;
          }
          else
          {
            msg("Unknown breakpoint: %a\n", proc_ip);
          }
        }
      }
      code = 0;
    }
    ti->child_signum = code;
    if ( !suspend && event->eid == EXCEPTION )
    {
      log(ti, "resuming after exception %d\n", code);
      goto RESUME;
    }
    //suspend_all_threads("get_debug_event");
  }
  else
  {
    if ( WIFSIGNALED(status) )
    {
      log(ti, "%s (terminated)\n", strsignal(WTERMSIG(status)));
      event->exit_code = WTERMSIG(status);
    }
    else
    {
      log(ti, "%d (exited)\n", WEXITSTATUS(status));
      event->exit_code = WEXITSTATUS(status);
    }
    //ti->child_signum = SIGSTOP;
    if ( threads.size() <= 1 || ti->tid == process_handle )
    {
      event->eid = PROCESS_EXIT;
      exited = true;
    }
    else
    {
      log(ti, "got a thread exit (state='%c')\n", getstate(ti->tid));
      event->eid = NO_EVENT;
      if ( ti->state == DYING || ti->state == DEAD )
      {
        // remove from internal list
        del_thread(event->tid);
      }
      else
      {
        ti->state = DEAD;
        dead_thread(event->tid);
      }
    }
  }
  log(ti, "low got event: %s, signum=%d\n", debug_event_str(event), ti->child_signum);
  ti->single_step = false;
  last_event = *event;
  return true;
}

//--------------------------------------------------------------------------
gdecode_t idaapi linux_debmod_t::dbg_get_debug_event(debug_event_t *event, int timeout_ms)
{
  QASSERT(!in_event || exited);
  while ( true )
  {
    // are there any pending events?
    if ( !events.empty() )
    {
      // get the first event and return it
      *event = events.front();
      events.pop_front();
      if ( event->eid == PROCESS_ATTACH )
        attaching = false;        // finally attached to it
      log(NULL, "GDE1: %s\n", debug_event_str(event));
      in_event = true;
      return events.empty() ? GDE_ONE_EVENT : GDE_MANY_EVENTS;
    }

    debug_event_t ev;
    if ( !get_debug_event("fromkernel", &ev, timeout_ms, false) )
      break;
    enqueue_event(ev, IN_BACK);
  }
  return GDE_NO_EVENT;
}

//--------------------------------------------------------------------------
static char getstate(int tid)
{
  char buf[QMAXPATH];
  qsnprintf(buf, sizeof(buf), "/proc/%u/status", tid);
  FILE *fp = fopenRT(buf);
  if ( fp == NULL )
    return ' ';
  qfgets(buf, sizeof(buf), fp);
  qfgets(buf, sizeof(buf), fp);
  char st;
  if ( qsscanf(buf, "State:  %c", &st) != 1 )
    INTERR();
  qfclose(fp);
  return st;
}

//--------------------------------------------------------------------------
// Make sure that the thread is suspended
bool linux_debmod_t::qsuspend_thread(thread_info_t &ti)
{
  if ( ti.state == RUNNING && !ti.waiting_sigstop )
  {
    char st = getstate(ti.tid);
    if ( st == 'T' ) // already stopped?
    {
      log(&ti, "thread already stopped?!\n");
    }
    else
    {
      log(&ti, "st=%c sending SIGSTOP\n", st);
      qkill(ti.tid, SIGSTOP);
      QASSERT(!ti.waiting_sigstop || exited);
      ti.waiting_sigstop = true;
    }

    int status = 0, tid;
    while ( true )
    {
      tid = qwait(ti.tid, &status, false);
      if ( tid > 0 )
        break;
      st = getstate(ti.tid);
      if ( st != 'R' && st != 'S' )
        break;
    }
//    QASSERT(tid == ti.tid); // even if the thread dies, we should first get thread_bpt
    // we can another event, though :(
    if ( WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP )
    {
      ti.state = STOPPED;
      ti.waiting_sigstop = false;
      nstopped++;
      log(&ti, "suspended successfully!\n", status);
    }
    else if ( tid > 0 )
    { // we got an event, but our SIGSTOP will fire back in the future :(
      store_pending_signal(tid, status);
      ti.state = PENDING_SIGNAL;
      log(&ti, "did not get SIGSTOP but got status %x for tid %d (ip=%a)!\n", status, tid, get_ip(ti.tid));
    }

  }
  ti.suspend_count++;
  return true;
}

//--------------------------------------------------------------------------
inline bool linux_debmod_t::has_pending_events(void)
{
  return !events.empty() || !pending_signals.empty();
}

//--------------------------------------------------------------------------
bool linux_debmod_t::qresume_thread(thread_info_t &ti)
{
  log(&ti, "%d: resume thread (pending=%d suspend_count=%d, ip=%a)\n", ti.tid, has_pending_events(), ti.suspend_count, get_ip(ti.tid));

  if ( ti.suspend_count > 0 )
  {
    if ( --ti.suspend_count > 0 )
      return true;
  }

  if ( (!may_run && ti.state != DYING) || exited )
    return true;

  if ( ti.state == STOPPED || ti.state == DYING )
  {
    __ptrace_request request = ti.single_step ? PTRACE_SINGLESTEP : PTRACE_CONT;
    log(&ti, "  PTRACE_%s, signum=%d, nstopped=%d, state: '%c'\n", ti.single_step ? "SINGLESTEP" : "CONT", ti.child_signum, nstopped, getstate(ti.tid));
    if ( qptrace(request, ti.tid, 0, (void *)ti.child_signum) != 0 && ti.state != DYING )
    {
      log(&ti, "    failed to resume thread (error %d)\n", errno);
      if ( getstate(ti.tid) != 'Z' )
        return false;
      // we have a zombie thread
      // report its death
      dead_thread(ti.tid);
    }
    if ( ti.state == DYING )
      ti.state = DEAD;
    else
      ti.state = RUNNING;
    nstopped--;
    QASSERT(nstopped >= 0);
  }
  return true;
}

//--------------------------------------------------------------------------
bool linux_debmod_t::suspend_all_threads(const char *name)
{
#ifdef LDEB
  //msg("\nsuspending app(%s)\n", name);
#endif
  bool ok = true;
  for ( threads_t::iterator p=threads.begin(); p != threads.end(); ++p )
  {
    thread_info_t &ti = p->second;
    if ( !qsuspend_thread(ti) )
      ok = false;
  }
  return ok;
  return true;
}

//--------------------------------------------------------------------------
bool linux_debmod_t::resume_all_threads(const char *from)
{
#ifdef LDEB
  //msg("resuming app(%s): (pending:%d, attach:%d)\n", from, has_pending_events(), attaching);
#endif
  bool ok = true;
  for ( threads_t::iterator p=threads.begin(); p != threads.end(); ++p )
  {
    thread_info_t &ti = p->second;
    if ( !qresume_thread(ti) )
      ok = false;
  }
#ifdef LDEB
  //msg("resuming app(%s): (pending:%d, attach:%d)\n", from, has_pending_events(), attaching);
  if ( nstopped )
  {
    msg("resume_all_threads(%s): %d threads still stopped", from, nstopped);
    for ( threads_t::iterator p=threads.begin(); p != threads.end(); ++p )
    {
      thread_info_t &ti = p->second;
      if ( ti.suspend_count > 0 )
        msg(" %d", ti.tid);
    }
    msg("\n");
  }
#endif
  return ok;
}

//--------------------------------------------------------------------------
ssize_t idaapi linux_debmod_t::dbg_write_file(int fn, uint32 off, const void *buf, size_t size)
{
  return 0;
}

//--------------------------------------------------------------------------
ssize_t idaapi linux_debmod_t::dbg_read_file(int fn, uint32 off, void *buf, size_t size)
{
  return 0;
}

//--------------------------------------------------------------------------
int idaapi linux_debmod_t::dbg_open_file(const char *file, uint32 *fsize, bool readonly)
{
  return 0;
}

//--------------------------------------------------------------------------
void idaapi linux_debmod_t::dbg_close_file(int fn)
{
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_continue_after_event(const debug_event_t *event)
{
  if ( event == NULL )
    return 0;

  int tid = event->tid;
  thread_info_t *t = get_thread(tid);
  if ( t == NULL && !exited )
  {
    dwarning("could not find thread %d!\n", tid);
    return 0;
  }

  log(t, "continue after event %s%s\n", debug_event_str(event), has_pending_events() ? " (there are pending events)" : "");

  if ( t != NULL )
  {
    if ( event->eid != THREAD_START
      && event->eid != THREAD_EXIT
      && event->eid != LIBRARY_LOAD
      && event->eid != LIBRARY_UNLOAD
      && (event->eid != EXCEPTION || event->handled) )
    {
      t->child_signum = 0;
    }

    if ( t->state == DYING )
    {
      // this thread is about to exit; resume it so it can do so
      qresume_thread(*t);
    }
    else if ( t->state == DEAD )
    {
      // remove from internal list
      del_thread(event->tid);
    }
  }
  return resume_app_if_no_pending_events();
}

//--------------------------------------------------------------------------
bool linux_debmod_t::resume_app_if_no_pending_events(void)
{
  if ( !has_pending_events() )
  {
    may_run = true;
    if ( !removed_bpts.empty() )
    {
      for ( easet_t::iterator p=removed_bpts.begin(); p != removed_bpts.end(); ++p )
        bpts.erase(*p);
      removed_bpts.clear();
    }
  }

  in_event = false;
  bool ok = resume_all_threads("continue_after_event");
//  msg("all threads\n");
//  display_all_threads();
  return ok;
}

// PTRACE_PEEKTEXT / PTRACE_POKETEXT operate on unsigned long values! (i.e. 4 bytes on x86 and 8 bytes on x64)
#define PEEKSIZE sizeof(unsigned long)

//--------------------------------------------------------------------------
int linux_debmod_t::_read_memory(int tid, ea_t ea, void *buffer, int size, bool suspend)
{
  if ( exited || process_handle == INVALID_HANDLE_VALUE )
    return -1;

  // stop all threads before accessing the process memory
  if ( suspend )
    suspend_all_threads("read_memory");

  if ( tid == -1 )
    tid = process_handle;

  int read_size = 0;
  bool tried_mem = false;
  bool tried_peek = false;
  // don't use memory for short reads
  if ( size > 3 * PEEKSIZE )
  {
TRY_MEMFILE:
    char filename[64];
    qsnprintf (filename, sizeof(filename), "/proc/%d/mem", tid);
    int fd = open(filename, O_RDONLY | O_LARGEFILE);
    if ( fd != -1 )
    {
      read_size = pread64 (fd, buffer, size, ea);
      close (fd);
    }
    // msg("%d: pread64 %d:%a:%d => %d\n", tid, fp, ea, size, read_size);

#ifdef LDEB
    if ( read_size < size )
      perror("read_memory: pread64 failed");
#endif
    tried_mem = true;
  }

  if ( read_size != size && !tried_peek )
  {
    uchar *ptr = (uchar *)buffer;
    read_size = 0;
    tried_peek = true;
    while ( read_size < size )
    {
      const int shift = ea & (PEEKSIZE-1);
      int part = shift;
      if ( part == 0 ) part = PEEKSIZE;
      if ( part > size ) part = size;
      unsigned long v = qptrace(PTRACE_PEEKTEXT, tid, (void *)(unsigned int)(ea-shift), 0);
      if ( errno != 0 )
      {
#ifdef LDEB
        msg("PEEKTEXT %d:%a => error %d\n", tid, ea-shift, errno);
        perror("PEEKTEXT failed");
#endif
        break;
      }
      else
      {
        //msg("PEEKTEXT %d:%a => OK\n", tid, ea-shift);
      }
      if ( part == PEEKSIZE )
      {
        *(unsigned long*)ptr = v;
      }
      else
      {
        v >>= shift*8;
        for ( int i=0; i < part; i++ )
        {
          ptr[i] = uchar(v);
          v >>= 8;
        }
      }
      ptr  += part;
      ea   += part;
      read_size += part;
    }
  }

  // sometimes PEEKTEXT fails but memfile succeeds... so try both
  if ( read_size < size && !tried_mem )
    goto TRY_MEMFILE;

  if ( suspend )
    resume_all_threads("read_memory");
  // msg("READ MEMORY (%d): %d\n", tid, read_size);
  return read_size > 0 ? read_size : 0;
}

//--------------------------------------------------------------------------
int linux_debmod_t::_write_memory(int tid, ea_t ea, const void *buffer, int size, bool suspend)
{
  if ( exited || process_handle == INVALID_HANDLE_VALUE )
    return -1;

  // stop all threads before accessing the process memory
  if ( suspend )
    suspend_all_threads("write_memory");

  if ( exited )
    return -1;

  if ( tid == -1 )
    tid = process_handle;
  int bs = bpt_code.size();
  if ( size == bs )      // might be a breakpoint add/del
  {
    if ( memcmp(buffer, bpt_code.begin(), bs) == 0 )
    {
//      msg("%a: add bpt\n", ea);
      bpts.insert(ea);
      removed_bpts.erase(ea);
    }
    else if ( bpts.find(ea) != bpts.end() )
    {
//      msg("%a: del bpt\n", ea);
      // we can not immediately remove the breakpoint from the list
      // because there might be SIGTRAPs associated with it. We will clean
      // up 'bpts' at the continuation time.
      removed_bpts.insert(ea);
    }
  }

  int ok = size;
  const uchar *ptr = (const uchar *)buffer;
  errno = 0;

  while ( size > 0 )
  {
    const int shift = ea & (PEEKSIZE-1);
    int part = shift;
    if ( part == 0 )
      part = PEEKSIZE;
    if ( part > size )
      part = size;
    unsigned long word;
    memcpy(&word, ptr, qmin(sizeof(word), part)); // use memcpy() to read unaligned bytes
    if ( part != PEEKSIZE )
    {
      unsigned long old = qptrace(PTRACE_PEEKTEXT, tid, (void *)(unsigned long)(ea-shift), 0);
      if ( errno != 0 )
      {
        ok = 0;
        break;
      }
      unsigned long mask = ~0;
      mask >>= (PEEKSIZE - part)*8;
      mask <<= shift*8;
      word <<= shift*8;
      word &= mask;
      word |= old & ~mask;
    }
    errno = 0;
    qptrace(PTRACE_POKEDATA, process_handle, (void *)(unsigned long)(ea-shift), (void *)word);
    if ( errno )
    {
      errno = 0;
      qptrace(PTRACE_POKETEXT, process_handle, (void *)(unsigned long)(ea-shift), (void *)word);
    }
    if ( errno )
    {
      ok = 0;
      break;
    }
    ptr  += part;
    ea   += part;
    size -= part;
  }

  if ( suspend )
    resume_all_threads("write_memory");

  //  debdeb("WRITE MEMORY: END\n");
  return ok;
}

//--------------------------------------------------------------------------
ssize_t idaapi linux_debmod_t::dbg_write_memory(ea_t ea, const void *buffer, size_t size)
{
  return _write_memory(-1, ea, buffer, size, true);
}

//--------------------------------------------------------------------------
ssize_t idaapi linux_debmod_t::dbg_read_memory(ea_t ea, void *buffer, size_t size)
{
  return _read_memory(-1, ea, buffer, size, true);
}

//--------------------------------------------------------------------------
void linux_debmod_t::add_dll(ea_t base, asize_t size, const char *modname, const char *soname)
{
  debdeb("%a: new dll %s (soname=%s)\n", base, modname, soname);
  debug_event_t ev;
  ev.eid     = LIBRARY_LOAD;
  ev.pid     = process_handle;
  ev.tid     = process_handle;
  ev.ea      = base;
  ev.handled = true;
  qstrncpy(ev.modinfo.name, modname, sizeof(ev.modinfo.name));
  ev.modinfo.base = base;
  ev.modinfo.size = size;
  ev.modinfo.rebase_to = BADADDR;
  if ( is_dll && input_file_path == modname )
    ev.modinfo.rebase_to = base;
  enqueue_event(ev, IN_FRONT);

  image_info_t ii(base, ev.modinfo.size, modname, soname);
  dlls.insert(make_pair(ii.base, ii));
  dlls_to_import.insert(ii.base);
}

//--------------------------------------------------------------------------
bool linux_debmod_t::import_dll(image_info_t &ii, name_info_t &ni)
{
  struct dll_symbol_importer_t : public symbol_visitor_t
  {
    linux_debmod_t *ld;
    image_info_t &ii;
    name_info_t &ni;
    dll_symbol_importer_t(linux_debmod_t *_ld, image_info_t &_ii, name_info_t &_ni)
      : symbol_visitor_t(VISIT_SYMBOLS), ld(_ld), ii(_ii), ni(_ni) {}
    int visit_symbol(ea_t ea, const char *name)
    {
      ea += ii.base;
      ni.addrs.push_back(ea);
      ni.names.push_back(qstrdup(name));
      ii.names[ea] = name;
      // every 10000th name send a message to ida - we are alive!
      if ( (ni.addrs.size() % 10000) == 0 )
        ld->dmsg("");
      return 0;
    }
  };
  if ( ii.base == BADADDR )
  {
    debdeb("Can't import symbols from %s: no imagebase\n", ii.fname.c_str());
    return false;
  }
  dll_symbol_importer_t dsi(this, ii, ni);
  return load_elf_symbols(ii.fname.c_str(), dsi) == 0;
}

//--------------------------------------------------------------------------
// enumerate names from the specified shared object and save the results
// we'll need to send it to IDA later
// if libname == NULL, enum all modules
void linux_debmod_t::enum_names(const char *libname)
{
  if ( dlls_to_import.empty() )
    return;

  for ( easet_t::iterator p=dlls_to_import.begin(); p != dlls_to_import.end(); )
  {
    images_t::iterator q = dlls.find(*p);
    if ( q != dlls.end() )
    {
      image_info_t &ii = q->second;
      if ( libname != NULL && strcmp(libname, ii.soname.c_str()) != 0 )
      {
        ++p;
        continue;
      }
      import_dll(ii, pending_names);
    }
    dlls_to_import.erase(p++);
  }
}

//--------------------------------------------------------------------------
ea_t linux_debmod_t::find_pending_name(const char *name)
{
  if ( name == NULL )
    return BADADDR;
  for ( int i=0; i < pending_names.addrs.size(); ++i )
    if ( strcmp(pending_names.names[i], name) == 0 )
      return pending_names.addrs[i];
  return BADADDR;
}

//--------------------------------------------------------------------------
void idaapi linux_debmod_t::dbg_stopped_at_debug_event(void)
{
  // we will take advantage of this event to import information
  // about the exported functions from the loaded dlls
  enum_names();

  name_info_t &ni = *get_debug_names();
  ni = pending_names; // NB: ownership of name pointers is transferred
  pending_names.addrs.clear();
  pending_names.names.clear();
}

//--------------------------------------------------------------------------
void linux_debmod_t::cleanup(void)
{
  // if the process is still running, kill it, otherwise it runs uncontrolled
  // normally the process is dead at this time but may survive if we arrive
  // here after an interr.
  if ( process_handle != INVALID_HANDLE_VALUE )
    dbg_exit_process();
  process_handle = INVALID_HANDLE_VALUE;
  thread_handle  = INVALID_HANDLE_VALUE;
  is_dll = false;
  exited = false;
  in_event = false;

  threads.clear();
  dlls.clear();
  dlls_to_import.clear();
  images.clear();
  events.clear();
  if ( mapfp != NULL )
  {
    qfclose(mapfp);
    mapfp = NULL;
  }

  complained_shlib_bpt = false;
  attaching = false;
  bpts.clear();

  tdb_delete();
  erase_internal_bp(birth_bpt);
  erase_internal_bp(death_bpt);
  erase_internal_bp(shlib_bpt);
  r_debug_ea = 0;
  nstopped = 0;
  interp.clear();

  inherited::cleanup();
}

//--------------------------------------------------------------------------
//
//      DEBUGGER INTERFACE FUNCTIONS
//
//--------------------------------------------------------------------------
inline const char *skipword(const char *ptr)
{
  while ( !isspace(*ptr) && *ptr !='\0' )
    ptr++;
  return ptr;
}

//--------------------------------------------------------------------------
// Returns the file name assciated with pid
static bool get_exec_fname(int pid, char *buf, size_t bufsize)
{
  char path[QMAXPATH];
  qsnprintf(path, sizeof(path), "/proc/%u/exe", pid);
  int len = readlink(path, buf, bufsize-1);
  if ( len > 0 )
  {
    buf[len] = '\0';
    return true;
  }
  else
  {
    qstrncpy(buf, path, bufsize);
    return false;
  }
}

//--------------------------------------------------------------------------
static bool read_command_line(process_info_t *info)
{
  char buf[QMAXPATH];
  qsnprintf(buf, sizeof(buf), "/proc/%u/cmdline", info->pid);
  FILE *cmdfp = qfopen(buf, "r");
  if ( cmdfp == NULL )
    return false;

  int size = qfread(cmdfp, buf, sizeof(buf));
  qfclose(cmdfp);

  char *ptr = info->name;
  char *end = info->name + sizeof(info->name);
  for ( int i=0; i < size; )
  {
    const char *in = &buf[i];
    if ( i != 0 )
      APPCHAR(ptr, end, ' ');

    bool quoted = false;
    if ( strchr(in, ' ') != NULL || strchr(in, '"') != NULL )
    {
      APPCHAR(ptr, end, '"');
      quoted = true;
    }
    str2user(ptr, buf+i, end-ptr);
    ptr = tail(ptr);
    if ( quoted )
      APPEND(ptr, end, "\""); // add terminating zero too

    i += strlen(in) + 1;
  }
  return true;
}

//--------------------------------------------------------------------------
void linux_debmod_t::refresh_process_list(void)
{
  int mypid = getpid();
  processes.clear();
  qffblk_t fb;
  for ( int code=qfindfirst("/proc/*", &fb, FA_DIREC);
        code == 0;
        code = qfindnext(&fb) )
  {
    if ( !isdigit(fb.ff_name[0]) )
      continue;
    process_info_t info;
    info.pid = atoi(fb.ff_name);
    if ( info.pid == mypid )
      continue;
    if ( !get_exec_fname(info.pid, info.name, sizeof(info.name)) )
      continue; // we skip the process because we can not debug it anyway
// if the input file is specified, display only the matching processes
//    if ( !input_file_path.empty()
//      if ( strcmp(qbasename(input_file_path.c_str()), qbasename(info.name)) != 0 )
//        continue;
    read_command_line(&info);
    processes.push_back(info);
  }
  qfindclose(&fb);
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
// input is valid only if n==0
int idaapi linux_debmod_t::dbg_process_get_info(int n, const char *input, process_info_t *info)
{
  if ( n == 0 )
  {
    input_file_path = input;
    refresh_process_list();
  }

  if ( n < 0 || n >= processes.size() )
    return false;

  if ( info != NULL )
    *info = processes[n];
  return true;
}

//--------------------------------------------------------------------------
// find a dll in the memory information array
static const memory_info_t *find_dll(const meminfo_vec_t &miv, const char *name)
{
  for ( int i=0; i < miv.size(); i++ )
    if ( miv[i].name == name )
      return &miv[i];
  return NULL;
}

//--------------------------------------------------------------------------
bool linux_debmod_t::add_shlib_bpt(const meminfo_vec_t &miv)
{
  if ( shlib_bpt.bpt_addr != 0 )
    return true;

  qstring interp_soname;
  if ( interp.empty() )
  {
    // find out the loader name
    struct interp_finder_t : public symbol_visitor_t
    {
      qstring interp;
      interp_finder_t(void) : symbol_visitor_t(VISIT_INTERP) {}
      int visit_symbol(ea_t ea, const char *name) { return 0; } // unused
      int visit_interp(const char *name)
      {
        interp = name;
        return 2;
      }
    };
    interp_finder_t itf;
    const char *exename = exe_path.c_str();
    int code = load_elf_symbols(exename, itf);
    if ( code == 0 )
    { // no interpreter
      if ( !complained_shlib_bpt )
      {
        complained_shlib_bpt = true;
        dwarning("%s:\n"
                 "Could not find the elf interpreter name,\n"
                 "shared object events will not be reported", exename);
      }
      return false;
    }
    if ( code != 2 )
    {
      dwarning("%s: could not read symbols on remote computer", exename);
      return false;
    }
    char path[QMAXPATH];
    qmake_full_path(path, sizeof(path), itf.interp.c_str());
    interp_soname.swap(itf.interp);
    interp = path;
  }

  // check if it is present in the memory map (normally it is)
  debdeb("INTERP: %s, SONAME: %s\n", interp.c_str(), interp_soname.c_str());
  const memory_info_t *mi = find_dll(miv, interp.c_str());
  if ( mi == NULL )
  {
    dwarning("%s: could not find in process memory", interp.c_str());
    return false;
  }

  asize_t size = calc_module_size(miv, mi);
  add_dll(mi->startEA, size, interp.c_str(), interp_soname.c_str());

  // set bpt at r_brk
  enum_names(interp_soname.c_str()); // update the name list
  ea_t ea = find_pending_name("_r_debug");
  if ( ea != BADADDR )
  {
    struct r_debug rd;
    if ( _read_memory(-1, ea, &rd, sizeof(rd), false) == sizeof(rd) )
    {
      r_debug_ea = ea;
      if ( rd.r_brk != 0 )
      {
        if ( !add_internal_bp(shlib_bpt, rd.r_brk) )
          debdeb("%a: could not set shlib bpt (r_debug_ea=%a)\n", rd.r_brk, r_debug_ea);
      }
    }
  }
  if ( shlib_bpt.bpt_addr == 0 )
  {
    static const char *const shlib_bpt_names[] =
    {
      "r_debug_state",
      "_r_debug_state",
      "_dl_debug_state",
      "rtld_db_dlactivity",
      "_rtld_debug_state",
      NULL
    };

    for ( int i=0; i < qnumber(shlib_bpt_names); i++ )
    {
      ea_t ea = find_pending_name(shlib_bpt_names[i]);
      if ( ea != BADADDR && ea != 0 )
      {
        if ( add_internal_bp(shlib_bpt, ea) )
          break;
        debdeb("%a: could not set shlib bpt (name=%s)\n", ea, shlib_bpt_names[i]);
      }
    }
    if ( shlib_bpt.bpt_addr == 0 )
      return false;
  }
  debdeb("%a: added shlib bpt (r_debug_ea=%a)\n", shlib_bpt.bpt_addr, r_debug_ea);
  return true;
}

//--------------------------------------------------------------------------
void linux_debmod_t::add_thread(int tid)
{
  threads.insert(std::make_pair(tid, thread_info_t(tid)));
  nstopped++;
}

//--------------------------------------------------------------------------
void linux_debmod_t::del_thread(int tid)
{
  threads_t::iterator p = threads.find(tid);
  QASSERT(p != threads.end());
  if ( p->second.state == STOPPED )
    nstopped--;
  threads.erase(p);
}

//--------------------------------------------------------------------------
bool linux_debmod_t::handle_process_start(pid_t pid, bool attaching)
{
  process_handle = pid;
  add_thread(pid);
  int status;
  waitpid(pid, &status, 0); // (should succeed) consume SIGSTOP
  debdeb("process pid/tid: %d\n", pid);
  may_run = false;

  debug_event_t ev;
  ev.eid     = PROCESS_START;
  ev.pid     = pid;
  ev.tid     = pid;
  ev.ea      = get_ip(pid);
  ev.handled = true;
  get_exec_fname(pid, ev.modinfo.name, sizeof(ev.modinfo.name));
  ev.modinfo.base = BADADDR;
  ev.modinfo.size = 0;
  ev.modinfo.rebase_to = BADADDR;

  char fname[QMAXPATH];
  qsnprintf(fname, sizeof(fname), "/proc/%u/maps", pid);
  mapfp = fopenRT(fname);
  if ( mapfp == NULL )
  {
OPEN_ERROR:
    dmsg("%s: %s\n", fname, winerr(errno));
    return false;               // if fails, the process did not start
  }

  exe_path = ev.modinfo.name;
  if ( !is_dll )
    input_file_path = exe_path;

  // find the executable base
  meminfo_vec_t miv;
  if ( get_memory_info(miv, false) <= 0 )
    INTERR();

  const memory_info_t *mi = find_dll(miv, ev.modinfo.name);
  if ( mi != NULL )
  {
    ev.modinfo.base = mi->startEA;
    if ( !is_dll ) // exe files: rebase idb to the loaded address
      ev.modinfo.rebase_to = mi->startEA;
  }
  else
  {
    if ( !is_dll )
      dmsg("%s: nowhere in the process memory?!\n", ev.modinfo.name);
  }

  if ( !add_shlib_bpt(miv) )
    dmsg("Could not set the shlib bpt, shared object events will not be handled\n");

  enqueue_event(ev, IN_FRONT);
  if ( attaching )
  {
    ev.eid = PROCESS_ATTACH;
    enqueue_event(ev, IN_BACK);
    // collect exported names from the main module
    qstring soname;
    get_soname(ev.modinfo.name, &soname);
    image_info_t ii(ev.modinfo.base, ev.modinfo.size, ev.modinfo.name, soname);
    import_dll(ii, pending_names);
  }

  tdb_new(pid); // initialize multi-thread support
  return true;
}

//--------------------------------------------------------------------------
static void idaapi kill_all_processes(void)
{
  struct process_killer_t : public debmod_visitor_t
  {
    int visit(debmod_t *debmod)
    {
      linux_debmod_t *ld = (linux_debmod_t *)debmod;
      if ( ld->process_handle != INVALID_HANDLE_VALUE )
        qkill(ld->process_handle, SIGKILL);
      return 0;
    }
  };
  process_killer_t pk;
  for_all_debuggers(pk);
}

//--------------------------------------------------------------------------
int idaapi linux_debmod_t::dbg_start_process(const char *path,
                                             const char *args,
                                             const char *startdir,
                                             int flags,
                                             const char *input_path,
                                             uint32 input_file_crc32)
{
  // input file specified in the database does not exist
  if ( input_path[0] != '\0' && !qfileexist(input_path) )
    return -2;

  // temporary thing, later we will retrieve the real file name
  // based on the process id
  input_file_path = input_path;
  is_dll = (flags & DBG_PROC_IS_DLL) != 0;

  if ( !qfileexist(path) )
  {
    dmsg("%s: %s\n", path, winerr(errno));
    return -1;
  }

  int mismatch = 0;
  if ( !check_input_file_crc32(input_file_crc32) )
    mismatch = CRC32_MISMATCH;

  if ( startdir[0] != '\0' && chdir(startdir) == -1 )
  {
    dmsg("chdir: %s\n", winerr(errno));
    return -1;
  }

  qstring errbuf;
  launch_process_t lpi;
  lpi.path = path;
  lpi.args = args;
  lpi.flags = LP_TRACE;
  void *child_pid = init_process(lpi, &errbuf);
  if ( child_pid == NULL )
  {
    dmsg("%s", errbuf.c_str());
    return -1;
  }
  if ( !handle_process_start(size_t(child_pid), false) )
  {
    dbg_exit_process();
    return -1;
  }
  return 1 | mismatch;
}


//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_attach_process(pid_t pid, int /*event_id*/)
{
  if ( qptrace(PTRACE_ATTACH, pid, NULL, NULL) == 0
    && handle_process_start(pid, true) )
  {
    gen_library_events(pid); // detect all loaded libraries

    // block the process until all generated events are processed
    attaching = true;
    return true;
  }
  qptrace(PTRACE_DETACH, pid, NULL, NULL);
  return false;
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_detach_process(void)
{
  // disable all internal breakpoints so that they don't kill the process after we detach
  erase_internal_bp(birth_bpt);
  erase_internal_bp(death_bpt);
  erase_internal_bp(shlib_bpt);

  bool had_pid = false;
  bool ok = true;
  log(NULL, "detach all threads.\n");
  for ( threads_t::iterator p=threads.begin(); ok && p != threads.end(); ++p )
  {
    thread_info_t &ti = p->second;
    if ( ti.tid == process_handle )
      had_pid = true;

    ok = qptrace(PTRACE_DETACH, ti.tid, NULL, NULL) == 0;
    log(NULL, "detach tid %d: ok=%d\n", ti.tid, ok);
  }

  if ( ok && !had_pid )
  {
    // if pid was not in the thread list, detach it separately
    ok = qptrace(PTRACE_DETACH, process_handle, NULL, NULL) == 0;
    log(NULL, "detach pid %d: ok=%d\n", process_handle, ok);
  }
  if ( ok )
  {
    debug_event_t ev;
    ev.eid     = PROCESS_DETACH;
    ev.pid     = process_handle;
    ev.tid     = process_handle;
    ev.ea      = BADADDR;
    ev.handled = true;
    enqueue_event(ev, IN_BACK);
    in_event = false;
    exited = true;
    threads.clear();
    process_handle = INVALID_HANDLE_VALUE;
    return true;
  }
  return false;
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_prepare_to_pause_process(void)
{
  qkill(process_handle, SIGSTOP);
  may_run = false;
  return true;
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_exit_process(void)
{
  bool ok = true;
  suspend_all_threads("exit_process");
  for ( threads_t::iterator p=threads.begin(); p != threads.end(); ++p )
  {
    thread_info_t &ti = p->second;
    if ( ti.state == STOPPED )
    {
      if ( qptrace(PTRACE_KILL, p->first, 0, (void*)SIGKILL) != 0 )
        ok = false;
    }
    else
    {
      if ( ti.tid != INVALID_HANDLE_VALUE && qkill(ti.tid, SIGKILL) != 0 )
        ok = false;
    }
    ti.state = RUNNING;
  }
  if ( ok )
    process_handle = INVALID_HANDLE_VALUE;
  may_run = true;
  exited = true;
  return ok;
}

//--------------------------------------------------------------------------
// Set hardware breakpoints for one thread
bool linux_debmod_t::set_hwbpts(HANDLE hThread)
{
#ifdef __ARM__
  return false;
#else
  uchar *offset = (uchar *)qoffsetof(user, u_debugreg);

  bool ok = set_dr(hThread, 0, hwbpt_ea[0])
    && set_dr(hThread, 1, hwbpt_ea[1])
    && set_dr(hThread, 2, hwbpt_ea[2])
    && set_dr(hThread, 3, hwbpt_ea[3])
    && set_dr(hThread, 6, 0)
    && set_dr(hThread, 7, dr7);
  // msg("set_hwbpts: DR0=%a DR1=%a DR2=%a DR3=%a DR7=%a => %d\n",
  //       hwbpt_ea[0],
  //       hwbpt_ea[1],
  //       hwbpt_ea[2],
  //       hwbpt_ea[3],
  //       dr7,
  //       ok);
  return ok;
#endif
}

//--------------------------------------------------------------------------
bool linux_debmod_t::refresh_hwbpts(void)
{
  for ( threads_t::iterator p=threads.begin(); p != threads.end(); ++p )
    if ( !set_hwbpts(p->second.tid) )
      return false;
  return true;
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_add_bpt(bpttype_t type, ea_t ea, int len)
{
  if ( type == BPT_SOFT )
  {
    int bs = bpt_code.size();
    return dbg_write_memory(ea, bpt_code.begin(), bs) == bs;
  }

#ifndef __ARM__
  int i = find_hwbpt_slot(ea);    // get slot number
  if ( i != -1 )
  {
    hwbpt_ea[i] = ea;

    int lenc;                               // length code used by the processor
    if ( len == 1 ) lenc = 0;
    if ( len == 2 ) lenc = 1;
    if ( len == 4 ) lenc = 3;

    dr7 |= (1 << (i*2));            // enable local breakpoint
    dr7 |= (type << (16+(i*2)));    // set breakpoint type
    dr7 |= (lenc << (18+(i*2)));    // set breakpoint length

    return refresh_hwbpts();
  }
#endif
  return false;
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_del_bpt(ea_t ea, const uchar *orig_bytes, int len)
{
  if ( orig_bytes != NULL )
    return dbg_write_memory(ea, orig_bytes, len) == len;

#ifndef __ARM__
  for ( int i=0; i < MAX_BPT; i++ )
  {
    if ( hwbpt_ea[i] == ea )
    {
      hwbpt_ea[i] = BADADDR;            // clean the address
      dr7 &= ~(3 << (i*2));             // clean the enable bits
      dr7 &= ~(0xF << (i*2+16));        // clean the length and type
      return refresh_hwbpts();
    }
  }
#endif
  return false;
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_thread_get_sreg_base(thid_t tid, int sreg_value, ea_t *pea)
{
#ifdef __ARM__
  return 0;
#else
  // find out which selector we're asked to retrieve
  struct user_regs_struct regs;
  if ( qptrace(PTRACE_GETREGS, tid, 0, &regs) != 0 )
    return 0;

#ifdef __X64__
#define INTEL_REG(reg) reg
#else
#define INTEL_REG(reg) x##reg
#endif

  if ( sreg_value == regs.INTEL_REG(fs) )
    return thread_get_fs_base(tid, R_FS, pea);
  else if ( sreg_value == regs.INTEL_REG(gs) )
    return thread_get_fs_base(tid, R_GS, pea);
  else
    *pea = 0; // all other selectors (cs, ds) usually have base of 0...
  return 1;
#endif
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_thread_suspend(thid_t tid)
{
return true;
  thread_info_t *ti = get_thread(tid);
  if ( ti == NULL )
    return false;
  return qsuspend_thread(*ti);
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_thread_continue(thid_t tid)
{
  thread_info_t *ti = get_thread(tid);
  if ( ti == NULL )
    return false;
  return qresume_thread(*ti);
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_thread_set_step(thid_t tid)
{
  thread_info_t *t = get_thread(tid);
  if ( t == NULL )
    return false;
  t->single_step = true;
  return true;
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_read_registers(thid_t tid, int clsmask, regval_t *values)
{
  if ( values == NULL )
    return 0;

  struct user_regs_struct regs;
  if ( qptrace(PTRACE_GETREGS, tid, 0, &regs) != 0 )
    return false;

#ifdef __ARM__
  if ( (clsmask & ARM_RC_GENERAL) != 0 )
  {
    values[R_R0].ival     = regs.uregs[0];
    values[R_R1].ival     = regs.uregs[1];
    values[R_R2].ival     = regs.uregs[2];
    values[R_R3].ival     = regs.uregs[3];
    values[R_R4].ival     = regs.uregs[4];
    values[R_R5].ival     = regs.uregs[5];
    values[R_R6].ival     = regs.uregs[6];
    values[R_R7].ival     = regs.uregs[7];
    values[R_R8].ival     = regs.uregs[8];
    values[R_R9].ival     = regs.uregs[9];
    values[R_R10].ival    = regs.uregs[10];
    values[R_R11].ival    = regs.uregs[11];
    values[R_R12].ival    = regs.uregs[12];
    values[R_SP].ival     = regs.uregs[13];
    values[R_LR].ival     = regs.uregs[14];
    values[R_PC].ival     = regs.uregs[15];
    values[R_PSR].ival    = regs.uregs[16];
  }
#elif defined(__X64__)
  if ( (clsmask & X86_RC_GENERAL) != 0 )
  {
    values[R_EAX].ival    = regs.rax;
    values[R_EBX].ival    = regs.rbx;
    values[R_ECX].ival    = regs.rcx;
    values[R_EDX].ival    = regs.rdx;
    values[R_ESI].ival    = regs.rsi;
    values[R_EDI].ival    = regs.rdi;
    values[R_EBP].ival    = regs.rbp;
    values[R_ESP].ival    = regs.rsp;
    values[R_EIP].ival    = regs.rip;
    values[R64_R8 ].ival  = regs.r8;
    values[R64_R9 ].ival  = regs.r9;
    values[R64_R10].ival  = regs.r10;
    values[R64_R11].ival  = regs.r11;
    values[R64_R12].ival  = regs.r12;
    values[R64_R13].ival  = regs.r13;
    values[R64_R14].ival  = regs.r14;
    values[R64_R15].ival  = regs.r15;
    values[R_EFLAGS].ival = regs.eflags;
  }
  if ( (clsmask & X86_RC_SEGMENTS) != 0 )
  {
    values[R_CS    ].ival = regs.cs;
    values[R_DS    ].ival = regs.ds;
    values[R_ES    ].ival = regs.es;
    values[R_FS    ].ival = regs.fs;
    values[R_GS    ].ival = regs.gs;
    values[R_SS    ].ival = regs.ss;
  }
#else
  if ( (clsmask & X86_RC_GENERAL) != 0 )
  {
    values[R_EAX   ].ival = uint32(regs.eax);
    values[R_EBX   ].ival = uint32(regs.ebx);
    values[R_ECX   ].ival = uint32(regs.ecx);
    values[R_EDX   ].ival = uint32(regs.edx);
    values[R_ESI   ].ival = uint32(regs.esi);
    values[R_EDI   ].ival = uint32(regs.edi);
    values[R_EBP   ].ival = uint32(regs.ebp);
    values[R_ESP   ].ival = uint32(regs.esp);
    values[R_EIP   ].ival = uint32(regs.eip);
    values[R_EFLAGS].ival = uint32(regs.eflags);
  }
  if ( (clsmask & X86_RC_SEGMENTS) != 0 )
  {
    values[R_CS    ].ival = uint32(regs.xcs);
    values[R_DS    ].ival = uint32(regs.xds);
    values[R_ES    ].ival = uint32(regs.xes);
    values[R_FS    ].ival = uint32(regs.xfs);
    values[R_GS    ].ival = uint32(regs.xgs);
    values[R_SS    ].ival = uint32(regs.xss);
  }
#endif

#ifndef __ARM__
#ifdef __X64__
  // 64-bit version uses one struct to return xmm & fpu
  if ( (clsmask & (X86_RC_XMM|X86_RC_FPU)) != 0 )
  {
    struct user_fpregs_struct i387;
    if ( qptrace(PTRACE_GETFPREGS, tid, 0, &i387) != 0 )
      return false;

    if ( (clsmask & (X86_RC_FPU|X86_RC_MMX)) != 0 )
    {
      if ( (clsmask & X86_RC_FPU) != 0 )
      {
        values[R_CTRL].ival = i387.cwd;
        values[R_STAT].ival = i387.swd;
        values[R_TAGS].ival = i387.ftw;
      }
      read_fpu_registers(values, clsmask, i387.st_space, 10);
    }
    if ( (clsmask & X86_RC_XMM) != 0 )
    {
      uchar *xptr = (uchar *)i387.xmm_space;
      for ( int i=R_XMM0; i < R_MXCSR; i++,xptr+=16 )
        values[i].set_bytes(xptr, 16);
      values[R_MXCSR].ival = i387.mxcsr;
    }
  }
#else
  // 32-bit version uses two different structures to return xmm & fpu
  if ( (clsmask & X86_RC_XMM) != 0 )
  {
    struct user_fpxregs_struct x387;
    if ( qptrace(PTRACE_GETFPXREGS, tid, 0, &x387) != 0 )
      return false;

    uchar *xptr = (uchar *)x387.xmm_space;
    for ( int i=R_XMM0; i < R_MXCSR; i++,xptr+=16 )
      values[i].set_bytes(xptr, 16);
    values[R_MXCSR].ival = x387.mxcsr;
  }
  if ( (clsmask & (X86_RC_FPU|X86_RC_MMX)) != 0 )
  {
    struct user_fpregs_struct i387;
    if ( qptrace(PTRACE_GETFPREGS, tid, 0, &i387) != 0 )
      return false;

    if ( (clsmask & X86_RC_FPU) != 0 )
    {
      values[R_CTRL].ival = uint32(i387.cwd);
      values[R_STAT].ival = uint32(i387.swd);
      values[R_TAGS].ival = uint32(i387.twd);
    }
    read_fpu_registers(values, clsmask, i387.st_space, 10);
  }
#endif
#endif
  return true;
}

//--------------------------------------------------------------------------
inline int get_reg_class(int idx)
{
#ifdef __ARM__
  return ARM_RC_GENERAL;
#else
  return get_x86_reg_class(idx);
#endif
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
static bool patch_reg_context(
        struct user_regs_struct *regs,
        struct user_fpregs_struct *i387,
        struct user_fpxregs_struct *x387,
        int reg_idx,
        const regval_t *value)
{
  if ( value == NULL )
    return false;

#if defined(__ARM__)
  if ( reg_idx >= qnumber(regs->uregs) || regs == NULL )
    return false;
  regs->uregs[reg_idx]  = value->ival;
#else
  int regclass = get_reg_class(reg_idx);
  if ( (regclass & (X86_RC_GENERAL|X86_RC_SEGMENTS)) != 0 )
  {
    if ( regs == NULL )
      return false;
    switch ( reg_idx )
    {
#if defined(__X64__)
      case R_CS:     regs->cs     = value->ival; break;
      case R_DS:     regs->ds     = value->ival; break;
      case R_ES:     regs->es     = value->ival; break;
      case R_FS:     regs->fs     = value->ival; break;
      case R_GS:     regs->gs     = value->ival; break;
      case R_SS:     regs->ss     = value->ival; break;
      case R_EAX:    regs->rax    = value->ival; break;
      case R_EBX:    regs->rbx    = value->ival; break;
      case R_ECX:    regs->rcx    = value->ival; break;
      case R_EDX:    regs->rdx    = value->ival; break;
      case R_ESI:    regs->rsi    = value->ival; break;
      case R_EDI:    regs->rdi    = value->ival; break;
      case R_EBP:    regs->rbp    = value->ival; break;
      case R_ESP:    regs->rsp    = value->ival; break;
      case R_EIP:    regs->rip    = value->ival; break;
      case R64_R8:   regs->r8     = value->ival; break;
      case R64_R9 :  regs->r9     = value->ival; break;
      case R64_R10:  regs->r10    = value->ival; break;
      case R64_R11:  regs->r11    = value->ival; break;
      case R64_R12:  regs->r12    = value->ival; break;
      case R64_R13:  regs->r13    = value->ival; break;
      case R64_R14:  regs->r14    = value->ival; break;
      case R64_R15:  regs->r15    = value->ival; break;
#else
      case R_CS:     regs->xcs    = value->ival; break;
      case R_DS:     regs->xds    = value->ival; break;
      case R_ES:     regs->xes    = value->ival; break;
      case R_FS:     regs->xfs    = value->ival; break;
      case R_GS:     regs->xgs    = value->ival; break;
      case R_SS:     regs->xss    = value->ival; break;
      case R_EAX:    regs->eax    = value->ival; break;
      case R_EBX:    regs->ebx    = value->ival; break;
      case R_ECX:    regs->ecx    = value->ival; break;
      case R_EDX:    regs->edx    = value->ival; break;
      case R_ESI:    regs->esi    = value->ival; break;
      case R_EDI:    regs->edi    = value->ival; break;
      case R_EBP:    regs->ebp    = value->ival; break;
      case R_ESP:    regs->esp    = value->ival; break;
      case R_EIP:    regs->eip    = value->ival; break;
#endif
      case R_EFLAGS: regs->eflags = value->ival; break;
    }
  }
  else if ( (regclass & X86_RC_XMM) != 0 )
  {
    if ( XMM_STRUCT == NULL )
      return false;
    if ( reg_idx == R_MXCSR )
    {
      XMM_STRUCT->mxcsr = value->ival;
    }
    else
    {
      uchar *xptr = (uchar *)XMM_STRUCT->xmm_space + (reg_idx - R_XMM0) * 16;
      const void *vptr = value->get_data();
      size_t size = value->get_data_size();
      memcpy(xptr, vptr, qmin(size, 16));
    }
  }
  else if ( (regclass & X86_RC_FPU) != 0 )
  { // FPU register
    if ( i387 == NULL )
      return false;
    if ( reg_idx >= R_ST0+FPU_REGS_COUNT ) // FPU status registers
    {
      switch ( reg_idx )
      {
        case R_CTRL:   i387->cwd = value->ival; break;
        case R_STAT:   i387->swd = value->ival; break;
        case R_TAGS:   i387->TAGS_REG = value->ival; break;
      }
    }
    else // FPU floating point register
    {
      uchar *fpu_float = (uchar *)i387->st_space;
      fpu_float += (reg_idx-R_ST0) * 10;
      memcpy(fpu_float, value->fval, 10);
    }
  }
  else if ( (regclass & X86_RC_MMX) != 0 )
  {
    if ( i387 == NULL )
      return false;
    uchar *fpu_float = (uchar *)i387->st_space;
    fpu_float += (reg_idx-R_MMX0) * 10;
    memcpy(fpu_float, value->get_data(), 8);
  }
#endif
  return true;
}

//--------------------------------------------------------------------------
// 1-ok, 0-failed
int idaapi linux_debmod_t::dbg_write_register(thid_t tid, int reg_idx, const regval_t *value)
{
  if ( value == NULL )
    return false;

  bool ret = false;
  int regclass = get_reg_class(reg_idx);
  if ( (regclass & CLASS_OF_INTREGS) != 0 )
  {
    struct user_regs_struct regs;
    if ( qptrace(PTRACE_GETREGS, tid, 0, &regs) != 0 )
      return false;

    if ( reg_idx == PCREG_IDX )
      log(get_thread(tid), "NEW EIP: %08lX\n", value->ival);

    if ( !patch_reg_context(&regs, NULL, NULL, reg_idx, value) )
      return false;

    ret = qptrace(PTRACE_SETREGS, tid, 0, &regs) != -1;
  }
#ifndef __ARM__
  else if ( (regclass & CLASSES_STORED_IN_FPREGS) != 0 )
  {
    struct user_fpregs_struct i387;
    if ( qptrace(PTRACE_GETFPREGS, tid, 0, &i387) != 0 )
      return false;

    if ( !patch_reg_context(NULL, &i387, NULL, reg_idx, value) )
      return false;

    ret = qptrace(PTRACE_SETFPREGS, tid, 0, &i387) != -1;
  }
#ifndef __X64__ // only for 32-bit debugger we have to handle xmm registers separately
  else if ( (regclass & X86_RC_XMM) != 0 )
  {
    struct user_fpxregs_struct x387;
    if ( qptrace(PTRACE_GETFPXREGS, tid, 0, &x387) != 0 )
      return false;

    if ( !patch_reg_context(NULL, NULL, &x387, reg_idx, value) )
      return false;

    ret = qptrace(PTRACE_SETFPXREGS, tid, 0, &x387) != -1;
  }
#endif
#endif
  return ret;
}

//--------------------------------------------------------------------------
bool idaapi linux_debmod_t::write_registers(
  thid_t tid,
  int start,
  int count,
  const regval_t *values,
  const int *indices)
{
  struct user_regs_struct regs;
  struct user_fpregs_struct i387;
#if !defined(__ARM__) && !defined( __X64__)
  // only for 32-bit debugger we have to handle xmm registers separately
  struct user_fpxregs_struct x387;
#define X387_PTR &x387
#else
#define X387_PTR NULL
#endif
  bool got_regs = false;
  bool got_i387 = false;
  bool got_x387 = false;

  for ( int i=0; i < count; i++, values++ )
  {
    int idx = indices != NULL ? indices[i] : start+i;
    int regclass = get_reg_class(idx);
    if ( (regclass & CLASS_OF_INTREGS) != 0 )
    { // general register
      if ( !got_regs )
      {
        if ( qptrace(PTRACE_GETREGS, tid, 0, &regs) != 0 )
          return false;
        got_regs = true;
      }
    }
#if !defined(__ARM__)
    else if ( (regclass & CLASSES_STORED_IN_FPREGS) != 0 )
    { // fpregs register
      if ( !got_i387 )
      {
        if ( qptrace(PTRACE_GETFPREGS, tid, 0, &i387) != 0 )
          return false;
        got_i387 = true;
      }
    }
#ifndef __X64__
    else if ( (regclass & X86_RC_XMM) != 0 )
    {
      if ( !got_x387 )
      {
        if ( qptrace(PTRACE_GETFPXREGS, tid, 0, &x387) != 0 )
          return false;
        got_x387 = true;
      }
    }
#endif
#endif
    if ( !patch_reg_context(&regs, &i387, X387_PTR, idx, values) )
      return false;
  }

  if ( got_regs && qptrace(PTRACE_SETREGS, tid, 0, &regs) == -1 )
    return false;

#if !defined(__ARM__)
  if ( got_i387 && qptrace(PTRACE_SETFPREGS, tid, 0, &i387) == -1 )
    return false;

#ifndef __X64__
  if ( got_x387 && qptrace(PTRACE_SETFPXREGS, tid, 0, &x387) == -1 )
    return false;
#endif
#endif

  return true;
}

//--------------------------------------------------------------------------
// find DT_SONAME of a elf image directly from the memory
bool linux_debmod_t::get_soname(const char *fname, qstring *soname)
{
  struct dll_soname_finder_t : public symbol_visitor_t
  {
    qstring *soname;
    dll_soname_finder_t(qstring *res) : symbol_visitor_t(VISIT_DYNINFO), soname(res) {}
    virtual int visit_dyninfo(uint64 tag, const char *name, uint64 value)
    {
      if ( tag == DT_SONAME )
      {
        *soname = name;
        return 1;
      }
      return 0;
    }
  };

  dll_soname_finder_t dsf(soname);
  return load_elf_symbols(fname, dsf) == 1;
}

//--------------------------------------------------------------------------
asize_t linux_debmod_t::calc_module_size(const meminfo_vec_t &miv, const memory_info_t *mi)
{
  QASSERT(miv.begin() <= mi && mi < miv.end());
  ea_t start = mi->startEA;
  ea_t end   = mi->endEA;
  if ( end == 0 )
    return 0; // unknown size
  const qstring &name = mi->name;
  while ( ++mi != miv.end() )
  {
    if ( name != mi->name )
      break;
    end = mi->endEA;
  }
  QASSERT(end > start);
  return end - start;
}

//--------------------------------------------------------------------------
void linux_debmod_t::handle_dll_movements(const meminfo_vec_t &miv)
{
  log(NULL, "handle_dll_movements\n");
  // unload missing dlls
  images_t::iterator p;
  for ( p=dlls.begin(); p != dlls.end(); )
  {
    image_info_t &ii = p->second;
    const char *fname = ii.fname.c_str();
    if ( find_dll(miv, fname) == NULL )
    {
      if ( !del_pending_event(LIBRARY_LOAD, fname) )
      {
        debug_event_t ev;
        ev.eid     = LIBRARY_UNLOAD;
        ev.pid     = process_handle;
        ev.tid     = process_handle;
        ev.ea      = BADADDR;
        ev.handled = true;
        qstrncpy(ev.info, fname, sizeof(ev.info));
        enqueue_event(ev, IN_FRONT);
      }
      dlls.erase(p++);
    }
    else
    {
      ++p;
    }
  }

  // load new dlls
  int n = miv.size();
  for ( int i=0; i < n; i++ )
  {
    // ignore unnamed dlls
    if ( miv[i].name.empty() )
      continue;

    // ignore the input file
    if ( !is_dll && miv[i].name == input_file_path )
      continue;

    // ignore if dll already exists
    ea_t base = miv[i].startEA;
    p = dlls.find(base);
    if ( p != dlls.end() )
      continue;

    // ignore memory chunks which do not correspond to an ELF header
    char magic[4];
    if ( _read_memory(-1, base, &magic, 4, false) != 4 )
      continue;

    if ( memcmp(magic, "\x7F\x45\x4C\x46", 4) != 0 )
      continue;

    qstring soname;
    const char *modname = miv[i].name.c_str();
    get_soname(modname, &soname);
    asize_t size = calc_module_size(miv, &miv[i]);
    add_dll(base, size, modname, soname.c_str());
  }
  if ( !dlls_to_import.empty() )
    tdb_new(process_handle); // initialize multi-thread support
}

//--------------------------------------------------------------------------
bool linux_debmod_t::read_mapping(
        ea_t *ea1,
        ea_t *ea2,
        char *perm,
        ea_t *offset,
        char *device,
        uint64 *inode,
        char *filename)
{
  char line[2*MAXSTR];
  if ( !qfgets(line, sizeof(line), mapfp) )
    return false;

  *ea1 = BADADDR;
  filename[0] = '\0';

  int len = 0;
  int code = qsscanf(line, "%a-%a %s %a %s %" FMT_64 "x%n",
    ea1, ea2, perm, offset, device, inode, &len);
  if ( code == 6 && len < sizeof(line) )
  {
    char *ptr = &line[len];
    ptr = skipSpaces(ptr);
    qstrncpy(filename, ptr, QMAXPATH);
    ptr = tail(filename);
    while ( ptr > filename && isspace(ptr[-1]) )
      ptr--;
    *ptr = '\0';
  }
  return true;
}

//--------------------------------------------------------------------------
int linux_debmod_t::get_memory_info(meminfo_vec_t &miv, bool suspend)
{
  log(NULL, "get_memory_info(suspend=%d)\n", suspend);
  if ( exited )
    return -1;
  if ( suspend )
    suspend_all_threads("get_memory_info");

  ea_t ea1, ea2, offset;
  uint64 inode;
  char perm[8], device[8], fname[QMAXPATH];
  rewind(mapfp);
  while ( read_mapping(&ea1, &ea2, perm, &offset, device, &inode, fname) )
  {
    if ( ea1 == BADADDR )               // could not make sense of the line
      continue;
    // for some reason linux lists some areas twice
    // ignore them
    int i;
    for ( i=0; i < miv.size(); i++ )
      if ( miv[i].startEA == ea1 )
        break;
    if ( i != miv.size() )
      continue;

    memory_info_t &mi = miv.push_back();
    mi.startEA = ea1;
    mi.endEA   = ea2;
    mi.name    = trim(skipSpaces(fname));
#ifdef __EA64__
    mi.bitness = 2; // 64bit
#else
    mi.bitness = 1; // 32bit
#endif
    // msg("%s: %a..%a\n", mi.name.c_str(), mi.startEA, mi.endEA);

    if ( strchr(perm, 'r') != NULL ) mi.perm |= SEGPERM_READ;
    if ( strchr(perm, 'w') != NULL ) mi.perm |= SEGPERM_WRITE;
    if ( strchr(perm, 'x') != NULL ) mi.perm |= SEGPERM_EXEC;
  }

  if ( suspend )
    resume_all_threads("get_memory_info");
  return 1;
}

//--------------------------------------------------------------------------
int idaapi linux_debmod_t::dbg_get_memory_info(meminfo_vec_t &areas)
{
  int code = get_memory_info(areas, false);
  if ( code == 1 )
  {
    if ( same_as_oldmemcfg(areas) )
      code = -2;
    else
      save_oldmemcfg(areas);
  }
  return code;
}

linux_debmod_t::~linux_debmod_t()
{
}

//--------------------------------------------------------------------------
int idaapi linux_debmod_t::dbg_init(bool _debug_debugger)
{
  debug_debugger = _debug_debugger;
  dbg_term(); // initialize various variables
  return 3; // process_get_info, detach
}

//--------------------------------------------------------------------------
void idaapi linux_debmod_t::dbg_term(void)
{
  cleanup();
  cleanup_hwbpts();
  exe_path.qclear();
}

//--------------------------------------------------------------------------
bool idaapi linux_debmod_t::thread_get_fs_base(thid_t tid, int reg_idx, ea_t *pea)
{
#ifdef __X64__

  /* The following definitions come from prctl.h, but may be absent
     for certain configurations.  */
  #ifndef ARCH_GET_FS
  #define ARCH_SET_GS 0x1001
  #define ARCH_SET_FS 0x1002
  #define ARCH_GET_FS 0x1003
  #define ARCH_GET_GS 0x1004
  #endif

  switch ( reg_idx )
  {
    case R_FS:
      if ( ptrace (PTRACE_ARCH_PRCTL, tid, pea, ARCH_GET_FS) == 0 )
    return true;
      break;
    case R_GS:
      if ( ptrace (PTRACE_ARCH_PRCTL, tid, pea, ARCH_GET_GS) == 0 )
    return true;
      break;
    case R_CS:
    case R_DS:
    case R_ES:
    case R_SS:
      *pea = 0;
      return true;
  }
  return false;
#else
  qnotused(tid);
  qnotused(reg_idx);
  qnotused(pea);
  return false;
#endif
}

//--------------------------------------------------------------------------
int idaapi linux_debmod_t::handle_ioctl(int fn, const void *in, size_t insize, void **, ssize_t *)
{
  if ( fn == 0 )  // chmod +x
  {
    char *fname = (char *)in;
    qstatbuf st;
    qstat(fname, &st);
    int mode = st.st_mode | S_IXUSR|S_IXGRP|S_IXOTH;
    chmod(fname, mode);
  }
  return 0;
}

//--------------------------------------------------------------------------
qmutex_t g_mutex = NULL;

bool init_subsystem()
{
  if ( (g_mutex = qmutex_create()) == NULL )
    return false;

  tdb_init();

  qatexit(kill_all_processes);
  return true;
}

//--------------------------------------------------------------------------
bool lock_begin()
{
  qmutex_lock(g_mutex);
  return true;
}

//--------------------------------------------------------------------------
bool lock_end()
{
  qmutex_unlock(g_mutex);
  return true;
}

//--------------------------------------------------------------------------
bool term_subsystem()
{
  del_qatexit(kill_all_processes);
  tdb_term();
  qmutex_free(g_mutex);
  return true;
}

//--------------------------------------------------------------------------
debmod_t *create_debug_session()
{
  return new linux_debmod_t();
}
