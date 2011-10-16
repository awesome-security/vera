
// IOCTL codes for the win32 debugger

#define WIN32_IOCTL_RDMSR 0 // read model specific register
#define WIN32_IOCTL_WRMSR 1 // write model specific register

// WIN32_IOCTL_WRMSR uses this structure:
struct win32_wrmsr_t
{
  uint32 reg;
  uint64 value;
};

