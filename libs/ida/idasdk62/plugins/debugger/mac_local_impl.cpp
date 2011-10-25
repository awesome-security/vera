#include <loader.hpp>

//--------------------------------------------------------------------------
// installs or uninstalls debugger specific idc functions
inline bool register_idc_funcs(bool)
{
  return true;
}

//--------------------------------------------------------------------------
void idaapi rebase_if_required_to(ea_t new_base)
{
  ea_t base = get_imagebase();
  if ( base != BADADDR && new_base != BADADDR && base != new_base )
    rebase_or_warn(base, new_base);
}

//--------------------------------------------------------------------------
static bool init_plugin(void)
{
#ifndef RPC_CLIENT
  if ( !init_subsystem() )
    return false;
#endif

  if ( !netnode::inited() || is_miniidb() || inf.is_snapshot() )
  {
#ifdef __MAC__
    // local debugger is available if we are running under MAC OS X
    return true;
#else
    // for other systems only the remote debugger is available
    return debugger.is_remote();
#endif
  }

  char buf[MAXSTR];
  if ( get_loader_name(buf, sizeof(buf)) <= 0 )
    return false;
  if ( stricmp(buf, "macho") != 0 )     // only Mach-O files
    return false;
  if ( ph.id != TARGET_PROCESSOR )
    return false;

  return true;
}

//--------------------------------------------------------------------------
inline void term_plugin(void)
{
#ifndef RPC_CLIENT
  term_subsystem();
#endif
}

//--------------------------------------------------------------------------
char comment[] = "Userland Mac OS X debugger plugin.";
