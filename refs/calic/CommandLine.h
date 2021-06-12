//////////////////////////////////////////////////////////////////////
//
// File: cmdline.h
// 
// Description:
//   Contains class CommandLine.
//
//////////////////////////////////////////////////////////////////////

// PROGRAMMER_INFO

#ifndef __CMDLINE_H__
#define __CMDLINE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_STR_LEN
#define MAX_STR_LEN 128
#endif  

class CommandLine {
public:
  int argc;
  char **argv;

  CommandLine(): argc(0), argv(NULL) {}
  CommandLine(int _argc, char **_argv) {
    setArguements(_argc, _argv);
  }
  ~CommandLine();

  void setArguements(int, char **);
  bool getOption(const char *);
  bool getParameter(const char *, char *);
  bool getParameter(const char *, int *);
  bool getParameter(const char *, float *);
  bool getParameter(const char *, double *);
};

void printUsage(const char *usage[]);

#endif //__CMDLINE_H__

