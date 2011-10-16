// read Mach-O symbols

#include <pro.h>
#include <fpro.h>
#include <kernwin.hpp>
#include <diskio.hpp>
#include "../../ldr/mach-o/common.cpp"
#include "symmacho.hpp"

#ifdef __X64__
#define MAGIC   MH_MAGIC_64
#define CPUTYPE CPU_TYPE_X86_64
#define HEADER  mach_header_64
#define CMD_SEGMENT LC_SEGMENT_64
#else
#define MAGIC   MH_MAGIC
#define CPUTYPE CPU_TYPE_I386
#define HEADER  mach_header
#define CMD_SEGMENT LC_SEGMENT
#endif

//--------------------------------------------------------------------------
bool read_macho_commands(linput_t *li, uint32 *p_off, bytevec_t &commands, int *ncmds)
{
  HEADER mh;
  *ncmds = 0;
  if ( qlread(li, &mh, sizeof(mh)) != sizeof(mh) )
    return false;

  uint32 off = 0;
  if ( mh.magic == FAT_MAGIC || mh.magic == FAT_CIGAM )
  {
    // locate the I386 or x64 part of the image
    bool mf = mh.magic == FAT_CIGAM;
    qlseek(li, 0, SEEK_SET);
    fat_header fh;
    if ( qlread(li, &fh, sizeof(fh)) != sizeof(fh) )
      return false;
    if ( mf )
      swap_fat_header(&fh);
    for ( int i=0; i < fh.nfat_arch; i++ )
    {
      fat_arch fa;
      if ( qlread(li, &fa, sizeof(fa)) != sizeof(fa) )
        return false;
      if ( mf )
        swap_fat_arch(&fa);
      if ( fa.cputype == CPUTYPE )
      {
        off = fa.offset;
        break;
      }
    }
    if ( off == 0 )
      return false;
    qlseek(li, off, SEEK_SET);
    if ( qlread(li, &mh, sizeof(mh)) != sizeof(mh) )
      return false;
  }
  if ( mh.magic != MAGIC || mh.sizeofcmds <= 0 )
    return false;

  commands.resize(mh.sizeofcmds);
  if ( qlread(li, &commands[0], commands.size()) != commands.size() )
    return false;

  *ncmds = mh.ncmds;
  if ( p_off != NULL )
    *p_off = off;

  return true;
}

//--------------------------------------------------------------------------
inline bool is_zeropage(const segment_command_64 &sg)
{
  return sg.vmaddr == 0 && sg.fileoff == 0 && sg.initprot == 0;
}

//--------------------------------------------------------------------------
inline bool is_text_segment(const segment_command_64 &sg)
{
  if ( is_zeropage(sg) )
    return false;
  const char *name = sg.segname;
  for ( int i=0; i < sizeof(sg.segname); i++, name++ )
    if ( *name != '_' )
      break;
  return strnicmp(name, "TEXT", 4) == 0;
}

//--------------------------------------------------------------------------
inline bool is_linkedit_segment(const segment_command_64 &sg)
{
  return strnicmp(sg.segname, SEG_LINKEDIT, sizeof(SEG_LINKEDIT)-1) == 0;
}

//--------------------------------------------------------------------------
local size_t get_segment_command(segment_command_64 *sg, const uchar *ptr)
{
#ifdef __X64__
  memcpy(sg, ptr, sizeof(*sg));
  if ( mf )
    swap_segment_command_64(sg);
  return sizeof(*sg);
#else
  segment_command tmp;
  memcpy(&tmp, ptr, sizeof(tmp));
  if ( mf )
    swap_segment_command(&tmp);
  *sg = segment_to64(tmp);
  return sizeof(tmp);
#endif
}

//--------------------------------------------------------------------------
local size_t get_section(const uchar *ptr, section_64 *s64)
{
#ifdef __X64__
  memcpy(s64, ptr, sizeof(*s64));
  if ( mf )
    swap_section_64(s64, 1);
  return sizeof(*s64);
#else
  section s;
  memcpy(&s, ptr, sizeof(s));
  if ( mf )
    swap_section(&s, 1);
  *s64 = section_to64(s);
  return sizeof(s);
#endif
}

//--------------------------------------------------------------------------
//parse Mach-O loader commands and fill in requested information
//return expected image base (start of __TEXT segment)
ea_t parse_mach_commands(
        linput_t *li,
        uint32 off,
        const bytevec_t &load_commands,
        int ncmds,
        nlists_t *symbols,
        bytevec_t *strings,
        seg_infos_t* seg_infos,
        bool in_mem)
{
  QASSERT(li != NULL);

  const uchar *begin = &load_commands[0];
  const uchar *end = &load_commands[load_commands.size()];
  const uchar *ptr = begin;
  sval_t expected_base = -1;
  sval_t linkedit_shift = -1;
  if ( seg_infos != NULL )
    seg_infos->qclear();

  for ( int i=0; i < ncmds; i++ )
  {
    load_command lc = *(load_command*)ptr;
    const uchar *lend = ptr + lc.cmdsize;
    if ( lend <= begin || lend > end )
      break;

    if ( lc.cmd == CMD_SEGMENT )
    {
      struct segment_command_64 sg;
      size_t cmdsize = get_segment_command(&sg, ptr);
      if ( is_text_segment(sg) && expected_base == -1 )
        expected_base = sg.vmaddr;
      if ( is_linkedit_segment(sg) && linkedit_shift == -1 )
        linkedit_shift = in_mem ? sg.vmaddr - expected_base - sg.fileoff : 0;

      if ( seg_infos != NULL )
      {
        if ( sg.nsects == 0 )
        {
          seg_info_t &si = seg_infos->push_back();
          si.name  = sg.segname;
          si.size  = sg.vmsize;
          si.start = sg.vmaddr;
        }
        else
        {
          ptr += cmdsize;
          for ( int i=0; i < sg.nsects && ptr < lend; i++ )
          {
            section_64 s64;
            ptr += get_section(ptr, &s64);
            seg_info_t &si = seg_infos->push_back();
            si.name  = s64.sectname;
            si.size  = s64.size;
            si.start = s64.addr;
          }
        }
      }
    }
    else if ( lc.cmd == LC_SYMTAB )
    {
      symtab_command &st = *(symtab_command*)ptr;
      if ( st.nsyms > 0 && symbols != NULL )
      {
        size_t nbytes = st.nsyms*sizeof(struct nlist_64);
        symbols->resize(st.nsyms);
        memset(symbols->begin(), 0, nbytes);
        qlseek(li, off + linkedit_shift + st.symoff, SEEK_SET);
        // we do not check the error code, if fails, we will have zeroes
#ifdef __X64__
        qlread(li, symbols->begin(), nbytes);
#else
        qvector<struct nlist> syms32;
        syms32.resize(st.nsyms);
        memset(syms32.begin(), 0, st.nsyms*sizeof(struct nlist));
        nlist_to64(syms32.begin(), symbols->begin(), st.nsyms, false);
#endif
      }
      if ( st.strsize > 0 && strings != NULL)
      {
        strings->resize(st.strsize, 0);
        qlseek(li, off + linkedit_shift + st.stroff, SEEK_SET);
        // we do not check the error code, if fails, we will have zeroes
        qlread(li, strings->begin(), st.strsize);
      }
      return expected_base == -1 ? 0 : expected_base;
    }
    ptr = lend;
  }
  return BADADDR;
}

//--------------------------------------------------------------------------
linput_t *create_mem_input(ea_t start, read_memory_t reader)
{
  struct meminput : generic_linput_t
  {
    ea_t start;
    read_memory_t reader;
    meminput(ea_t _start, read_memory_t _reader) : start(_start), reader(_reader)
    {
      filesize = 0;
      blocksize = 0;
    }
    virtual ssize_t idaapi read(off_t off, void *buffer, size_t nbytes)
    {
      return reader(start+off, buffer, nbytes);
    }
  };
  meminput* pmi = new meminput(start, reader);
  return create_generic_linput(pmi);
}

//--------------------------------------------------------------------------
//parse a mach-o file image in memory and enumerate its segments and symbols
bool parse_macho(ea_t start, linput_t *li, symbol_visitor_t &sv, bool in_mem)
{
  uint32 off;
  int ncmds;
  bytevec_t commands;
  if ( !read_macho_commands(li, &off, commands, &ncmds) )
    return false;

  seg_infos_t seg_infos;
  nlists_t symbols;
  bytevec_t strings;
  ea_t expected_base = parse_mach_commands(li, off, commands, ncmds, &symbols, &strings, &seg_infos, in_mem);
  if ( expected_base == BADADDR )
    return false;

  sval_t slide = start - expected_base;

  if ( (sv.velf & VISIT_SEGMENTS) != 0 )
  {
    for ( size_t i=0; i < seg_infos.size(); i++ )
    {
      const seg_info_t &si = seg_infos[i];
      sv.visit_segment(si.start + slide, si.size, si.name.c_str());
    }
  }

  if ( (sv.velf & VISIT_SYMBOLS) != 0 )
  {
    for ( size_t i=0; i < symbols.size(); i++ )
    {
      const struct nlist_64 &nl = symbols[i];
      if ( nl.n_un.n_strx > strings.size() )
        continue;
      const char *name = (char *)strings.begin() + nl.n_un.n_strx;

      ea_t ea;
      int type = nl.n_type & N_TYPE;
      switch ( type )
      {
        case N_UNDF:
        case N_PBUD:
        case N_ABS:
          break;
        case N_SECT:
        case N_INDR:
          ea = nl.n_value + slide;
          if ( name[0] != '\0' )
          {
            if ( (nl.n_type & (N_EXT|N_PEXT)) == N_EXT ) // exported
            {
              sv.visit_symbol(ea, name);
            }
            else if ( type == N_SECT && nl.n_sect != NO_SECT ) // private symbols
            {
              sv.visit_symbol(ea, name);
            }
          }
          break;
      }
    }
  }
  return true;
}

//--------------------------------------------------------------------------
//parse a mach-o file image in memory and enumerate its segments and symbols
bool parse_macho_mem(ea_t start, read_memory_t reader, symbol_visitor_t &sv)
{
  linput_t *li = create_mem_input(start, reader);
  if ( li == NULL )
    return false;

  bool ok = parse_macho(start, li, sv, true);

  close_linput(li);
  return ok;
}

//--------------------------------------------------------------------------
asize_t calc_macho_image_size(linput_t *li, ea_t *p_base)
{
  if ( li == NULL )
    return 0;
  if ( p_base != NULL )
    *p_base = BADADDR;

  asize_t size = 0;
  uint32 off;
  bytevec_t commands;
  int ncmds;
  if ( read_macho_commands(li, &off, commands, &ncmds) )
  {
    const uchar *begin = &commands[0];
    const uchar *end = &commands[commands.size()];
    const uchar *ptr = begin;
    ea_t base = BADADDR;
    ea_t maxea = 0;
    for ( int i=0; i < ncmds; i++ )
    {
      load_command lc = *(load_command*)ptr;
      const uchar *lend = ptr + lc.cmdsize;
      if ( lend <= begin || lend > end )
        break;

      if ( lc.cmd == CMD_SEGMENT )
      {
        segment_command_64 sg;
        get_segment_command(&sg, ptr);
        // since mac os x scatters application segments over the memory
        // we calculate only the text segment size
        if ( is_text_segment(sg) )
        {
          if ( base == BADADDR )
            base = sg.vmaddr;
          ea_t end = sg.vmaddr + sg.vmsize;
          if ( maxea < end )
            maxea = end;
//          msg("segment %s base %a size %d maxea %a\n", sg.segname, sg.vmaddr, sg.vmsize, maxea);
        }
      }
      ptr = lend;
    }
    size = maxea - base;
    if ( p_base != NULL )
      *p_base = base;
//    msg("%s: base %a size %d\n", fname, base, size);
  }
  return size;
}

//--------------------------------------------------------------------------
bool is_dylib_header(ea_t base, read_memory_t read_mem, char *filename, size_t namesize)
{
  HEADER mh;
  if ( read_mem(base, &mh, sizeof(mh)) != sizeof(mh) )
    return false;

  if ( mh.magic != MAGIC || mh.filetype != MH_DYLINKER )
    return false;

  // seems to be dylib
  // find its file name
  filename[0] = '\0';
  ea_t ea = base + sizeof(mh);
  for ( int i=0; i < mh.ncmds; i++ )
  {
    struct load_command lc;
    lc.cmd = 0;
    read_mem(ea, &lc, sizeof(lc));
    if ( lc.cmd == LC_ID_DYLIB )
    {
      struct dylib_command dcmd;
      read_mem(ea, &dcmd, sizeof(dcmd));
      read_mem(ea+dcmd.dylib.name.offset, filename, namesize);
      break;
    }
    else if ( lc.cmd == LC_ID_DYLINKER )
    {
      struct dylinker_command dcmd;
      read_mem(ea, &dcmd, sizeof(dcmd));
      read_mem(ea+dcmd.name.offset, filename, namesize);
      break;
    }
    ea += lc.cmdsize;
  }
  return true;
}
