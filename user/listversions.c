#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "Usage: listversions filename\n");
    exit(1);
  }

  listversions(argv[1]);
  exit(0);
}
