// veradbg.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>

void usage(void);

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		usage();
		return -1;
	}
	else if (argc >= 2)
	{
		if (strcmp(argv[1], "-attach") == 0)
		{ 
			if (argc == 3)
			{
				printf("Process attach functionality stub %d\n", atoi(argv[2]));
			}
			else
			{
				printf("-pid requirement requires either a PID or process name\n");
			}
		}
		else if (strcmp(argv[1], "-start") == 0 )
		{
			printf("New process start stub\n");
		}
		else if (strcmp(argv[1], "-help") == 0)
		{
			usage();
		}
	}

	return 0;
}


void usage(void)
{
	printf("Usage: veradbg -attach [Program Name or PID to attach to]\n"
		   "               -start [Path to the process to start]\n"
		   "               -help\n");
}