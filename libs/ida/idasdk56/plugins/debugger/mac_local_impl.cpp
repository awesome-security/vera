#include <loader.hpp>

//;!#ifndef __LINUX__
#ifdef __MAC__
#include <mach-o/loader.h>
#else
// Linux gcc can not compile macho-o headers
// We really want to have remote mac debugger for linux, so we
// define this structure here
struct mach_header {
  uint32 magic;
  uint32 cputype;
  uint32 cpusubtype;
  uint32 filetype;
  uint32 ncmds;
  uint32 sizeofcmds;
  uint32 flags;
};
#define	MH_FVMLIB	0x3
#define	MH_DYLIB	0x6
#define	MH_DYLINKER	0x7
#define	MH_BUNDLE	0x8
#define	MH_DYLIB_STUB	0x9
#endif

#define MACHO_NODE "$ macho"
//--------------------------------------------------------------------------
void idaapi rebase_if_required_to(ea_t new_base)
{
  ea_t base = get_imagebase();
  if ( base != BADADDR && new_base != BADADDR && base != new_base )
  {
    int code = rebase_program(new_base - base, MSF_FIXONCE);
    if ( code != MOVE_SEGM_OK )
    {
      msg("Failed to rebase program, error code %d\n", code);
      warning("IDA Pro failed to rebase the program.\n"
              "Most likely it happened because of the debugger\n"
              "segments created to reflect the real memory state.\n\n"
              "Please stop the debugger and rebase the program manually.\n"
              "For that, please select the whole program and\n"
              "use Edit, Segments, Rebase program with delta 0x%08a",
                                        new_base - base);
    }
  }
}

//--------------------------------------------------------------------------
static bool init_plugin(void)
{
#ifndef DEBUGGER_CLIENT
  if (!init_subsystem())
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

  is_dll = false;
  netnode node(MACHO_NODE);
  mach_header mh;
  if ( node.supval(0, &mh, sizeof(mh)) == sizeof(mh) )
  {
    switch ( mh.filetype )
    {
      case MH_FVMLIB:
      case MH_DYLIB:
      case MH_DYLINKER:
      case MH_BUNDLE:
      case MH_DYLIB_STUB:
        is_dll = true;
        break;
    }
  }

  return true;
}

//--------------------------------------------------------------------------
inline void term_plugin(void)
{
}

//--------------------------------------------------------------------------
char comment[] = "Userland Mac OS X debugger plugin.";
char help[]    = "Userland Mac OS X debugger plugin.";

