#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(2, "usage: restoreversion file version\n");
    exit(1);
  }

  int version = atoi(argv[2]);

  if(restoreversion(argv[1], version) < 0){
    fprintf(2, "restoreversion failed\n");
    exit(1);
  }

  exit(0);
}

