#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf(2, "usage: restoreversion file version\n");
    exit();
  }

  int version = atoi(argv[2]);

  if(restoreversion(argv[1], version) < 0){
    printf(2, "restoreversion failed\n");
    exit();
  }

  exit();
}
