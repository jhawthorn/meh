
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "meh.h"

struct image *imagemagick_open(FILE *f){
  int tmpfd[2];
  if(pipe(tmpfd)){
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  int pid;
  if(!(pid = fork())){
    close(tmpfd[0]);
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
    asprintf(&argv[4], "ppm:fd:%i", tmpfd[1]);
    argv[5] = NULL;

#ifdef NDEBUG
    /* STFU OMFG */
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
#endif

    execvp(argv[0], argv);

    perror("exec");
    exit(EXIT_FAILURE);
  }else{
    close(tmpfd[1]);
    FILE *ftmp;
    if(!(ftmp = fdopen(tmpfd[0], "rb"))){
      perror("fopen");
      exit(EXIT_FAILURE);
    }
    struct image *img = netpbm.open(ftmp);
    if(!img)
      return NULL;
    fclose(f);
    return img;
  }
}

struct imageformat imagemagick = {
	imagemagick_open,
	NULL,
	NULL,
	NULL
};

