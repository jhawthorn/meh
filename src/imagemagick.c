
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "meh.h"

struct image *imagemagick_open(FILE *f){
  char template[] = "/tmp/fileXXXXXX";
  char *ntmp = mktemp(template);
  if(!ntmp){
    perror("mktemp");
    exit(EXIT_FAILURE);
  }

  int pid;
  if(!(pid = fork())){
    int origfd = fileno(f);
    if(lseek(origfd, 0, SEEK_SET) != 0){
      perror("lseek");
      exit(EXIT_FAILURE);
    }

    char *argv[6];

    argv[0] = "convert";
    argv[1] = "-depth";
    argv[2] = "255";
    asprintf(&argv[3], "fd:%i", origfd);
    asprintf(&argv[4], "ppm:%s", ntmp);
    argv[5] = NULL;
    execvp(argv[0], argv);
    perror("exec");
    exit(EXIT_FAILURE);
  }else{
    int status;
    waitpid(pid, &status, 0);
    if(status){
      return NULL;
    }
    fclose(f);
    FILE *ftmp;
    if(!(ftmp = fopen(ntmp, "rb"))){
      perror("fopen");
      exit(EXIT_FAILURE);
    }
    return netpbm.open(ftmp);
  }
}

struct imageformat imagemagick = {
	imagemagick_open,
	NULL,
	NULL,
	NULL
};

