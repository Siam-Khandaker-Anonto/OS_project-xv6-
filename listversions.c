#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    printf(2, "Usage: listversions filename\n");
    exit();
  }

  listversions(argv[1]);
  exit();
}
