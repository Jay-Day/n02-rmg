/******************************************************************************
***  n02 v0.3 winnt                                                         ***
**   Open Kaillera Client Core                                               **
***  For latest sources, visit http://sf.net/projects/okai                  ***
******************************************************************************/
#pragma once

class nSettings {
public:
	static void Initialize(char * submo = "p2p", bool global = false);
	static void Terminate();
	static int get_int(char * key, int def_ = -1);
	static char* get_str(char * key, char * buf, char * def_=0);
	static void set_int(char * key, int val);
	static void set_str(char * key, char * val);
};
