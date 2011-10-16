#ifndef __DEB_PC__
#define __DEB_PC__

#include <ua.hpp>
#include <area.hpp>
#include <idd.hpp>

#if DEBUGGER_ID != DEBUGGER_ID_GDB_USER
  #ifdef __X64__
    #ifdef __VC__
      #include <windows.h>
      typedef DWORD64 cpuregtype_t;
    #else
      typedef uint64 cpuregtype_t;
    #endif
    //#define Eax Rax
    //#define Ebx Rbx
    //#define Ecx Rcx
    //#define Edx Rdx
    //#define Esi Rsi
    //#define Edi Rdi
    //#define Ebp Rbp
    #define Esp Rsp
    #define Eip Rip
  #else
    typedef uint32 cpuregtype_t;
  #endif
#endif

#define MEMORY_PAGE_SIZE 0x1000
#define X86_BPT_CODE         { 0xCC }
#define FPU_REGS_COUNT 8        // number of FPU registers
#define MAX_BPT 4               // maximal number of hardware breakpoints
#define X86_BPT_SIZE 1         // size of 0xCC instruction
#define EFLAGS_TRAP_FLAG 0x00000100

//--------------------------------------------------------------------------
enum register_class_x86_t
{
  X86_RC_GENERAL          = 0x01,
  X86_RC_SEGMENTS         = 0x02,
  X86_RC_FPU              = 0x04, // Floating Point Unit registers
  // RC_MMX              = 0x08
};

//--------------------------------------------------------------------------
enum register_x86_t
{
  // FPU registers
  R_ST0,
  R_ST1,
  R_ST2,
  R_ST3,
  R_ST4,
  R_ST5,
  R_ST6,
  R_ST7,
  R_CTRL,
  R_STAT,
  R_TAGS,
  // segment registers
  R_CS,
  R_DS,
  R_ES,
  R_FS,
  R_GS,
  R_SS,
  // general registers
  R_EAX,
  R_EBX,
  R_ECX,
  R_EDX,
  R_ESI,
  R_EDI,
  R_EBP,
  R_ESP,
  R_EIP,
#ifdef __EA64__
  R64_R8,
  R64_R9,
  R64_R10,
  R64_R11,
  R64_R12,
  R64_R13,
  R64_R14,
  R64_R15,
#endif
  R_EFLAGS,
};

// Number of registers in x86 and x64
#define X86_X64_NREGS 35
#define X86_X86_NREGS 27

#ifdef __EA64__
  #define X86_NREGS X86_X64_NREGS
#else
  #define X86_NREGS X86_X86_NREGS
#endif

#define X86_REG_SP  24
#define X86_REG_IP  25
#define X86_REG_EFL X86_NREGS-1

extern const char *x86_register_classes[];
extern register_info_t x86_registers[X86_NREGS];

int idaapi x86_thread_read_registers(thid_t thread_id, regval_t *values, int count);
int idaapi x86_thread_write_register(thid_t thread_id, int reg_idx, const regval_t *value);
int is_x86_valid_bpt(bpttype_t type, ea_t ea, int len);

#endif
