// test console application to call in stream mode
//

#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

#define ASYNC_TEST
#include "../async.cpp"
//--------------------------------------------------------------------------
char *idaapi winerr(int code)
{
  static char buf[1024];
  wchar_t wbuf[1024];
  if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                     NULL,
                     code,
                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     wbuf,
                     1024,
                     NULL) )
  {
    WideCharToMultiByte(CP_ACP, 0, wbuf, -1, buf, sizeof(buf), NULL, NULL);
  }
  else
  {
    _snprintf(buf, sizeof(buf), "Unknown error, code = %d", code);
  }
  return buf;
}

//--------------------------------------------------------------------------
int main(int /*argc*/, char* /*argv*/[])
{
  idarpc_stream_t *irs = init_client_irs(NULL, 0);
  if ( irs == NULL )
  {
FATAL:
    printf("Error: %s\n", winerr(GetLastError()));
    return 1;
  }
  printf("READY\n");
  while( true )
  {
    char c = getch();
    if ( c == 0x1B )
      break;
    putchar(c);
    rpc_packet_t rp;
    rp.length = 0;
    rp.code = c;
    if ( irs_send(irs, &rp, sizeof(rp)) != sizeof(rp) )
    {
      printf("irs_send: %s\n", winerr(irs_error(irs)));
      break;
    }
    memset(&rp, 0, sizeof(rp));
    if ( irs_recv(irs, &rp, sizeof(rp)) != sizeof(rp) )
    {
      printf("irs_recv: %s\n", winerr(irs_error(irs)));
      break;
    }
    putchar(rp.code);
  }
  putchar('\n');
  term_client_irs(irs);
  return 0;
}
