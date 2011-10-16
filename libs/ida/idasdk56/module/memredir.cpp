
//      This file redirects all memory requests to the kernel
//      for Borland CBuilder!

#pragma option push -V?-

// bcb6 bug support
#pragma alias "___org_malloc" = "_malloc"
#pragma alias "___org_free" = "_free"

// Redirect all memory requests to the kernel
namespace std { extern "C" {
void *   _RTLENTRY _EXPFUNC calloc(_SIZE_T __nitems, _SIZE_T __size) { return qcalloc(__nitems, __size); }
void     _RTLENTRY _EXPFUNC free(void * __block)                     { qfree(__block); }
void *   _RTLENTRY _EXPFUNC malloc(_SIZE_T  __size)                  { return qalloc(__size); }
void *   _RTLENTRY _EXPFUNC realloc(void * __block, _SIZE_T __size)  { return qrealloc(__block, __size); }

struct heap_redirector_t
{
  int version;
  int d1;
  int d2;
  void  _RTLENTRY (*free)(void *);
  void *_RTLENTRY (*malloc)(size_t);
  void *_RTLENTRY (*realloc)(void *, size_t);
  void  _RTLENTRY (*free_heaps)(void);
};

extern heap_redirector_t _heap_redirector;

static void _RTLENTRY free_heaps(void) {}

// Internal borland function which is called at the first heap management call
//
int _RTLENTRY _EXPFUNC __CRTL_MEM_CheckBorMem(void)
{
  if ( _heap_redirector.version != 28 )
    error("Wrong heap redirector version!");
  _heap_redirector.free       = free;
  _heap_redirector.malloc     = malloc;
  _heap_redirector.realloc    = realloc;
  _heap_redirector.free_heaps = free_heaps;
  return 1;
}

}}

#pragma option pop

