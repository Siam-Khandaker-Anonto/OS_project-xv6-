#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"


static void
check(int condition, const char *msg)
{
  if(condition){
    printf(1, "  PASS: %s\n", msg);
  } else {
    printf(1, "  FAIL: %s\n", msg);
    exit();
  }
}

static void
test_basic(void)
{
  int fd;
  char buf[32];

  printf(1, "\n[Test 1] Basic symlink\n");

  fd = open("realfile", O_CREATE | O_WRONLY);
  check(fd >= 0, "create realfile");
  write(fd, "hello", 5);
  close(fd);

  check(symlink("realfile", "mylink") == 0, "symlink() returns 0");

  fd = open("mylink", O_RDONLY);
  check(fd >= 0, "open via symlink");

  int n = read(fd, buf, sizeof(buf));
  buf[n] = '\0';
  check(strcmp(buf, "hello") == 0, "content matches real file");
  close(fd);

  unlink("realfile");
  unlink("mylink");
}

static void
test_nofollow(void)
{
  int fd;
  struct stat st;

  printf(1, "\n[Test 2] O_NOFOLLOW flag\n");

  fd = open("target", O_CREATE | O_WRONLY);
  check(fd >= 0, "create target");
  close(fd);

  check(symlink("target", "link2") == 0, "create link2 -> target");

  fd = open("link2", O_RDONLY | O_NOFOLLOW);
  check(fd >= 0, "open with O_NOFOLLOW succeeds");

  fstat(fd, &st);
  check(st.type == T_SYMLINK, "fstat reports T_SYMLINK");
  close(fd);

  unlink("target");
  unlink("link2");
}

static void
test_chain(void)
{
  int fd;
  char buf[32];

  printf(1, "\n[Test 3] Chained symlinks\n");

  fd = open("chainfile", O_CREATE | O_WRONLY);
  check(fd >= 0, "create chainfile");
  write(fd, "chained", 7);
  close(fd);

  check(symlink("chainfile", "linkC") == 0, "linkC -> chainfile");
  check(symlink("linkC",     "linkB") == 0, "linkB -> linkC");
  check(symlink("linkB",     "linkA") == 0, "linkA -> linkB");

  fd = open("linkA", O_RDONLY);
  check(fd >= 0, "open linkA follows full chain");

  int n = read(fd, buf, sizeof(buf));
  buf[n] = '\0';
  check(strcmp(buf, "chained") == 0, "content correct after chain");
  close(fd);

  unlink("chainfile");
  unlink("linkC");
  unlink("linkB");
  unlink("linkA");
}

static void
test_cycle(void)
{
  int fd;

  printf(1, "\n[Test 4] Circular symlinks\n");

  check(symlink("cylinkB", "cylinkA") == 0, "create cylinkA -> cylinkB");
  check(symlink("cylinkA", "cylinkB") == 0, "create cylinkB -> cylinkA");

  fd = open("cylinkA", O_RDONLY);
  check(fd < 0, "open on cycle returns -1");

  unlink("cylinkA");
  unlink("cylinkB");
}

static void
test_dangling(void)
{
  int fd;

  printf(1, "\n[Test 5] Dangling symlink\n");

  check(symlink("ghost", "dangling") == 0, "create dangling symlink");

  fd = open("dangling", O_RDONLY);
  check(fd < 0, "opening dangling symlink fails");

  unlink("dangling");
}

int
main(void)
{
  printf(1, "=== symlinktest start ===\n");

  test_basic();
  test_nofollow();
  test_chain();
  test_cycle();
  test_dangling();

  printf(1, "\n=== symlinktest: ALL TESTS PASSED ===\n");
  exit();
}
