//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

static struct inode* create(char *path, short type, short major, short minor);

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}


int
sys_symlink(void)
{
  char *target, *path;
  struct inode *ip;

  if(argstr(0, &target) < 0 || argstr(1, &path) < 0)
    return -1;

  begin_op();
  ip = create(path, T_SYMLINK, 0, 0);
  if(ip == 0){
    end_op();
    return -1;
  }
  if(writei(ip, target, 0, strlen(target)) != strlen(target)){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}


// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

static int
unlink_path(char *path)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ];
  uint off;

  if((dp = nameiparent(path, name)) == 0)
    return -1;

  ilock(dp);

  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink_path: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink_path: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  return 0;

bad:
  iunlockput(dp);
  return -1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
    // Follow symlinks unless O_NOFOLLOW is set
    if(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)){
      int depth = 0;
      char target[MAXPATH];
      int n;
      while(ip->type == T_SYMLINK){
        if(depth++ >= 10){
          iunlockput(ip);
          end_op();
          return -1;
        }
        n = readi(ip, target, 0, MAXPATH - 1);
        if(n <= 0){
          iunlockput(ip);
          end_op();
          return -1;
        }
        target[n] = '\0';
        iunlockput(ip);
        if((ip = namei(target)) == 0){
          end_op();
          return -1;
        }
        ilock(ip);
      }
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if((omode & O_TRUNC) && ip->type == T_FILE){
    char snapname[MAXPATH];
    char oldname[MAXPATH];
    char digits[16];
    char dname[DIRSIZ];
    struct inode *ddp;
    struct dirent de;
    uint doff;
    int namelen, snap_count, min_ver, max_ver, i, j;

    // Get parent directory and base name of the file
    if((ddp = nameiparent(path, dname)) == 0)
      goto skipsnap;

    namelen = strlen(dname);

    // Scan directory: count snapshots, find min and max version numbers
    ilock(ddp);
    snap_count = 0;
    min_ver = 999999;
    max_ver = -1;
    for(doff = 0; doff < ddp->size; doff += sizeof(de)){
      if(readi(ddp, (char*)&de, doff, sizeof(de)) != sizeof(de))
        break;
      if(de.inum == 0)
        continue;
      if(strncmp(de.name, dname, namelen) == 0 &&
         de.name[namelen] == '.' &&
         de.name[namelen+1] == 'v' &&
         de.name[namelen+2] >= '0' &&
         de.name[namelen+2] <= '9') {
        // Parse version number
        int ver = 0;
        int k = namelen + 2;
        while(de.name[k] >= '0' && de.name[k] <= '9'){
          ver = ver * 10 + (de.name[k] - '0');
          k++;
        }
        snap_count++;
        if(ver < min_ver)
          min_ver = ver;
        if(ver > max_ver)
          max_ver = ver;
      }
    }
    iunlockput(ddp);

    // If at capacity, delete the oldest snapshot (min version)
    if(snap_count >= 3){
      i = 0;
      j = 0;
      while(path[j] && j < MAXPATH - 16){
        oldname[j] = path[j];
        j++;
      }
      memmove(oldname + j, ".v", 2);
      j += 2;
      if(min_ver == 0){
        oldname[j++] = '0';
      } else {
        int tmp = min_ver;
        while(tmp > 0){
          digits[i++] = '0' + tmp % 10;
          tmp /= 10;
        }
        while(i > 0)
          oldname[j++] = digits[--i];
      }
      oldname[j] = 0;
      unlink_path(oldname);
    }

    // New version = max + 1 (or 0 if no snapshots exist yet)
    {
      int newver = (max_ver >= 0) ? max_ver + 1 : 0;
      i = 0;
      j = 0;
      while(path[j] && j < MAXPATH - 16){
        snapname[j] = path[j];
        j++;
      }
      memmove(snapname + j, ".v", 2);
      j += 2;
      if(newver == 0){
        snapname[j++] = '0';
      } else {
        int tmp = newver;
        while(tmp > 0){
          digits[i++] = '0' + tmp % 10;
          tmp /= 10;
        }
        while(i > 0)
          snapname[j++] = digits[--i];
      }
      snapname[j] = 0;
    }

    struct inode *snap = create(snapname, T_FILE, 0, 0);
    if(snap){
      char buf[BSIZE];
      uint off = 0;
      int nread;
      while(off < ip->size){
        int ncopy = ip->size - off;
        if(ncopy > BSIZE)
          ncopy = BSIZE;
        nread = readi(ip, buf, off, ncopy);
        if(nread <= 0 || writei(snap, buf, off, nread) != nread)
          break;
        off += nread;
      }
      iunlockput(snap);
    }
skipsnap:
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_restoreversion(void)
{
  char *path;
  char snapname[MAXPATH], digits[16], buf[BSIZE];
  struct inode *ip, *snap;
  int version, i = 0, j = 0, nread, ret = 0;
  uint off = 0;

  if(argstr(0, &path) < 0)
    return -1;
  if(argint(1, &version) < 0)
    return -1;
  if(version < 0)
    return -1;

  while(path[j] && j < MAXPATH - 16){
    snapname[j] = path[j];
    j++;
  }
  snapname[j++] = '.';
  snapname[j++] = 'v';
  if(version == 0){
    snapname[j++] = '0';
  } else {
    while(version > 0){
      digits[i++] = '0' + (version % 10);
      version /= 10;
    }
    while(i > 0)
      snapname[j++] = digits[--i];
  }
  snapname[j] = '\0';

  begin_op();
  if((snap = namei(snapname)) == 0){
    end_op();
    return -1;
  }
  if((ip = namei(path)) == 0){
    iput(snap);
    end_op();
    return -1;
  }

  ilock(snap);
  ilock(ip);
  if(snap->type != T_FILE || ip->type != T_FILE){
    ret = -1;
  } else {
    itrunc(ip);
    while(off < snap->size){
      int ncopy = snap->size - off;
      if(ncopy > BSIZE)
        ncopy = BSIZE;
      nread = readi(snap, buf, off, ncopy);
      if(nread <= 0 || writei(ip, buf, off, nread) != nread){
        ret = -1;
        break;
      }
      off += nread;
    }
  }
  iunlockput(ip);
  iunlockput(snap);
  end_op();

  return ret;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int
sys_listversions(void)
{
  char *path;
  char name[DIRSIZ];
  struct inode *dp;
  struct dirent de;
  uint off;
  int namelen;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  namelen = strlen(name);

  ilock(dp);
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("listversions: readi");
    if(de.inum == 0)
      continue;
    if(strncmp(de.name, name, namelen) == 0 &&
       de.name[namelen] == '.' &&
       de.name[namelen+1] == 'v' &&
       de.name[namelen+2] >= '0' &&
       de.name[namelen+2] <= '9') {
      cprintf("%s\n", de.name);
    }
  }
  iunlockput(dp);
  end_op();
  return 0;
}
