#include <loader.hpp>
#include "consts.h"

bool plugin_inited;
bool debugger_inited;
bool is_dll;


#if TARGET_PROCESSOR == PLFM_386
  #define REGISTERS                x86_registers
  #define REGISTERS_SIZE           qnumber(x86_registers)
  #define REGISTER_CLASSES         x86_register_classes
  #define REGISTER_CLASSES_DEFAULT X86_RC_GENERAL
  #define READ_REGISTERS           x86_read_registers
  #define WRITE_REGISTER           x86_write_register
#if DEBUGGER_ID != DEBUGGER_ID_GDB_USER
  #define is_valid_bpt             is_x86_valid_bpt
#endif
  #define BPT_CODE                 X86_BPT_CODE
  #define BPT_CODE_SIZE            X86_BPT_SIZE
#elif TARGET_PROCESSOR == PLFM_ARM
  #define REGISTERS                arm_registers
  #define REGISTERS_SIZE           qnumber(arm_registers)
  #define REGISTER_CLASSES         arm_register_classes
  #define REGISTER_CLASSES_DEFAULT ARM_RC_GENERAL
  #define READ_REGISTERS           arm_read_registers
  #define WRITE_REGISTER           arm_write_register
#if DEBUGGER_ID != DEBUGGER_ID_GDB_USER
  #define is_valid_bpt             is_arm_valid_bpt
#endif
  #define BPT_CODE                 ARM_BPT_CODE
  #define BPT_CODE_SIZE            ARM_BPT_SIZE
#else
  #error This processor is not supported yet
#endif

static const uchar bpt_code[] = BPT_CODE;

static bool ask_user_and_copy(const char *ipath);
//--------------------------------------------------------------------------
int idaapi is_ok_bpt(bpttype_t type, ea_t ea, int len)
{
  int ret = is_valid_bpt(type, ea, len);
  if ( ret != BPT_OK )
    return ret;

  return s_is_ok_bpt(type, ea, len);
}

//--------------------------------------------------------------------------
static int idaapi add_bpt(bpttype_t type, ea_t ea, int len)
{
  int ret = is_valid_bpt(type, ea, len);
  if ( ret != BPT_OK )
    return false;

  return s_add_bpt(type, ea, len);
}

//--------------------------------------------------------------------------
static int idaapi _attach_process(pid_t process_id, int event_id)
{
  int r = s_attach_process(process_id, event_id);
  // If we attach to a process using one of the debuggers below and succeed then
  // the file is a PE file. We set the file type so other plugins can work properly
  // (since they check the file type)
#if ( DEBUGGER_ID == DEBUGGER_ID_X86_IA32_WIN32_USER) || (DEBUGGER_ID == DEBUGGER_ID_ARM_WINCE_USER) || (DEBUGGER_ID == DEBUGGER_ID_WINDBG )
  if ( r == 1 && is_miniidb() && inf.filetype == 0 )
  {
    inf.filetype = f_PE;
  }
#endif
  return r;
}

//--------------------------------------------------------------------------
static int idaapi start_process(
  const char *path,
  const char *args,
  const char *startdir,
  uint32 input_file_crc32)
{
  // check that the host application has been specified
  char p2[QMAXPATH];
  dbg_get_input_path(p2, sizeof(p2));
  if ( is_dll && strcmp(qbasename(path), qbasename(p2)) == 0 )
  {
    warning("AUTOHIDE NONE\n"
            "Please specify the host application first (Debugger, Process options)");
    return 0;
  }
  int flags = is_dll ? DBG_PROC_IS_DLL : 0;
// Win32 stub should tell the server to always create a console window
#if !defined(REMOTE_DEBUGGER) || (DEBUGGER_ID != DEBUGGER_ID_X86_IA32_WIN32_USER)
  if ( callui(ui_get_hwnd).vptr != NULL || is_idaq() )
    flags |= DBG_PROC_IS_GUI;
#endif
  const char *input;
  if ( is_temp_database() )
  {
    input = "";
  }
  else
  {
    // for mini databases the name of the input file won't have the full
    // path. make it full path so that we will use correct path for the input
    // file name.
    if ( is_miniidb() )
    {
      set_root_filename(path);
      input = path;
    }
    else
    {
      input = p2;
    }
  }
  int code;
  while ( true )
  {
    code = s_start_process(path, args, startdir, flags, input, input_file_crc32);
#ifdef REMOTE_DEBUGGER
    if ( code == -2 && debugger.open_file != NULL )
    {
      // if the file is missing on the remote location
      // then propose to copy it
      if ( ask_user_and_copy(p2) )
      {
        dbg_get_input_path(p2, sizeof(p2));
        path = p2;
        startdir = "";
        continue;
      }
    }
#endif
    break;
  }
  return code;
}

//--------------------------------------------------------------------------
static gdecode_t idaapi get_debug_event(debug_event_t *event, bool ida_is_idle)
{
  gdecode_t code = s_get_debug_event(event, ida_is_idle ? TIMEOUT : 0);
  if ( code >= GDE_ONE_EVENT )
  {
    // determine rebasing - we can't do that reliabily remotely, because:
    // - 'is_dll' is not passed to attach_process(), and we can't modify
    //   the protocol without breaking compatibility
    // - 'input_file_path' is undefined if attach_process() but process_get_info()
    //   was not called (if debugger started from the command line with PID)
    switch ( event->eid )
    {
      case PROCESS_ATTACH:
#if DEBUGGER_ID == DEBUGGER_ID_ARM_WINCE_USER
        info("AUTOHIDE REGISTRY\n"
             "Successfully attached to the process.\n"
             "Now you can browse the process memory and set breakpoints.\n");
#endif
        // no break
      case PROCESS_START:
        event->modinfo.rebase_to = is_dll ? BADADDR : event->modinfo.base;
      default:
        break;

      case LIBRARY_LOAD:
        {
          // get input module info
          char full_input_file[QMAXFILE];
          dbg_get_input_path(full_input_file, sizeof(full_input_file));
          char *base_input_file = qbasename(full_input_file);

          // get current module info
          char *full_modname = event->modinfo.name;
          char *base_modname = qbasename(full_modname);

          // we compare basenames and then full path
          // if they have same base names, then we ask the user
          if ( stricmp(base_input_file, base_modname) == 0 )
          {
            // let us compare full path now, if they match, then rebase
            bool rebase = stricmp(full_input_file, full_modname) == 0
                       || !qdirname(NULL, 0, full_input_file); // input file is without full path

            // no match, we may have same names different files
            if ( !rebase )
            {
              static const char dbg_module_conflict[] =
                "TITLE Debugger warning\n"
                "ICON WARNING\n"
                "HIDECANCEL\n"
                "AUTOHIDE DATABASE\n"
                "Debugger found two modules with same base name but different paths\n"
                "This could happen if the program loads the module from a path different than the specified input file\n"
                "\n"
                "Is the loaded file \"%s\" the same as the input file \"%s\"?\n";

              rebase = askbuttons_c("~S~ame", "~N~ot the same", NULL, 1, dbg_module_conflict, full_modname, full_input_file) == 1;
            }
            if ( rebase )
              event->modinfo.rebase_to = event->modinfo.base;
          }
          break;
        }
    }
  }
  return code;
}

//--------------------------------------------------------------------------
static void idaapi stopped_at_debug_event(bool dlls_added)
{
  if ( dlls_added )
    s_stopped_at_debug_event();
}

//--------------------------------------------------------------------------
#ifdef REMOTE_DEBUGGER
static int copy_to_remote(const char *lname, const char *rname)
{
  int code = 0;
  int fn = s_open_file(rname, NULL, false);
  if ( fn != -1 )
  {
    linput_t *li = open_linput(lname, false);
    if ( li != NULL )
    {
      size_t size = qlsize(li);
      if ( size > 0 )
      {
        char *buf = (char *)qalloc(size);
        qlread(li, buf, size);
        if ( s_write_file(fn, 0, buf, size) != ssize_t(size) )
          code = qerrcode();
      }
      close_linput(li);
    }
    else
    {
      code = qerrcode();
    }
    s_close_file(fn);
#if DEBUGGER_ID == DEBUGGER_ID_X86_IA32_LINUX_USER
    // chmod +x
    s_ioctl(0, rname, strlen(rname)+1, NULL, 0);
#endif
  }
  else
  {
    code = qerrcode();
  }
  return code;
}

//--------------------------------------------------------------------------
// ipath==input file path
static bool ask_user_and_copy(const char *ipath)
{
  // check if the input file exists at the current dir of the remote host
  const char *input_file = ipath;
#if DEBUGGER_ID != DEBUGGER_ID_ARM_EPOC_USER
  input_file = qbasename(input_file);
#endif
  int fn = -1;
  // try to open remote file in the current dir if not tried before
  if ( input_file != ipath )
    fn = s_open_file(input_file, NULL, true);
  if ( fn != -1 )
  {
    s_close_file(fn);
    switch ( askbuttons_c("~U~se found",
                          "~C~opy new",
                          "Cancel",
                          1,
                          "IDA could not find the remote file %s.\n"
                          "But it could find remote file %s.\n"
                          "Do you want to use the found file?",
                          ipath, input_file) )
    {
      case 1:
        set_root_filename(input_file);
        return true;
      case -1:
        return false;
    }
    // the user wants to overwrite the old file
  }
  else
  {
    if ( askyn_c(1, "HIDECANCEL\n"
                    "The remote file %s could not be found.\n"
                    "Do you want IDA to copy the executable to the remote computer?",
                    ipath) <= 0 )
      return false;
  }

  // We are to copy the input file to the remote computer's current directory
  const char *lname = ipath;
  // check if the file path is valid on the local system
  if ( !qfileexist(lname) )
  {
    lname = askfile_c(false, lname, "Please select the file to copy");
    if ( lname == NULL )
      return false;
  }
#if DEBUGGER_ID == DEBUGGER_ID_ARM_EPOC_USER
  const char *rname = input_file;
#else
  const char *rname = qbasename(lname);
#endif
  int code = copy_to_remote(lname, rname);
  if ( code != 0 )
  {
#if DEBUGGER_ID == DEBUGGER_ID_ARM_WINCE_USER
    // Windows CE does not have errno and uses GetLastError()
    const char *err = winerr(code);
#else
    const char *err = qerrstr(code);
#endif
    warning("Failed to copy %s -> %s\n%s", lname, rname, err);
  }
  set_root_filename(rname);
  return true;
}

//--------------------------------------------------------------------------
#else  // local debugger
// another copy of this function (for remotel debugging) is defined in rpc_server.cpp
int send_ioctl(
  void *,
  int fn,
  const void *buf,
  size_t size,
  void **poutbuf,
  ssize_t *poutsize)
{
  return g_dbgmod.handle_ioctl(fn, buf, size, poutbuf, poutsize);
}
#endif

//--------------------------------------------------------------------------
static int idaapi process_get_info(int n, process_info_t *info)
{
  char input[QMAXFILE];
  input[0] = '\0';
  if ( n == 0 && !is_temp_database() )
    dbg_get_input_path(input, sizeof(input));
  return s_process_get_info(n, input, info);
}

//--------------------------------------------------------------------------
static bool idaapi init_debugger(const char *hostname, int port_num, const char *password)
{
  if ( !s_open_remote(hostname, port_num, password) )
    return false;

  int code = s_init((debug & IDA_DEBUG_DEBUGGER) != 0);
  if ( code <= 0 )   // (network) error
  {
    s_close_remote();
    return false;
  }
  debugger.process_get_info = (code & 1) ? process_get_info : NULL;
  debugger.detach_process   = (code & 2) ? s_detach_process : NULL;
  debugger_inited = true;
#if DEBUGGER_ID == DEBUGGER_ID_ARM_WINCE_USER
  slot = BADADDR;
  netnode n;
  n.create("$ wince rstub");
  enable_hwbpts(n.altval(0) != 0);
#endif
  register_idc_funcs(true);
  return true;
}

//--------------------------------------------------------------------------
static bool idaapi term_debugger(void)
{
  if ( debugger_inited )
  {
    debugger_inited = false;
    register_idc_funcs(false);
    g_dbgmod.dbg_term();
    return s_close_remote();
  }
  return false;
}

//--------------------------------------------------------------------------
// Initialize debugger plugin
static int idaapi init(void)
{
  if ( init_plugin() )
  {
    dbg = &debugger;
    plugin_inited = true;
    return PLUGIN_KEEP;
  }
  return PLUGIN_SKIP;
}

//--------------------------------------------------------------------------
// Terminate debugger plugin
static void idaapi term(void)
{
  if ( plugin_inited )
  {
    term_plugin();
    plugin_inited = false;
  }
}

//--------------------------------------------------------------------------
// The plugin method - is not used for debugger plugins
static void idaapi run(int arg)
{
#ifdef HAVE_PLUGIN_RUN
  plugin_run(arg);
#else
  qnotused(arg);
#endif
}

//--------------------------------------------------------------------------
//
//      DEBUGGER DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------

#ifdef REMOTE_DEBUGGER
#  ifndef S_OPEN_FILE
#    define S_OPEN_FILE  s_open_file
#  endif
#  ifndef S_CLOSE_FILE
#    define S_CLOSE_FILE s_close_file
#  endif
#  ifndef S_READ_FILE
#    define S_READ_FILE  s_read_file
#  endif
#else
#  define S_OPEN_FILE  NULL
#  define S_CLOSE_FILE NULL
#  define S_READ_FILE  NULL
#endif

#ifndef GET_DEBMOD_EXTS
#  define GET_DEBMOD_EXTS NULL
#endif

#ifndef HAVE_UPDATE_CALL_STACK
#  define UPDATE_CALL_STACK NULL
#else
#  define UPDATE_CALL_STACK s_update_call_stack
#endif

#ifndef HAVE_APPCALL
#  define APPCALL NULL
#  define CLEANUP_APPCALL NULL
#else
#  define APPCALL s_appcall
#  define CLEANUP_APPCALL s_cleanup_appcall
#endif

#ifndef S_MAP_ADDRESS
#  define S_MAP_ADDRESS NULL
#endif

#ifndef SET_DBG_OPTIONS
#  define SET_DBG_OPTIONS NULL
#endif

// wince has no single step mechanism (except Symbian TRK, which provides support for it)
#if TARGET_PROCESSOR == PLFM_ARM && DEBUGGER_ID == DEBUGGER_ID_ARM_WINCE_USER
#  define S_THREAD_SET_STEP NULL
#else
#  define S_THREAD_SET_STEP s_thread_set_step
#endif
debugger_t debugger =
{
  IDD_INTERFACE_VERSION,
  DEBUGGER_NAME,
  DEBUGGER_ID,
  PROCESSOR_NAME,
  DEBUGGER_FLAGS,

  REGISTER_CLASSES,
  REGISTER_CLASSES_DEFAULT,
  REGISTERS,
  REGISTERS_SIZE,

  MEMORY_PAGE_SIZE,

  bpt_code,
  qnumber(bpt_code),

  init_debugger,
  term_debugger,

  NULL, // process_get_info: patched at runtime if ToolHelp functions are available
  start_process,
  _attach_process,
  NULL, // detach_process:   patched at runtime if Windows XP/2K3
  rebase_if_required_to,
  s_prepare_to_pause_process,
  s_exit_process,

  get_debug_event,
  s_continue_after_event,
  s_set_exception_info,
  stopped_at_debug_event,

  s_thread_suspend,
  s_thread_continue,
  S_THREAD_SET_STEP,
  READ_REGISTERS,
  WRITE_REGISTER,
  s_thread_get_sreg_base,

  s_get_memory_info,
  s_read_memory,
  s_write_memory,

  is_ok_bpt,
  add_bpt,
  s_del_bpt,
  S_OPEN_FILE,
  S_CLOSE_FILE,
  S_READ_FILE,
  S_MAP_ADDRESS,
  SET_DBG_OPTIONS,
  GET_DEBMOD_EXTS,
  UPDATE_CALL_STACK,
  APPCALL,
  CLEANUP_APPCALL,
};
//--------------------------------------------------------------------------
//
//      PLUGIN DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------
plugin_t PLUGIN =
{
  IDP_INTERFACE_VERSION,
  PLUGIN_HIDE|PLUGIN_DBG, // plugin flags
  init,                 // initialize

  term,                 // terminate. this pointer may be NULL.

  run,                  // invoke plugin

  comment,              // long comment about the plugin
                        // it could appear in the status line
                        // or as a hint

  help,                 // multiline help about the plugin

  wanted_name,          // the preferred short name of the plugin
#if DEBUGGER_ID == DEBUGGER_ID_ARM_WINCE_USER
  "Ctrl-F1",
#else
  ""                    // the preferred hotkey to run the plugin
#endif
};
