#ifndef __LINUX_DEBUGGER_MODULE__
#define __LINUX_DEBUGGER_MODULE__

#include <signal.h>
#include <sys/ptrace.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

extern "C"
{
#include <thread_db.h>
}

#include <algorithm>
#include <map>
#include <set>
#include <deque>

#ifdef __ARM__
#  define BASE_DEBUGGER_MODULE arm_debmod_t
#  include "arm_debmod.h"
#  define BPT_CODE_SIZE ARM_BPT_SIZE
#else
#  define BASE_DEBUGGER_MODULE pc_debmod_t
#  include "pc_debmod.h"
#  define BPT_CODE_SIZE X86_BPT_SIZE
#endif

typedef int HANDLE;
typedef uint32 DWORD;
#define INVALID_HANDLE_VALUE (-1)

using std::pair;
using std::make_pair;

typedef qvector<process_info_t> processes_t;
typedef std::set<ea_t> easet_t;         // set of addresses
typedef std::map<ea_t, qstring> ea2name_t;
typedef std::map<qstring, ea_t> name2ea_t;

// image information
struct image_info_t
{
  image_info_t() : base(BADADDR), size(0) {}
  image_info_t(
        ea_t _base,
        asize_t _size,
        const qstring &_fname,
        const qstring &_soname)
    : base(_base), size(_size), fname(_fname), soname(_soname) { }
  ea_t base;
  asize_t size;         // image size, currently 0
  qstring fname;
  qstring soname;
  ea2name_t names;
};
typedef std::map<ea_t, image_info_t> images_t; // key: image base address

enum thstate_t
{
  RUNNING,            // running
  STOPPED,            // waiting to be resumed after waitpid
  PENDING_SIGNAL,     // got a signal but not handled it yet
};

// thread information
struct thread_info_t
{
  thread_info_t(int t)
    : tid(t), suspend_count(0), child_signum(0), single_step(false),
      state(STOPPED), waiting_sigstop(false), thr(NULL) {}
  int tid;
  int suspend_count;
  int child_signum;
  bool single_step;
  thstate_t state;
  bool waiting_sigstop;
  const td_thrhandle_t *thr;
};

typedef std::map<HANDLE, thread_info_t> threads_t; // (tid -> info)

enum ps_err_e
{
  PS_OK,      /* Success.  */
  PS_ERR,     /* Generic error.  */
  PS_BADPID,  /* Bad process handle.  */
  PS_BADLID,  /* Bad LWP id.  */
  PS_BADADDR, /* Bad address.  */
  PS_NOSYM,   /* Symbol not found.  */
  PS_NOFREGS  /* FPU register set not available.  */
};
struct ps_prochandle
{
  pid_t pid;
};
#define UINT32_C uint32

struct internal_bpt
{
  ea_t bpt_addr;
  uchar saved[BPT_CODE_SIZE];
  internal_bpt(): bpt_addr(0) {};
  ea_t getaddr() { return bpt_addr; };
};

class linux_debmod_t: public BASE_DEBUGGER_MODULE
{
  typedef BASE_DEBUGGER_MODULE inherited;

  // thread_db related data and functions:
  struct ps_prochandle prochandle;
  td_thragent_t *ta;

  internal_bpt birth_bpt; //thread created
  internal_bpt death_bpt; //thread exited
  internal_bpt shlib_bpt; //shared lib list changed
  bool complained_shlib_bpt;

  bool add_internal_bp(internal_bpt &bp, ea_t addr)
  {
    bp.bpt_addr = addr;
    return _read_memory(addr, bp.saved, sizeof(bp.saved))
        && dbg_add_bpt(BPT_SOFT, addr, -1);
  };

  bool erase_internal_bp(internal_bpt &bp)
  {
    bool ok = bp.bpt_addr == 0 || dbg_del_bpt(bp.bpt_addr, bp.saved, sizeof(bp.saved));
    bp.bpt_addr = 0;
    return ok;
  };

  void tdb_enable_event(td_event_e event, internal_bpt *bp);
  void tdb_update_threads(void);
  bool tdb_new(int pid);

  // list of debug names not yet sent to IDA
  name_info_t pending_names;

public:
  // processes list information
  processes_t processes;
  easet_t dlls_to_import;          // list of dlls to import information from
  images_t dlls;                   // list of loaded DLLs
  images_t images;                 // list of detected PE images
  threads_t threads;

  // debugged process information
  HANDLE process_handle;
  HANDLE thread_handle;
  bool is_dll;             // Is dynamic library?
  bool exited;             // Did the process exit?
  intvec_t pending_signals;// signals retrieved by other threads

  easet_t bpts;     // breakpoint list
  easet_t removed_bpts; // removed breakpoints

  FILE *mapfp;             // map file handle
  int mem_handle;          // memory file handle

  bool attaching;          // Handling events linked to PTRACE_ATTACH, don't run the program
  int nstopped;            // number of threads in STOPPED state
  bool may_run;
  bool in_event;           // IDA kernel is handling a debugger event

  ea_t r_debug_ea;
  qstring interp;

  qstring exe_path;        // name of the executable file

  linux_debmod_t();
  ~linux_debmod_t();

  void add_thread(int tid);
  void del_thread(int tid);
  thread_info_t *get_thread(thid_t tid);
  bool retrieve_pending_signal(pid_t *pid, int *status);
  int get_debug_event(const char *from, debug_event_t *event, int timeout, bool autoresume=true);
  void enqueue_event(const debug_event_t &ev, int flags);
#define IN_FRONT 1
#define IN_BACK  0
  bool qsuspend_thread(thread_info_t &ti);
  bool qresume_thread(thread_info_t &ti);
  bool suspend_all_threads(const char *name);
  bool resume_all_threads(const char *name);
  bool resume_app_if_no_pending_events(void);
  bool has_pending_events(void);
  int _read_memory(ea_t ea, void *buffer, int size, bool suspend=false);
  int _write_memory(ea_t ea, const void *buffer, int size, bool suspend=false);
  void add_dll(image_info_t &ii);
  bool import_dll(image_info_t &ii, name_info_t &ni);
  void enum_names(const char *libpath=NULL);
  void delete_unloaded_names(image_info_t &ii);
  bool add_shlib_bpt(const meminfo_vec_t &miv);
  bool handle_shlib_bpt(thread_info_t &ti);
  void emulate_retn(int tid);
  void cleanup(void);
  void refresh_process_list(void);
  bool handle_process_start(pid_t pid);
  int get_memory_info(meminfo_vec_t &areas, bool suspend);
  void gen_libload_events(void);
  bool set_hwbpts(HANDLE hThread);
  virtual bool refresh_hwbpts();
  void handle_dll_movements(const meminfo_vec_t &miv);
  bool idaapi thread_get_fs_base(thid_t tid, int reg_idx, ea_t *pea);
  bool read_mapping(ea_t *ea1, ea_t *ea2,char *perm,ea_t *offset,char *device,uint64 *inode,char *filename);
  bool get_soname(const char *fname, qstring *soname);
  ea_t find_pending_name(const char *name);
  bool handle_hwbpt(debug_event_t *event);

  //
  virtual int idaapi dbg_init(bool _debug_debugger);
  virtual void idaapi dbg_term(void);
  virtual int  idaapi dbg_process_get_info(int n,
    const char *input,
    process_info_t *info);
  virtual int  idaapi dbg_detach_process(void);
  virtual int  idaapi dbg_start_process(const char *path,
    const char *args,
    const char *startdir,
    int flags,
    const char *input_path,
    uint32 input_file_crc32);
  virtual int  idaapi dbg_get_debug_event(debug_event_t *event, bool ida_is_idle);
  virtual int  idaapi dbg_attach_process(pid_t process_id, int event_id);
  virtual int  idaapi dbg_prepare_to_pause_process(void);
  virtual int  idaapi dbg_exit_process(void);
  virtual int  idaapi dbg_continue_after_event(const debug_event_t *event);
  virtual void idaapi dbg_stopped_at_debug_event(void);
  virtual int  idaapi dbg_thread_suspend(thid_t thread_id);
  virtual int  idaapi dbg_thread_continue(thid_t thread_id);
  virtual int  idaapi dbg_thread_set_step(thid_t thread_id);
  virtual int  idaapi dbg_thread_read_registers(thid_t thread_id,
    regval_t *values,
    int count);
  virtual int  idaapi dbg_thread_write_register(thid_t thread_id,
    int reg_idx,
    const regval_t *value);
  virtual int  idaapi dbg_thread_get_sreg_base(thid_t thread_id,
    int sreg_value,
    ea_t *ea);
  virtual int  idaapi dbg_get_memory_info(meminfo_vec_t &areas);
  virtual ssize_t idaapi dbg_read_memory(ea_t ea, void *buffer, size_t size);
  virtual ssize_t idaapi dbg_write_memory(ea_t ea, const void *buffer, size_t size);
  virtual int  idaapi dbg_add_bpt(bpttype_t type, ea_t ea, int len);
  virtual int  idaapi dbg_del_bpt(ea_t ea, const uchar *orig_bytes, int len);
  virtual int  idaapi dbg_open_file(const char *file, uint32 *fsize, bool readonly);
  virtual void idaapi dbg_close_file(int fn);
  virtual ssize_t idaapi dbg_read_file(int fn, uint32 off, void *buf, size_t size);
  virtual ssize_t idaapi dbg_write_file(int fn, uint32 off, const void *buf, size_t size);
  virtual int  idaapi handle_ioctl(int fn, const void *buf, size_t size, void **outbuf, ssize_t *outsize);
  virtual bool idaapi thread_write_registers(
    thid_t tid,
    int start,
    int count,
    const regval_t *values,
    const int *indices);

  // thread_db
  void display_thrinfo(thid_t tid);
  void display_all_threads();
};

#endif
