/* syscall.c - proxy system call handler routines
 *
 * SimpleScalar Ô Tool Suite
 * © 1994-2003 Todd M. Austin, Ph.D. and SimpleScalar, LLC
 * All Rights Reserved.
 * 
 * THIS IS A LEGAL DOCUMENT BY DOWNLOADING SIMPLESCALAR, YOU ARE AGREEING TO
 * THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted as
 * described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express or
 * implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged.  SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship purposes
 * provided that this notice in its entirety accompanies all copies. Copies of
 * the modified software can be delivered to persons who use it solely for
 * nonprofit, educational, noncommercial research, and noncommercial
 * scholarship purposes provided that this notice in its entirety accompanies
 * all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a copy
 * of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright © 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * Copyright © 2009 by Gabriel H. Loh and the Georgia Tech Research Corporation
 * Atlanta, GA  30332-0415
 * All Rights Reserved.
 * 
 * THIS IS A LEGAL DOCUMENT BY DOWNLOADING ZESTO, YOU ARE AGREEING TO THESE
 * TERMS AND CONDITIONS.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * NOTE: Portions of this release are directly derived from the SimpleScalar
 * Toolset (property of SimpleScalar LLC), and as such, those portions are
 * bound by the corresponding legal terms and conditions.  All source files
 * derived directly or in part from the SimpleScalar Toolset bear the original
 * user agreement.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Georgia Tech Research Corporation nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * 4. Zesto is distributed freely for commercial and non-commercial use.  Note,
 * however, that the portions derived from the SimpleScalar Toolset are bound
 * by the terms and agreements set forth by SimpleScalar, LLC.  In particular:
 * 
 *   "Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 *   downloaded, compiled, executed, copied, and modified solely for nonprofit,
 *   educational, noncommercial research, and noncommercial scholarship
 *   purposes provided that this notice in its entirety accompanies all copies.
 *   Copies of the modified software can be delivered to persons who use it
 *   solely for nonprofit, educational, noncommercial research, and
 *   noncommercial scholarship purposes provided that this notice in its
 *   entirety accompanies all copies."
 * 
 * User is responsible for reading and adhering to the terms set forth by
 * SimpleScalar, LLC where appropriate.
 * 
 * 5. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 6. Noncommercial and nonprofit users may distribute copies of Zesto in
 * compiled or executable form as set forth in Section 2, provided that either:
 * (A) it is accompanied by the corresponding machine-readable source code, or
 * (B) it is accompanied by a written offer, with no time limit, to give anyone
 * a machine-readable copy of the corresponding source code in return for
 * reimbursement of the cost of distribution. This written offer must permit
 * verbatim duplication by anyone, or (C) it is distributed by someone who
 * received only the executable form, and is accompanied by a copy of the
 * written offer of source code.
 * 
 * 7. Zesto was developed by Gabriel H. Loh, Ph.D.  US Mail: 266 Ferst Drive,
 * Georgia Institute of Technology, Atlanta, GA 30332-0765
 */

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/mman.h>
#include <syscall.h>

#include "thread.h"

  /* only enable a minimal set of systen call proxies if on limited
     hosts or if in cross endian live execution mode */
#ifndef MIN_SYSCALL_MODE
#if defined(_MSC_VER) || defined(__CYGWIN32__) || defined(MD_CROSS_ENDIAN)
#define MIN_SYSCALL_MODE
#endif
#endif /* !MIN_SYSCALL_MODE */

  /* live execution only support on same-endian hosts... */
#ifdef _MSC_VER
#include <io.h>
#else /* !_MSC_VER */
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#ifndef _MSC_VER
#include <sys/param.h>
#endif
#include <errno.h>
#include <time.h>
#ifndef _MSC_VER
#include <sys/time.h>
#endif
#ifndef _MSC_VER
#include <sys/resource.h>
#endif
#include <signal.h>
#ifndef _MSC_VER
#include <sys/file.h>
#endif
#include <sys/stat.h>
#ifndef _MSC_VER
#include <sys/uio.h>
#endif
#include <setjmp.h>
#ifndef _MSC_VER
#include <sys/times.h>
#endif
#include <limits.h>
#ifndef _MSC_VER
#include <sys/ioctl.h>
#include <termios.h>
#include <rpcsvc/rex.h>
#endif
#if defined(linux)
#include <utime.h>
#include <dirent.h>
#include <sys/vfs.h>
#endif
#if defined(_AIX)
#include <sys/statfs.h>
#else /* !_AIX */
#ifndef _MSC_VER
#include <sys/mount.h>
#endif
#endif /* !_AIX */
#if !defined(linux) && !defined(sparc) && !defined(hpux) && !defined(__hpux) && !defined(__CYGWIN32__) && !defined(ultrix)
#ifndef _MSC_VER
#include <sys/select.h>
#endif
#endif
#ifdef linux
#include <sgtty.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/in.h>

#include <linux/unistd.h>
#include <asm/unistd.h>
#include <sys/types.h>

#include <sys/utsname.h>

/* Gabe: for some reason gcc isn't getting the definition of struct user_desc. */
#ifdef LINUX_RHEL4
 struct user_desc {
  unsigned int  entry_number;
  unsigned long base_addr;
  unsigned int  limit;
  unsigned int  seg_32bit:1;
  unsigned int  contents:2;
  unsigned int  read_exec_only:1;
  unsigned int  limit_in_pages:1;
  unsigned int  seg_not_present:1;
  unsigned int  useable:1;
};
#endif
#include <asm/ldt.h>


#endif /* linux */

#if defined(__svr4__)
#include <sys/dirent.h>
#include <sys/filio.h>
#elif defined(__osf__)
#include <dirent.h>
  /* -- For some weird reason, getdirentries() is not declared in any
   * -- header file under /usr/include on the Alpha boxen that I tried
   * -- SS-Alpha on. But the function exists in the libraries.
   */
  int getdirentries(int fd, char *buf, int nbytes, long *basep);
#endif

#if defined(__svr4__) || defined(__osf__)
#include <sys/statvfs.h>
#define statfs statvfs
#include <sys/time.h>
#include <utime.h>
#include <sgtty.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#if defined(sparc) && defined(__unix__)
#if defined(__svr4__) || defined(__USLC__)
#include <dirent.h>
#else
#include <sys/dir.h>
#endif




  /* dorks */
#undef NL0
#undef NL1
#undef CR0
#undef CR1
#undef CR2
#undef CR3
#undef TAB0
#undef TAB1
#undef TAB2
#undef XTABS
#undef BS0
#undef BS1
#undef FF0
#undef FF1
#undef ECHO
#undef NOFLSH
#undef TOSTOP
#undef FLUSHO
#undef PENDIN
#endif

#if defined(hpux) || defined(__hpux)
#undef CR0
#endif

#ifdef __FreeBSD__
#include <sys/ioctl_compat.h>
#else
#ifndef _MSC_VER
#include <termio.h>
#endif
#endif

#if defined(hpux) || defined(__hpux)
  /* et tu, dorks! */
#undef HUPCL
#undef ECHO
#undef B50
#undef B75
#undef B110
#undef B134
#undef B150
#undef B200
#undef B300
#undef B600
#undef B1200
#undef B1800
#undef B2400
#undef B4800
#undef B9600
#undef B19200
#undef B38400
#undef NL0
#undef NL1
#undef CR0
#undef CR1
#undef CR2
#undef CR3
#undef TAB0
#undef TAB1
#undef BS0
#undef BS1
#undef FF0
#undef FF1
#undef EXTA
#undef EXTB
#undef B900
#undef B3600
#undef B7200
#undef XTABS
#include <sgtty.h>
#include <utime.h>
#endif

#ifdef __CYGWIN32__
#include <sys/unistd.h>
#include <sys/vfs.h>
#endif

#include <sys/socket.h>
#include <sys/poll.h>

#ifdef _MSC_VER
#define access		_access
#define chmod		_chmod
#define chdir		_chdir
#define unlink		_unlink
#define open		_open
#define creat		_creat
#define pipe		_pipe
#define dup		_dup
#define dup2		_dup2
#define stat		_stat
#define fstat		_fstat
#define lseek		_lseek
#define read		_read
#define write		_write
#define close		_close
#define getpid		_getpid
#define utime		_utime
#include <sys/utime.h>
#endif /* _MSC_VER */

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "sim.h"
#include "endian.h"
#include "eio.h"
#include "syscall.h"
  /* #include "syscall_names.h" */
#include <linux/unistd.h>

  /* Syscall numbers
     Got these numbers from /kernel/arch/arm/kernel/calls.S 
     Need to sync with kernel/include/asm-arm/unistd.h (Done!)
     Some system calls seem to be out of kernel V2.4 */

#define X86_SYS_ni_syscall	0
#define X86_SYS_exit		1
#define X86_SYS_fork		2
#define X86_SYS_read		3
#define X86_SYS_write		4
#define X86_SYS_open		5
#define X86_SYS_close		6
#define X86_SYS_waitpid		7
#define X86_SYS_creat		8
#define X86_SYS_link		9
#define X86_SYS_unlink		10
#define X86_SYS_execve		11
#define X86_SYS_chdir		12
#define X86_SYS_time		13
#define X86_SYS_mknod		14
#define X86_SYS_chmod		15
#define X86_SYS_lchown16	16
  /* 17 and 18 not defined???  in Kernel v2.4 */
#define X86_SYS_break		17

  /*#define X86_SYS_oldstat	18*/

#define X86_SYS_lseek		19
#define X86_SYS_getpid		20
#define X86_SYS_mount		21
#define X86_SYS_umount		22
#define X86_SYS_setuid		23
#define X86_SYS_getuid		24
#define X86_SYS_stime		25
#define X86_SYS_ptrace		26
#define X86_SYS_alarm		27

#define X86_SYS_pause		29
#define X86_SYS_utime		30
#define X86_SYS_stty		31
#define X86_SYS_gtty		32
#define X86_SYS_access		33
#define X86_SYS_nice		34
#define X86_SYS_ftime		35
#define X86_SYS_sync		36
#define X86_SYS_kill		37
#define X86_SYS_rename		38
#define X86_SYS_mkdir		39
#define X86_SYS_rmdir		40
#define X86_SYS_dup		41
#define X86_SYS_pipe		42
#define X86_SYS_times		43
#define X86_SYS_prof		44
#define X86_SYS_brk		45
#define X86_SYS_setgid		46
#define X86_SYS_getgid		47
#define X86_SYS_signal		48
#define X86_SYS_geteuid		49
#define X86_SYS_getegid		50
#define X86_SYS_acct		51
#define X86_SYS_umount2		52
#define X86_SYS_lock		53
#define X86_SYS_ioctl		54
#define X86_SYS_fcntl		55
#define X86_SYS_mpx		56
#define X86_SYS_setpgid		57
#define X86_SYS_ulimit		58

#define X86_SYS_umask		60
#define X86_SYS_chroot		61
#define X86_SYS_ustat		62
#define X86_SYS_dup2		63
#define X86_SYS_getppid		64
#define X86_SYS_getpgrp		65
#define X86_SYS_setsid		66
#define X86_SYS_sigaction	67
#define X86_SYS_sgetmask	68
#define X86_SYS_ssetmask	69
#define X86_SYS_setreuid	70
#define X86_SYS_setregid	71
#define X86_SYS_sigsuspend	72
#define X86_SYS_sigpending	73
#define X86_SYS_sethostname	74
#define X86_SYS_setrlimit	75
#define X86_SYS_getrlimit	76
#define X86_SYS_getrusage	77
#define X86_SYS_gettimeofday	78
#define X86_SYS_settimeofday	79
#define X86_SYS_getgroups16	80
#define X86_SYS_setgroups16	81
#define X86_SYS_oldselect	82
#define X86_SYS_symlink		83

#define X86_SYS_readlink	85
#define X86_SYS_uselib		86
#define X86_SYS_swapon		87
#define X86_SYS_reboot		88
#define X86_SYS_readdir		89
#define X86_SYS_mmap		90
#define X86_SYS_munmap		91
#define X86_SYS_truncate	92
#define X86_SYS_ftruncate	93
#define X86_SYS_fchmod		94
#define X86_SYS_fchown16	95
#define X86_SYS_getpriority	96
#define X86_SYS_setpriority	97
#define X86_SYS_profil		98
#define X86_SYS_statfs		99
#define X86_SYS_fstatfs		100
#define X86_SYS_ioperm		101
#define X86_SYS_socketcall	102
#define X86_SYS_syslog		103
#define X86_SYS_setitimer	104
#define X86_SYS_getitimer	105
#define X86_SYS_stat		106
#define X86_SYS_lstat		107
#define X86_SYS_fstat		108
#define X86_SYS_olduname		109
#define X86_SYS_iopl		110
#define X86_SYS_vhangup		111
#define X86_SYS_idle		112
#define X86_SYS_syscall		113
#define X86_SYS_wait4		114
#define X86_SYS_swapoff		115
#define X86_SYS_sysinfo		116
#define X86_SYS_ipc		117
#define X86_SYS_fsync		118
#define X86_SYS_sigreturn	119
#define X86_SYS_clone		120
#define X86_SYS_setdomainname	121
#define X86_SYS_uname		122
#define X86_SYS_adjtimex	124
#define X86_SYS_mprotect	125
#define X86_SYS_sigprocmask	126
#define X86_SYS_create_module	127
#define X86_SYS_init_module	128
#define X86_SYS_delete_module	129
#define X86_SYS_get_kernel_syms	130
#define X86_SYS_quotactl	131
#define X86_SYS_getpgid		132
#define X86_SYS_fchdir		133
#define X86_SYS_bdflush		134
#define X86_SYS_sysfs		135
#define X86_SYS_personality	136
#define X86_SYS_afs_syscall	137
#define X86_SYS_setfsuid	138
#define X86_SYS_setfsgid	139
#define X86_SYS_llseek		140
#define X86_SYS_getdents	141
#define X86_SYS_select		142
#define X86_SYS_flock		143
#define X86_SYS_msync		144
#define X86_SYS_readv		145
#define X86_SYS_writev		146
#define X86_SYS_getsid		147
#define X86_SYS_fdatasync	148
#define X86_SYS_sysctl		149
#define X86_SYS_mlock		150
#define X86_SYS_munlock		151
#define X86_SYS_mlockall	152
#define X86_SYS_munlockall	153
#define X86_SYS_sched_setparam	154
#define X86_SYS_sched_getparam	155
#define X86_SYS_sched_setscheduler	156
#define X86_SYS_sched_getscheduler	157
#define X86_SYS_sched_yield	158
#define X86_SYS_sched_get_priority_max	159
#define X86_SYS_sched_get_priority_min	160
#define X86_SYS_sched_rr_get_interval	161
#define X86_SYS_nanosleep	162
#define X86_SYS_mremap		163
#define X86_SYS_setresuid		164
#define X86_SYS_getresuid		165
#define X86_SYS_vm86		166
#define X86_SYS_query_module		167
#define X86_SYS_poll		168
#define X86_SYS_nfsservctl		169
#define X86_SYS_setresgid		170
#define X86_SYS_getresgid		171
#define X86_SYS_prctl		172
#define X86_SYS_rt_sigreturn		173
#define X86_SYS_rt_sigaction		174
#define X86_SYS_rt_sigprocmask		175
#define X86_SYS_rt_sigpending		176
#define X86_SYS_rt_sigtimedwait		177
#define X86_SYS_rt_sigqueueinfo		178
#define X86_SYS_rt_sigsuspend		179
#define X86_SYS_rt_pread		180
#define X86_SYS_rt_sys_pwrite		181
#define X86_SYS_chown		182
#define X86_SYS_getcwd		183
#define X86_SYS_capget		184
#define X86_SYS_capset		185
#define X86_SYS_sigaltstack		186
#define X86_SYS_sendfile		187
#define X86_SYS_getpmsg		188
#define X86_SYS_putpmsg		189
#define X86_SYS_vfork		190

#define X86_SYS_ftruncate64	194   /* skumar */
#define X86_SYS_stat64		195
#define X86_SYS_lstat64		196
#define X86_SYS_fstat64		197

#define X86_SYS_mmap2		192
#define X86_SYS_getuid32	199
#define X86_SYS_getgid32	200
#define X86_SYS_geteuid32	201
#define X86_SYS_getegid32	202
#define X86_SYS_getdents64	220
#define X86_SYS_fcntl64		221
#define X86_SYS_futex	240 /* gabe */
#define X86_SYS_set_thread_area	243 /* gabe */
#define X86_SYS_exit_group	252

/* system call wrappers */
#ifdef LINUX_RHEL4
_syscall6(void *, mmap2, void *, start, size_t, length, int, prot, int, flags, int, fd, off_t, offset);
_syscall3(int, getdents, uint, fd, struct dirent *, dirp, uint, count);
_syscall3(int, getdents64, uint, fd, struct dirent *, dirp, uint, count);
_syscall1(int, set_thread_area, struct user_desc *, u_info);
#else
void * mmap2(void * start, size_t length, int prot, int flags, int fd, off_t offset) { return (void*) syscall (X86_SYS_mmap2, start, length, prot, flags, fd, offset); }
int getdents(unsigned int fd, struct dirent * dirp, unsigned int count) { return syscall(X86_SYS_getdents, fd, dirp, count); }
int getdents64(unsigned int fd, struct dirent * dirp, unsigned int count) { return syscall(X86_SYS_getdents64, fd, dirp, count); }
int set_thread_area(struct user_desc * u_info) { return syscall(X86_SYS_set_thread_area,u_info); }
#endif


  /* These are the numbers for the socket function on a socketcall */
  /* these were defined at /usr/include/sys/socketcall.h */
#define X86_SYS_SOCKET      1
#define X86_SYS_BIND        2
#define X86_SYS_CONNECT     3
#define X86_SYS_LISTEN      4
#define X86_SYS_ACCEPT      5
#define X86_SYS_GETSOCKNAME 6
#define X86_SYS_GETPEERNAME 7
#define X86_SYS_SOCKETPAIR  8
#define X86_SYS_SEND        9
#define X86_SYS_RECV        10
#define X86_SYS_SENDTO      11
#define X86_SYS_RECVFROM    12
#define X86_SYS_SHUTDOWN    13
#define X86_SYS_SETSOCKOPT  14
#define X86_SYS_GETSOCKOPT  15
#define X86_SYS_SENDMSG     16
#define X86_SYS_RECVMSG     17


#ifdef linux
#ifdef LINUX_RHEL4
_syscall5(int, _llseek, int, fd, unsigned int, hi, unsigned long, lo, loff_t*, lresult, unsigned int, wh)
#else
int _llseek(int fd, unsigned int hi, unsigned int lo, loff_t * lresult, unsigned int wh) { return syscall(X86_SYS_llseek,fd,hi,lo,lresult,wh); }
#endif

/* 5-6-04 for syscall debug */
int              sim_debug_syscall = 0;

/*  for IOCTL */
#define __KERNEL_NCCS 19

struct __kernel_termios
{
  tcflag_t c_iflag;		/* input mode flags */
  tcflag_t c_oflag;		/* output mode flags */
  tcflag_t c_cflag;		/* control mode flags */
  tcflag_t c_lflag;		/* local mode flags */
  cc_t c_line;		/* line discipline */
  cc_t c_cc[__KERNEL_NCCS];	/* control characters */
};  

#endif

/* translate system call arguments */
struct xlate_table_t
{
  int target_val;
  int host_val;
};

  int
xlate_arg(int target_val, struct xlate_table_t *map, int map_sz, char *name)
{
  int i;

  for (i=0; i < map_sz; i++)
  {
    if (target_val == map[i].target_val)
      return map[i].host_val;
  }

  /* not found, issue warning and return target_val */
  warn("could not translate argument for `%s': %d", name, target_val);
  return target_val;
}

/* internal system call buffer size, used primarily for file name arguments,
   argument larger than this will be truncated */
#define MAXBUFSIZE 		1024

/* total bytes to copy from a valid pointer argument for ioctl() calls,
   syscall.c does not decode ioctl() calls to determine the size of the
   arguments that reside in memory, instead, the ioctl() proxy simply copies
   NUM_IOCTL_BYTES bytes from the pointer argument to host memory */
#define NUM_IOCTL_BYTES		128

/* OSF ioctl() requests */
#define OSF_TIOCGETP		0x40067408
#define OSF_FIONREAD		0x4004667f

/* target stat() buffer definition, the host stat buffer format is
   automagically mapped to/from this format in syscall.c */
struct  linux_statbuf
{
  /* NOTE: this is the internal linux statbuf definition... */
  word_t linux_st_dev;
  word_t pad0;			/* to match Linux padding... */
  dword_t linux_st_ino;
  word_t linux_st_mode;
  word_t linux_st_nlink;
  word_t linux_st_uid;
  word_t linux_st_gid;
  word_t linux_st_rdev;
  word_t pad1;
  dword_t linux_st_size;
  dword_t linux_st_blksize;
  dword_t linux_st_blocks;
  dword_t linux_st_atime;
  dword_t pad2;
  dword_t linux_st_mtime;
  dword_t pad3;
  dword_t linux_st_ctime;
  dword_t pad4;
  dword_t pad5;
  dword_t pad6;
};

struct  linux_statbuf64
{
  /* NOTE: this is the internal linux statbuf definition... */
  word_t linux_st_dev;
  byte_t pad0[10];		/* to match Linux padding... */
  dword_t _linux_st_ino;
  dword_t linux_st_mode;
  dword_t linux_st_nlink;
  dword_t linux_st_uid;
  dword_t linux_st_gid;
  word_t linux_st_rdev;
  byte_t pad1[10];
  qword_t linux_st_size;
  dword_t linux_st_blksize;
  dword_t linux_st_blocks;
  dword_t pad1a;
  dword_t linux_st_atime;
  dword_t pad2;
  dword_t linux_st_mtime;
  dword_t pad3;
  dword_t linux_st_ctime;
  dword_t pad4;
  qword_t linux_st_ino;
};

struct osf_sgttyb {
  byte_t sg_ispeed;	/* input speed */
  byte_t sg_ospeed;	/* output speed */
  byte_t sg_erase;	/* erase character */
  byte_t sg_kill;	/* kill character */
  sword_t sg_flags;	/* mode flags */
};

#define OSF_NSIG		32

#define OSF_SIG_BLOCK		1
#define OSF_SIG_UNBLOCK		2
#define OSF_SIG_SETMASK		3

struct osf_sigcontext {
  qword_t sc_onstack;              /* sigstack state to restore */
  qword_t sc_mask;                 /* signal mask to restore */
  qword_t sc_pc;                   /* pc at time of signal */
  qword_t sc_ps;                   /* psl to retore */
  qword_t sc_regs[32];             /* processor regs 0 to 31 */
  qword_t sc_ownedfp;              /* fp has been used */
  qword_t sc_fpregs[32];           /* fp regs 0 to 31 */
  qword_t sc_fpcr;                 /* floating point control register */
  qword_t sc_fp_control;           /* software fpcr */
};

struct osf_statfs {
  dword_t f_type;		/* type of filesystem (see below) */
  dword_t f_bsize;		/* optimal transfer block size */
  dword_t f_blocks;		/* total data blocks in file system, */
  /* note: may not represent fs size. */
  dword_t f_bfree;		/* free blocks in fs */
  dword_t f_bavail;		/* free blocks avail to non-su */
  dword_t f_files;		/* total file nodes in file system */
  dword_t f_ffree;		/* free file nodes in fs */
  qword_t f_fsid;		/* file system id */
  dword_t f_namelen;
  dword_t f_spare[6];		/* spare for later */
};

struct osf_timeval
{
  sdword_t osf_tv_sec;		/* seconds */
  sdword_t osf_tv_usec;		/* microseconds */
};

struct osf_timezone
{
  sdword_t osf_tz_minuteswest;	/* minutes west of Greenwich */
  sdword_t osf_tz_dsttime;	/* type of dst correction */
};

/* target getrusage() buffer definition, the host stat buffer format is
   automagically mapped to/from this format in syscall.c */
struct osf_rusage
{
  struct osf_timeval osf_ru_utime;
  struct osf_timeval osf_ru_stime;
  sdword_t osf_ru_maxrss;
  sdword_t osf_ru_ixrss;
  sdword_t osf_ru_idrss;
  sdword_t osf_ru_isrss;
  sdword_t osf_ru_minflt;
  sdword_t osf_ru_majflt;
  sdword_t osf_ru_nswap;
  sdword_t osf_ru_inblock;
  sdword_t osf_ru_oublock;
  sdword_t osf_ru_msgsnd;
  sdword_t osf_ru_msgrcv;
  sdword_t osf_ru_nsignals;
  sdword_t osf_ru_nvcsw;
  sdword_t osf_ru_nivcsw;
};

struct osf_rlimit
{
  qword_t osf_rlim_cur;		/* current (soft) limit */
  qword_t osf_rlim_max;		/* maximum value for rlim_cur */
};

struct osf_sockaddr
{
  word_t sa_family;		/* address family, AF_xxx */
  byte_t sa_data[24];		/* 14 bytes of protocol address */
};

struct osf_iovec
{
  md_addr_t iov_base;		/* starting address */
  dword_t iov_len;		/* length in bytes */
  dword_t pad;
};

struct linux_sysctl
{
  md_addr_t name;
  dword_t nlen;
  md_addr_t oldval;
  md_addr_t oldlenp;
  md_addr_t newval;
  dword_t newlen;
};

#if 0
/* returns size of DIRENT structure */
#define OSF_DIRENT_SZ(STR)						\
  (sizeof(dword_t) + 2*sizeof(word_t) + strlen(STR) + 1)
#endif

struct osf_dirent
{
  dword_t d_ino;			/* file number of entry */
  word_t d_reclen;		/* length of this record */
  word_t d_namlen;		/* length of string in d_name */
  char d_name[256];		/* DUMMY NAME LENGTH */
  /* the real maximum length is */
  /* returned by pathconf() */
  /* At this time, this MUST */
  /* be 256 -- the kernel */
  /* requires it */
};

/* open(2) flags for Alpha/AXP OSF target, syscall.c automagically maps
   between these codes to/from host open(2) flags */
#define LINUX_O_RDONLY		00
#define LINUX_O_WRONLY		01
#define LINUX_O_RDWR		02
#define LINUX_O_CREAT		0100
#define LINUX_O_EXCL		0200
#define LINUX_O_NOCTTY		0400
#define LINUX_O_TRUNC		01000
#define LINUX_O_APPEND		02000
#define LINUX_O_NONBLOCK	04000
#define LINUX_O_SYNC		010000
#define LINUX_O_ASYNC		020000

/* open(2) flags translation table for SimpleScalar target */
const struct {
  int linux_flag;
  int local_flag;
} linux_flag_table[] = {
  /* target flag */	/* host flag */
#ifdef _MSC_VER
  { LINUX_O_RDONLY,	_O_RDONLY },
  { LINUX_O_WRONLY,	_O_WRONLY },
  { LINUX_O_RDWR,	_O_RDWR },
  { LINUX_O_APPEND,	_O_APPEND },
  { LINUX_O_CREAT,	_O_CREAT },
  { LINUX_O_TRUNC,	_O_TRUNC },
  { LINUX_O_EXCL,	_O_EXCL },
#ifdef _O_NONBLOCK
  { LINUX_O_NONBLOCK,	_O_NONBLOCK },
#endif
#ifdef _O_NOCTTY
  { LINUX_O_NOCTTY,	_O_NOCTTY },
#endif
#ifdef _O_SYNC
  { LINUX_O_SYNC,	_O_SYNC },
#endif
#else /* !_MSC_VER */
  { LINUX_O_RDONLY,	O_RDONLY },
  { LINUX_O_WRONLY,	O_WRONLY },
  { LINUX_O_RDWR,	O_RDWR },
  { LINUX_O_APPEND,	O_APPEND },
  { LINUX_O_CREAT,	O_CREAT },
  { LINUX_O_TRUNC,	O_TRUNC },
  { LINUX_O_EXCL,	O_EXCL },
  { LINUX_O_NONBLOCK,	O_NONBLOCK },
  { LINUX_O_NOCTTY,	O_NOCTTY },
#ifdef O_SYNC
  { LINUX_O_SYNC,		O_SYNC },
#endif
#ifdef O_ASYNC
  { LINUX_O_ASYNC,	O_ASYNC },
#endif
#endif /* _MSC_VER */
};
#define LINUX_NFLAGS	(sizeof(linux_flag_table)/sizeof(linux_flag_table[0]))

/* unix systems will initialize the following to all zeros */
qword_t sigmask[MAX_CORES]; /* = 0; */

qword_t sigaction_array[MAX_CORES][OSF_NSIG];
/* = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; */

/* setsockopt option names */
#define OSF_SO_DEBUG		0x0001
#define OSF_SO_ACCEPTCONN	0x0002
#define OSF_SO_REUSEADDR	0x0004
#define OSF_SO_KEEPALIVE	0x0008
#define OSF_SO_DONTROUTE	0x0010
#define OSF_SO_BROADCAST	0x0020
#define OSF_SO_USELOOPBACK	0x0040
#define OSF_SO_LINGER		0x0080
#define OSF_SO_OOBINLINE	0x0100
#define OSF_SO_REUSEPORT	0x0200

const struct xlate_table_t sockopt_map[] =
{
  { OSF_SO_DEBUG,	SO_DEBUG },
#ifdef SO_ACCEPTCONN
  { OSF_SO_ACCEPTCONN,	SO_ACCEPTCONN },
#endif
  { OSF_SO_REUSEADDR,	SO_REUSEADDR },
  { OSF_SO_KEEPALIVE,	SO_KEEPALIVE },
  { OSF_SO_DONTROUTE,	SO_DONTROUTE },
  { OSF_SO_BROADCAST,	SO_BROADCAST },
#ifdef SO_USELOOPBACK
  { OSF_SO_USELOOPBACK,	SO_USELOOPBACK },
#endif
  { OSF_SO_LINGER,	SO_LINGER },
  { OSF_SO_OOBINLINE,	SO_OOBINLINE },
#ifdef SO_REUSEPORT
  { OSF_SO_REUSEPORT,	SO_REUSEPORT }
#endif
};

/* setsockopt TCP options */
#define OSF_TCP_NODELAY		0x01 /* don't delay send to coalesce packets */
#define OSF_TCP_MAXSEG		0x02 /* maximum segment size */
#define OSF_TCP_RPTR2RXT	0x03 /* set repeat count for R2 RXT timer */
#define OSF_TCP_KEEPIDLE	0x04 /* secs before initial keepalive probe */
#define OSF_TCP_KEEPINTVL	0x05 /* seconds between keepalive probes */
#define OSF_TCP_KEEPCNT		0x06 /* num of keepalive probes before drop */
#define OSF_TCP_KEEPINIT	0x07 /* initial connect timeout (seconds) */
#define OSF_TCP_PUSH		0x08 /* set push bit in outbnd data packets */
#define OSF_TCP_NODELACK	0x09 /* don't delay send to coalesce packets */

const struct xlate_table_t tcpopt_map[] =
{
  { OSF_TCP_NODELAY,	TCP_NODELAY },
  { OSF_TCP_MAXSEG,	TCP_MAXSEG },
#ifdef TCP_RPTR2RXT
  { OSF_TCP_RPTR2RXT,	TCP_RPTR2RXT },
#endif
#ifdef TCP_KEEPIDLE
  { OSF_TCP_KEEPIDLE,	TCP_KEEPIDLE },
#endif
#ifdef TCP_KEEPINTVL
  { OSF_TCP_KEEPINTVL,	TCP_KEEPINTVL },
#endif
#ifdef TCP_KEEPCNT
  { OSF_TCP_KEEPCNT,	TCP_KEEPCNT },
#endif
#ifdef TCP_KEEPINIT
  { OSF_TCP_KEEPINIT,	TCP_KEEPINIT },
#endif
#ifdef TCP_PUSH
  { OSF_TCP_PUSH,	TCP_PUSH },
#endif
#ifdef TCP_NODELACK
  { OSF_TCP_NODELACK,	TCP_NODELACK }
#endif
};

/* setsockopt level names */
#define OSF_SOL_SOCKET		0xffff	/* options for socket level */
#define OSF_SOL_IP		0	/* dummy for IP */
#define OSF_SOL_TCP		6	/* tcp */
#define OSF_SOL_UDP		17	/* user datagram protocol */

const struct xlate_table_t socklevel_map[] =
{
#if defined(__svr4__) || defined(__osf__)
  { OSF_SOL_SOCKET,	SOL_SOCKET },
  { OSF_SOL_IP,		IPPROTO_IP },
  { OSF_SOL_TCP,	IPPROTO_TCP },
  { OSF_SOL_UDP,	IPPROTO_UDP }
#else
  { OSF_SOL_SOCKET,	SOL_SOCKET },
    { OSF_SOL_IP,		SOL_IP },
    { OSF_SOL_TCP,	SOL_TCP },
    { OSF_SOL_UDP,	SOL_UDP }
#endif
};

/* socket() address families */
#define OSF_AF_UNSPEC		0
#define OSF_AF_UNIX		1	/* Unix domain sockets */
#define OSF_AF_INET		2	/* internet IP protocol */
#define OSF_AF_IMPLINK		3	/* arpanet imp addresses */
#define OSF_AF_PUP		4	/* pup protocols: e.g. BSP */
#define OSF_AF_CHAOS		5	/* mit CHAOS protocols */
#define OSF_AF_NS		6	/* XEROX NS protocols */
#define OSF_AF_ISO		7	/* ISO protocols */

const struct xlate_table_t family_map[] =
{
  { OSF_AF_UNSPEC,	AF_UNSPEC },
  { OSF_AF_UNIX,	AF_UNIX },
  { OSF_AF_INET,	AF_INET },
#ifdef AF_IMPLINK
  { OSF_AF_IMPLINK,	AF_IMPLINK },
#endif
#ifdef AF_PUP
  { OSF_AF_PUP,		AF_PUP },
#endif
#ifdef AF_CHAOS
  { OSF_AF_CHAOS,	AF_CHAOS },
#endif
#ifdef AF_NS
  { OSF_AF_NS,		AF_NS },
#endif
#ifdef AF_ISO
  { OSF_AF_ISO,		AF_ISO }
#endif
};

/* socket() socket types */
#define OSF_SOCK_STREAM		1	/* stream (connection) socket */
#define OSF_SOCK_DGRAM		2	/* datagram (conn.less) socket */
#define OSF_SOCK_RAW		3	/* raw socket */
#define OSF_SOCK_RDM		4	/* reliably-delivered message */
#define OSF_SOCK_SEQPACKET	5	/* sequential packet socket */

const struct xlate_table_t socktype_map[] =
{
  { OSF_SOCK_STREAM,	SOCK_STREAM },
  { OSF_SOCK_DGRAM,	SOCK_DGRAM },
  { OSF_SOCK_RAW,	SOCK_RAW },
  { OSF_SOCK_RDM,	SOCK_RDM },
  { OSF_SOCK_SEQPACKET,	SOCK_SEQPACKET }
};

/* OSF table() call. Right now, we only support TBL_SYSINFO queries */
#define OSF_TBL_SYSINFO		12
struct osf_tbl_sysinfo 
{
  long si_user;		/* user time */
  long si_nice;		/* nice time */
  long si_sys;		/* system time */
  long si_idle;		/* idle time */
  long si_hz;
  long si_phz;
  long si_boottime;	/* boot time in seconds */
  long wait;		/* wait time */
};

struct linux_tms
{
  dword_t tms_utime;		/* user CPU time */
  dword_t tms_stime;		/* system CPU time */

  dword_t tms_cutime;		/* user CPU time of children */
  dword_t tms_cstime;		/* system CPU time of children */

};


/* X86 SYSTEM CALL CONVENTIONS

   System call conventions as taken from unistd.h of arm architecture
   Depending on the call type r0, r1, r2, r3, r4 will contain the arguments
   of the system call.
   The actual system call number is found out using the link register after 
   an swi call is made. How this is done is shown in 
   kernel/arch/arm/kernel/entry-common.S file
   We need to decode the instruction to find out the system call number.

   Return value is returned in register 0. If the return value is between -1
   and -125 inclusive then there's an error.
 */

/* syscall proxy handler, architect registers and memory are assumed to be
   precise when this function is called, register and memory are updated with
   the results of the sustem call */

  void
sys_syscall(struct thread_t *thread,	/* current processor core */
    mem_access_fn mem_fn,	    /* generic memory accessor */
    md_inst_t inst,		        /* system call inst */
    int traceable)		        /* traceable system call? */
{
  int thread_id = thread->id;

  struct regs_t * regs = &thread->regs;
  struct mem_t * mem = thread->mem;

  md_addr_t returnval, hostaddr, length;

  //if(sim_debug_syscall)
    //fprintf(stderr,"Sys call invoked: \n");

  /* Figure out the system call number from the swi instruction
     Last 24 bits gives us this number */

  dword_t syscode = regs->regs_R.dw[MD_REG_EAX];

  /* first, check if an EIO trace is being consumed... */
  /* Left from Alpha code should be the same in X86 though */

  if (traceable && sim_eio_fd[thread_id] != NULL)
  {
    if((syscode != X86_SYS_exit) && (syscode != X86_SYS_exit_group))
      eio_read_trace(thread, sim_eio_fd[thread_id], mem_fn, inst);

    if(sim_debug_syscall)
      fprintf(stderr,"returning\n");
    /* fini... */
    return;
  }

  //if(sim_debug_syscall)
    //fprintf(stderr,"ok to execute live sys call\n");

  /* no, OK execute the live system call... */

  if(sim_debug_syscall)
    fprintf(stderr, "PC %x syscode %d\n", regs->regs_PC, syscode);

  //if(sim_debug_syscall)
    //fprintf(stderr,"sys: %d\n", syscode);
  switch (syscode)
  {

    case X86_SYS_exit:
    case X86_SYS_exit_group: // wrong, but we don't support threads anyhow, so...
      /* exit jumps to the target set in main() */
      longjmp(sim_exit_buf,
          /* exitcode + fudge */(regs->regs_R.dw[MD_REG_EBX] & 0xff) + 1);
      break;

    case X86_SYS_personality:
      regs->regs_R.dw[MD_REG_EAX] = 0x1000000;
      break;

    case X86_SYS_sysctl:
      {
        struct linux_sysctl buf;

        mem_bcopy(thread_id, mem_fn, mem, Read, /*buf*/regs->regs_R.dw[MD_REG_EBX],
            &buf, sizeof(struct linux_sysctl));

        /* write kernel version */
        mem_strcpy(thread_id, mem_fn, mem, Write, /*fname*/buf.oldval, "2.2.16-22");
        MEM_WRITE_WORD(mem, buf.oldlenp, 9);
        regs->regs_R.dw[MD_REG_EAX] = 0;
      }
      break;

      /* CONCERN: What happens if mmap/mmap2 returns an address that obviously
         doesn't conflict with the host (real) memory, *but* it does conflict
         with an already mapped virtual (simulated) memory address?!? */
    case X86_SYS_mmap2:
      {
        md_addr_t mmap_args[6];
        mmap_args[0] = regs->regs_R.dw[MD_REG_EBX];
        mmap_args[1] = regs->regs_R.dw[MD_REG_ECX];
        mmap_args[2] = regs->regs_R.dw[MD_REG_EDX];
        mmap_args[3] = regs->regs_R.dw[MD_REG_ESI];
        mmap_args[4] = regs->regs_R.dw[MD_REG_EDI];
        mmap_args[5] = regs->regs_R.dw[MD_REG_EBP];

        hostaddr = (md_addr_t) mmap_args[0];  // DO NOT TRANSLATE THE ADDRESS.
        /* 2nd arg is buffer length */
        length = mmap_args[1];

        /* we need to make up a new mapping so we don't
           stomp on our own memory if the simulated program 
           passes an address that happens to conflict with us. */

        //mmap_args[2] |= PROT_WRITE;

        returnval = (md_addr_t) mmap2(NULL, length,
            mmap_args[2], (mmap_args[3] & (~MAP_FIXED)), 
            mmap_args[4], mmap_args[5]);
        if(returnval == -1) {
          perror("mmap2 failed!");
        }
        /* translate the returned address into VM address if not already
         * translated
         */
        //if( returnval != -1 ) {
          if (!hostaddr)
            hostaddr = returnval;
          returnval = mem_newmap2(mem, hostaddr, returnval, length, TRUE);
        //}
#ifdef DEBUG
        if(debugging) 
          fprintf(stderr, "MMAP2: requested addr: %x, length %d return addr: %x\n", 
              hostaddr, length, returnval);
#endif
        regs->regs_R.dw[MD_REG_EAX] = returnval;

        break;
      }
    case X86_SYS_mmap:
      {
        md_addr_t stack_args[6];

        /* this copies out the args, which because there are more than 5, are
         * in a structure pointed to by EBX
         */
        mem_bcopy(thread_id, mem_fn, mem, Read, regs->regs_R.dw[MD_REG_EBX], &stack_args, sizeof(stack_args));
        hostaddr = (md_addr_t) stack_args[0];  // DO NOT TRANSLATE THE ADDRESS.
        /* 2nd arg is buffer length */
        length = stack_args[1];
        // we need to make up a new mapping so we don't
        // stomp on our own memory if the simulated program 
        // passes an address that happens to conflict with us.
        stack_args[2] |= PROT_WRITE;
        returnval = (md_addr_t) mmap(NULL, length,
            stack_args[2], (stack_args[3] & (~MAP_FIXED)), 
            stack_args[4], stack_args[5]);
        //if( returnval != -1 ) {
          if (!hostaddr)
            hostaddr = returnval;
          returnval = mem_newmap2(mem, hostaddr, returnval, length, TRUE);
        //}
#ifdef DEBUG
        if(debugging) 
          fprintf(stderr, "MMAP: requested addr: %x, length %d return addr: %x\n", 
              hostaddr, length, returnval);
#endif
        regs->regs_R.dw[MD_REG_EAX] = returnval;
      }
      break;

    case X86_SYS_munmap:
      {

        hostaddr = (md_addr_t) MEM_ADDR2HOST(mem, regs->regs_R.dw[MD_REG_EBX]);
        length = regs->regs_R.dw[MD_REG_ECX];
        mem_delmap( mem, regs->regs_R.dw[MD_REG_EBX], length);
        returnval = munmap((void*)hostaddr, length);

#ifdef DEBUG
        if(debugging) 
          fprintf(stderr, "MUNMAP request addr EBX %x return %d\n", 
              regs->regs_R.dw[MD_REG_EBX], returnval);
#endif
        regs->regs_R.dw[MD_REG_EAX] = returnval;

#ifdef DEBUG
        if (debugging)
          fprintf(stderr, "MUNMAP: -> 0x%08x, %d bytes...\n",
              regs->regs_R.dw[MD_REG_EAX], 
              regs->regs_R.dw[MD_REG_EBX]);
#endif
      }
      break;

    case X86_SYS_mremap: /* GL */
      {
        md_addr_t vaddr = regs->regs_R.dw[MD_REG_EBX];
        hostaddr = (md_addr_t) MEM_ADDR2HOST(mem, vaddr);
        md_addr_t old_size = regs->regs_R.dw[MD_REG_ECX];
        md_addr_t new_size = regs->regs_R.dw[MD_REG_EDX];
        int flags = regs->regs_R.dw[MD_REG_ESI];
        returnval = (md_addr_t) mremap((void*)hostaddr,old_size,new_size,flags|1); /* MREMAP_MAYMOVE == 1 */

        if(((void*)returnval) != MAP_FAILED)
        {
          /* round sizes up to even multiple of page size */
          old_size = (old_size / MD_PAGE_SIZE + ((old_size % MD_PAGE_SIZE>0)? 1 : 0)) * MD_PAGE_SIZE;
          new_size = (new_size / MD_PAGE_SIZE + ((new_size % MD_PAGE_SIZE>0)? 1 : 0)) * MD_PAGE_SIZE;

          if(returnval != hostaddr) /* remapped to new location */
          {
            mem_delmap(mem, vaddr, old_size); /* remove old mapping */
            returnval = mem_newmap2(mem, returnval, returnval, new_size, TRUE); /* add new mapping */
          }
          else /* size of existing location modified */
          {
            if(new_size > old_size)
            {
              mem_newmap2(mem, vaddr+old_size, returnval+old_size, new_size-old_size, TRUE); /* add to end of block */
            }
            else if(new_size < old_size)
            {
              mem_delmap(mem, vaddr+new_size, old_size-new_size); /* remove end of old block */
            }
            /* else equal: don't do anything */
          }
        }
      }
      regs->regs_R.dw[MD_REG_EAX] = returnval;
      break;


    case X86_SYS_read:
      {
        char *buf;
        int error_return;

        /* allocate same-sized input buffer in host memory */
        if (!(buf =
              (char *)calloc(/*nbytes*/regs->regs_R.dw[MD_REG_EDX], sizeof(char))))
          fatal("out of memory in SYS_read");

        /* read data from file */
        do {
          /*nread*/error_return =
            read(/*fd*/regs->regs_R.dw[MD_REG_EBX], buf,
                /*nbytes*/regs->regs_R.dw[MD_REG_EDX]);
          if (error_return <= -1 && error_return >= -125)
            regs->regs_R.dw[MD_REG_EAX] = -errno;
          else
            regs->regs_R.dw[MD_REG_EAX] = error_return;
        } while (/*nread*/error_return == -1
            && errno == EAGAIN);

        /* check for error condition, is not necessary to do this but lets keep it for now 
           it is taken care of in the do-while loop */

        if (error_return != -1) {
          regs->regs_R.dw[MD_REG_EAX] = error_return;
        } /* no error */
        else {
          regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
        }

        /* copy results back into host memory */
        mem_bcopy(thread_id, mem_fn, mem, Write,
            /*buf*/regs->regs_R.dw[MD_REG_ECX], buf,
            /*nread*/regs->regs_R.dw[MD_REG_EDX]);

        /* done with input buffer */
        free(buf);
      }
      break;

    case X86_SYS_write:
      {
        char *wrbuf;
        int error_return;

        /* allocate same-sized output buffer in host memory */
        if (!(wrbuf =
              (char *)calloc(/*nbytes*/regs->regs_R.dw[MD_REG_EDX], sizeof(char))))
          fatal("out of memory in SYS_write");

        /* copy inputs into host memory */
        mem_bcopy(thread_id, mem_fn, mem, Read, /*wrbuf*/regs->regs_R.dw[MD_REG_ECX], wrbuf,
            /*nbytes*/regs->regs_R.dw[MD_REG_EDX]);

        /* write data to file */
        if (sim_progfd && MD_OUTPUT_SYSCALL(inst,regs))
        {
          /* redirect program output to file */

          /*nwritten*/error_return =
            fwrite(wrbuf, 1, /*nbytes*/regs->regs_R.dw[MD_REG_EDX], sim_progfd);
        }
        else
        {
          /* perform program output request */
          do {
            /*nwritten*/error_return =
              write(/*fd*/regs->regs_R.dw[MD_REG_EBX],
                  wrbuf, /*nbytes*/regs->regs_R.dw[MD_REG_EDX]);
          } while (/*nwritten*/error_return == -1 && errno == EAGAIN);
        }

        /* check for an error condition */
        if (error_return <= -1)
          regs->regs_R.dw[MD_REG_EAX] = -errno;
        else
          regs->regs_R.dw[MD_REG_EAX] = error_return;

        /* done with output buffer */
        free(wrbuf);
      }
      break;

      /* I can't find this system call in linux ?? ctw */
#if !defined(MIN_SYSCALL_MODE) && 0
      /* ADDED BY CALDER 10/27/99 */
    case X86_SYS_getdomainname:
      /* get program scheduling priority */
      {
        char *buf;
        int error_return;

        buf = calloc(1,/* len */(size_t)regs->regs_R.dw[MD_REG_ECX]);
        if (!buf)
          fatal("out of virtual memory in gethostname()");

        /* result */error_return =
          getdomainname(/* name */buf,
              /* len */(size_t)regs->regs_R.dw[MD_REG_ECX]);

        /* copy string back to simulated memory */
        mem_bcopy(thread_id, mem_fn, mem, Write,
            /* name */regs->regs_R.dw[MD_REG_EBX],
            buf, /* len */regs->regs_R.dw[MD_REG_ECX]);
      }

      /* check for an error condition */
      if (error_return != -1) {
        regs->regs_R.dw[MD_REG_EAX] = error_return;
      } /* no error */
      else {
        regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
      }
  }
  break;
#endif

#if !defined(MIN_SYSCALL_MODE)
  /* ADDED BY CALDER 10/27/99 */
  case X86_SYS_flock:
  /* get flock() information on the file */
  {
    int error_return;

    error_return =
      flock(/*fd*/(int)regs->regs_R.dw[MD_REG_EBX],
          /*cmd*/(int)regs->regs_R.dw[MD_REG_ECX]);

    /* check for an error condition */
    if (error_return != -1) {
      regs->regs_R.dw[MD_REG_EAX] = error_return;
    } /* no error */
    else {
      regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
    }
  }

  break;
#endif

  /*-------------------------------Is there a bind in arm-linux??----------------------*/
#if !defined(MIN_SYSCALL_MODE) && 0
  /* ADDED BY CALDER 10/27/99 */
  case OSF_SYS_bind:
  {
    const struct sockaddr a_sock;

    mem_bcopy(thread_id, mem_fn, mem, Read, /* serv_addr */regs->regs_R[MD_REG_A1],
        &a_sock, /* addrlen */(int)regs->regs_R[MD_REG_A2]);

    regs->regs_R[MD_REG_V0] =
      bind((int) regs->regs_R[MD_REG_A0],
          &a_sock,(int) regs->regs_R[MD_REG_A2]);

    /* check for an error condition */
    if (regs->regs_R[MD_REG_V0] != (dword_t)-1)
      regs->regs_R[MD_REG_A3] = 0;
    else /* got an error, return details */
    {
      regs->regs_R[MD_REG_A3] = -1;
      regs->regs_R[MD_REG_V0] = errno;
    }
  }
  break;
#endif
  /*----------------------------------------------------------------------------------------*/

  /*-------------------------------Is there a sendto in arm-linux??----------------------*/
#if !defined(MIN_SYSCALL_MODE) && 0
  /* ADDED BY CALDER 10/27/99 */
  case OSF_SYS_sendto:
  {
    char *buf = NULL;
    struct sockaddr d_sock;
    int buf_len = 0;

    buf_len = regs->regs_R[MD_REG_A2];

    if (buf_len > 0)
      buf = (char *) calloc(1,buf_len*sizeof(char));

    mem_bcopy(thread_id, mem_fn, mem, Read, /* serv_addr */regs->regs_R[MD_REG_A1],
        buf, /* addrlen */(int)regs->regs_R[MD_REG_A2]);

    if (regs->regs_R[MD_REG_A5] > 0) 
      mem_bcopy(thread_id, mem_fn, mem, Read, regs->regs_R[MD_REG_A4],
          &d_sock, (int)regs->regs_R[MD_REG_A5]);

    regs->regs_R[MD_REG_V0] =
      sendto((int) regs->regs_R[MD_REG_A0],
          buf,(int) regs->regs_R[MD_REG_A2],
          (int) regs->regs_R[MD_REG_A3],
          &d_sock,(int) regs->regs_R[MD_REG_A5]);

    mem_bcopy(thread_id, mem_fn, mem, Write, /* serv_addr */regs->regs_R[MD_REG_A1],
        buf, /* addrlen */(int)regs->regs_R[MD_REG_A2]);

    /* maybe copy back whole size of sockaddr */
    if (regs->regs_R[MD_REG_A5] > 0)
      mem_bcopy(thread_id, mem_fn, mem, Write, regs->regs_R[MD_REG_A4],
          &d_sock, (int)regs->regs_R[MD_REG_A5]);

    /* check for an error condition */
    if (regs->regs_R[MD_REG_V0] != (dword_t)-1)
      regs->regs_R[MD_REG_A3] = 0;
    else /* got an error, return details */
    {
      regs->regs_R[MD_REG_A3] = -1;
      regs->regs_R[MD_REG_V0] = errno;
    }

    if (buf != NULL) 
      free(buf);
  }
  break;
#endif
  /*----------------------------------------------------------------------------------------*/

  /*-------------------------------Is there a recvfrom in arm-linux??----------------------*/
#if !defined(MIN_SYSCALL_MODE) && 0
  /* ADDED BY CALDER 10/27/99 */
  case OSF_SYS_old_recvfrom:
  case OSF_SYS_recvfrom:
  {
    int addr_len;
    char *buf;
    struct sockaddr *a_sock;

    buf = (char *) calloc(regs->regs_R[MD_REG_A2],sizeof(char));

    mem_bcopy(thread_id, mem_fn, mem, Read, /* serv_addr */regs->regs_R[MD_REG_A1],
        buf, /* addrlen */(int)regs->regs_R[MD_REG_A2]);

    mem_bcopy(thread_id, mem_fn, mem, Read, /* serv_addr */regs->regs_R[MD_REG_A5],
        &addr_len, sizeof(int));

    a_sock = (struct sockaddr *)calloc(1,addr_len);

    mem_bcopy(thread_id, mem_fn, mem, Read, regs->regs_R[MD_REG_A4],
        a_sock, addr_len);

    regs->regs_R[MD_REG_V0] =
      recvfrom((int) regs->regs_R[MD_REG_A0],
          buf,(int) regs->regs_R[MD_REG_A2],
          (int) regs->regs_R[MD_REG_A3], a_sock,&addr_len);

    mem_bcopy(thread_id, mem_fn, mem, Write, regs->regs_R[MD_REG_A1],
        buf, (int) regs->regs_R[MD_REG_V0]);

    mem_bcopy(thread_id, mem_fn, mem, Write, /* serv_addr */regs->regs_R[MD_REG_A5],
        &addr_len, sizeof(int));

    mem_bcopy(thread_id, mem_fn, mem, Write, regs->regs_R[MD_REG_A4],
        a_sock, addr_len);

    /* check for an error condition */
    if (regs->regs_R[MD_REG_V0] != (dword_t)-1)
      regs->regs_R[MD_REG_A3] = 0;
    else /* got an error, return details */
    {
      regs->regs_R[MD_REG_A3] = -1;
      regs->regs_R[MD_REG_V0] = errno;
    }
    if (buf != NULL)
      free(buf);
  }
  break;
#endif
  /*----------------------------------------------------------------------------------------*/

  case X86_SYS_open:
  {
    char buf[MAXBUFSIZE];
    int error_return;
    int linux_flags = regs->regs_R.dw[MD_REG_ECX], local_flags = 0;

#if 0
    unsigned int i;
    /* Need to make sure if these flags are the same in X86 */
    /* translate open(2) flags */
    for (i=0; i < LINUX_NFLAGS; i++)
    {
      if (linux_flags & linux_flag_table[i].linux_flag)
      {
        linux_flags &= ~linux_flag_table[i].linux_flag;
        local_flags |= linux_flag_table[i].local_flag;
      }
    }
    /* any target flags left? */
    if (linux_flags != 0)
      fatal("syscall: open: cannot decode flags: 0x%08x", linux_flags);
#endif
    local_flags = linux_flags; // skumar - FIXME Need to check flag definitions
    // this works if binary is simulated on same machine 
    // it was compiled on

    /* copy filename to host memory */
    mem_strcpy(thread_id, mem_fn, mem, Read, /*fname*/regs->regs_R.dw[MD_REG_EBX], buf);

    /* open the file */
#ifdef __CYGWIN32__
    /*fd*/error_return =
      open(buf, local_flags|O_BINARY, /*mode*/regs->regs_R.dw[MD_REG_EDX]);
#else /* !__CYGWIN32__ */
    /*fd*/error_return =
      open(buf, local_flags, /*mode*/regs->regs_R.dw[MD_REG_EDX]);
#endif /* __CYGWIN32__ */

    /* check for an error condition */
    if (error_return != -1) {
      regs->regs_R.dw[MD_REG_EAX] = error_return;
    } /* no error */
    else {
      regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
    }
  }
  break;

  case X86_SYS_close:
  {
    int error_return;
    /* don't close stdin, stdout, or stderr as this messes up sim logs */
    if (/*fd*/regs->regs_R.dw[MD_REG_EBX] == 0
        || /*fd*/regs->regs_R.dw[MD_REG_EBX] == 1
        || /*fd*/regs->regs_R.dw[MD_REG_EBX] == 2)
    {
      regs->regs_R.dw[MD_REG_EAX] = 0;
      break;
    }

    /* close the file */
    error_return = close(/*fd*/regs->regs_R.dw[MD_REG_EBX]);

    /* check for an error condition */
    if (error_return != -1) {
      regs->regs_R.dw[MD_REG_EAX] = error_return;
    } /* no error */
    else {
      regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
    }
  }
  break;

#if 0
  case X86_SYS_creat:
  {
    char buf[MAXBUFSIZE];
    int error_return;

    /* copy filename to host memory */
    mem_strcpy(thread_id, mem_fn, Read, /*fname*/regs->regs_R.dw[MD_REG_EBX], buf);

    /* create the file */
    /*fd*/error_return =
      creat(buf, /*mode*/regs->regs_R.dw[MD_REG_ECX]);

    /* check for an error condition */
    if (error_return != -1) {
      regs->regs_R.dw[MD_REG_EAX] = error_return;
    } /* no error */
    else {
      regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
    }
  }
  break;
#endif

  case X86_SYS_unlink:
  {
    char buf[MAXBUFSIZE];
    int error_return;

    /* copy filename to host memory */
    mem_strcpy(thread_id, mem_fn, mem, Read, /*fname*/regs->regs_R.dw[MD_REG_EBX], buf);

    /* delete the file */
    /*result*/error_return = unlink(buf);

    /* check for an error condition */
    if (error_return != -1) {
      regs->regs_R.dw[MD_REG_EAX] = error_return;
    } /* no error */
    else {
      regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
    }
  }
  break;

  case X86_SYS_chdir:
  {
    char buf[MAXBUFSIZE];
    int error_return;

    /* copy filename to host memory */
    mem_strcpy(thread_id, mem_fn, mem, Read, /*fname*/regs->regs_R.dw[MD_REG_EBX], buf);

    /* change the working directory */
    /*result*/error_return = chdir(buf);

    /* check for an error condition */
    if (error_return != -1) {
      regs->regs_R.dw[MD_REG_EAX] = error_return; /* normal return value */
    } /* no error */
    else {
      regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
    }
  }
  break;

  case X86_SYS_time:
  {
    time_t intime = regs->regs_R.dw[MD_REG_EBX];
    dword_t tval = (dword_t)time(&intime);
    /* fprintf(stderr, "time(%d) = %d\n", (int)intime, tval); */

    /* check for an error condition */
    if (tval != (dword_t)-1)
      regs->regs_R.dw[MD_REG_EAX] = tval; /* normal return value */
    else
      regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the errnum */
  }
  break;

  case X86_SYS_chmod:
  {
    char buf[MAXBUFSIZE];

    /* copy filename to host memory */
    mem_strcpy(thread_id, mem_fn, mem, Read, /*fname*/regs->regs_R.dw[MD_REG_EBX], buf);

    /* chmod the file */
    /*result*/regs->regs_R.dw[MD_REG_EAX] =
      chmod(buf, /*mode*/regs->regs_R.dw[MD_REG_ECX]);

    /* check for an error condition */
    if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
      /* got an error, return details */
      regs->regs_R.dw[MD_REG_EAX] = -errno;

  }
  break;

  case X86_SYS_chown:
#ifdef _MSC_VER
  warn("syscall chown() not yet implemented for MSC...");
  regs->regs_R.dw[MD_REG_EAX] = 0;
#else /* !_MSC_VER */
  {
    char buf[MAXBUFSIZE];

    /* copy filename to host memory */
    mem_strcpy(thread_id, mem_fn, mem,Read, /*fname*/regs->regs_R.dw[MD_REG_EBX], buf);

    /* chown the file */
    /*result*/regs->regs_R.dw[MD_REG_EAX] =
      chown(buf, /*owner*/regs->regs_R.dw[MD_REG_ECX],
          /*group*/regs->regs_R.dw[MD_REG_EDX]);

    /* check for an error condition */
    if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
      /* got an error, return details */
      regs->regs_R.dw[MD_REG_EAX] = -errno;
  }
#endif /* _MSC_VER */
  break;

  case X86_SYS_getcwd:
#ifdef _MSC_VER
  warn("syscall getcwd() not yet implemented for MSC...");
  regs->regs_R.dw[MD_REG_EAX] = 0;
#else /* !_MSC_VER */
  {
    char buf[MAXBUFSIZE];

    /* chown the file */
    char * result = getcwd(buf, /*size*/regs->regs_R.dw[MD_REG_ECX]);

    /* copy directory from host memory */
    mem_bcopy(thread_id, mem_fn, mem, Write, regs->regs_R.dw[MD_REG_EBX],
          (void*) buf, regs->regs_R.dw[MD_REG_ECX]);

    /* check for an error condition */
    if(result)
      regs->regs_R.dw[MD_REG_EAX] = regs->regs_R.dw[MD_REG_EBX];
    else
      regs->regs_R.dw[MD_REG_EAX] = (dword_t)0;

  }
#endif /* _MSC_VER */
  break;

  case X86_SYS_brk:
  {
    md_addr_t addr = regs->regs_R.dw[MD_REG_EBX];

    if (addr != 0)
    {
      thread->loader.brk_point = addr;
      regs->regs_R.dw[MD_REG_EAX] = thread->loader.brk_point;

      /* check whether heap area has merged with stack area */
      if (addr >= regs->regs_R.dw[MD_REG_ESP])
      {
        /* out of address space, indicate error */
        regs->regs_R.dw[MD_REG_EAX] = -ENOMEM;
      }
    }
    else /* just return break point */
      regs->regs_R.dw[MD_REG_EAX] = thread->loader.brk_point;
  }
  break;

  case X86_SYS_lseek:
  /* seek into file */
  regs->regs_R.dw[MD_REG_EAX] =
    lseek(/*fd*/regs->regs_R.dw[MD_REG_EBX],
        /*off*/regs->regs_R.dw[MD_REG_ECX], /*dir*/regs->regs_R.dw[MD_REG_EDX]);

  /* check for an error condition */
  if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
    /* got an error, return details */
    regs->regs_R.dw[MD_REG_EAX] = -errno;

  break;

  case X86_SYS_getpid:
  /* get the simulator process id */
  /*result*/regs->regs_R.dw[MD_REG_EAX] = getpid();

  /* check for an error condition */
  if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
    /* got an error, return details */
    regs->regs_R.dw[MD_REG_EAX] = -errno;

  break;

  case X86_SYS_getuid:
  case X86_SYS_getuid32:
#ifdef _MSC_VER
  warn("syscall getuid() not yet implemented for MSC...");
  regs->regs_R.dw[MD_REG_EAX] = 0;
#else /* !_MSC_VER */
  /* get current user id */
  /*first result*/regs->regs_R.dw[MD_REG_EAX] = getuid();

  /* check for an error condition */
  if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
    /* got an error, return details */
    regs->regs_R.dw[MD_REG_EAX] = -errno;
#endif /* _MSC_VER */
  break;


  case X86_SYS_geteuid:
  case X86_SYS_geteuid32:
#ifdef _MSC_VER
  warn("syscall getuid() not yet implemented for MSC...");
  regs->regs_R.dw[MD_REG_EAX] = 0;
#else /* !_MSC_VER */
  /* get current user id */
  /*first result*/regs->regs_R.dw[MD_REG_EAX] = geteuid();

  /* check for an error condition */
  if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
    /* got an error, return details */
    regs->regs_R.dw[MD_REG_EAX] = -errno;
#endif /* _MSC_VER */
  break;


  case X86_SYS_access:
  {
    char buf[MAXBUFSIZE];

    /* copy filename to host memory */
    mem_strcpy(thread_id, mem_fn, mem, Read, /*fName*/regs->regs_R.dw[MD_REG_EBX], buf);

    /* check access on the file */
    /*result*/regs->regs_R.dw[MD_REG_EAX] =
      access(buf, /*mode*/regs->regs_R.dw[MD_REG_ECX]);

    /* check for an error condition */
    if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
      /* got an error, return details */
      regs->regs_R.dw[MD_REG_EAX] = -errno;
  }
  break;

  case X86_SYS_stat:
  case X86_SYS_lstat:
  {
    char buf[MAXBUFSIZE];
    struct linux_statbuf linux_sbuf;
    struct stat sbuf;

    /* copy filename to host memory */
    mem_strcpy(thread_id, mem_fn, mem, Read, /*fName*/regs->regs_R.dw[MD_REG_EBX], buf);

    /* stat() the file */
    if (syscode == X86_SYS_stat)
      /*result*/regs->regs_R.dw[MD_REG_EAX] = stat(buf, &sbuf);
    else /* syscode == X86_SYS_lstat */
      /*result*/regs->regs_R.dw[MD_REG_EAX] = lstat(buf, &sbuf);

    /* check for an error condition */
    if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
      /* got an error, return details */
      regs->regs_R.dw[MD_REG_EAX] = -errno;

    /* translate from host stat structure to target format */
    linux_sbuf.linux_st_dev = MD_SWAPW(sbuf.st_dev);
    linux_sbuf.linux_st_ino = MD_SWAPD(sbuf.st_ino);
    linux_sbuf.linux_st_mode = MD_SWAPW(sbuf.st_mode);
    linux_sbuf.linux_st_nlink = MD_SWAPW(sbuf.st_nlink);
    linux_sbuf.linux_st_uid = MD_SWAPW(sbuf.st_uid);
    linux_sbuf.linux_st_gid = MD_SWAPW(sbuf.st_gid);
    linux_sbuf.linux_st_rdev = MD_SWAPW(sbuf.st_rdev);
    linux_sbuf.linux_st_size = MD_SWAPD(sbuf.st_size);
    linux_sbuf.linux_st_blksize = MD_SWAPD(sbuf.st_blksize);
    linux_sbuf.linux_st_blocks = MD_SWAPD(sbuf.st_blocks);
    linux_sbuf.linux_st_atime = MD_SWAPD(sbuf.st_atime);
    linux_sbuf.linux_st_mtime = MD_SWAPD(sbuf.st_mtime);
    linux_sbuf.linux_st_ctime = MD_SWAPD(sbuf.st_ctime);

    /* copy stat() results to simulator memory */
    mem_bcopy(thread_id, mem_fn, mem, Write, /*sbuf*/regs->regs_R.dw[MD_REG_ECX],
        &linux_sbuf, sizeof(struct linux_statbuf));
  }
  break;

  case X86_SYS_stat64:
  case X86_SYS_lstat64:
  {
    char buf[MAXBUFSIZE];
    struct linux_statbuf64 linux_sbuf;
    struct stat sbuf;

    /* copy filename to host memory */
    mem_strcpy(thread_id, mem_fn, mem, Read, /*fName*/regs->regs_R.dw[MD_REG_EBX], buf);

    /* stat() the file */
    if (syscode == X86_SYS_stat64)
      /*result*/regs->regs_R.dw[MD_REG_EAX] = stat(buf, &sbuf);
    else /* syscode == X86_SYS_lstat64 */
      /*result*/regs->regs_R.dw[MD_REG_EAX] = lstat(buf, &sbuf);

    /* check for an error condition */
    if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
      /* got an error, return details */
      regs->regs_R.dw[MD_REG_EAX] = -errno;

    /* translate from host stat structure to target format */
    linux_sbuf.linux_st_dev = MD_SWAPW(sbuf.st_dev);
    linux_sbuf.linux_st_ino = MD_SWAPQ(sbuf.st_ino);
    linux_sbuf.linux_st_mode = MD_SWAPD(sbuf.st_mode);
    linux_sbuf.linux_st_nlink = MD_SWAPD(sbuf.st_nlink);
    linux_sbuf.linux_st_uid = MD_SWAPD(sbuf.st_uid);
    linux_sbuf.linux_st_gid = MD_SWAPD(sbuf.st_gid);
    linux_sbuf.linux_st_rdev = MD_SWAPW(sbuf.st_rdev);
    linux_sbuf.linux_st_size = MD_SWAPQ(sbuf.st_size);
    linux_sbuf.linux_st_blksize = MD_SWAPD(sbuf.st_blksize);
    linux_sbuf.linux_st_blocks = MD_SWAPD(sbuf.st_blocks);
    linux_sbuf.linux_st_atime = MD_SWAPD(sbuf.st_atime);
    linux_sbuf.linux_st_mtime = MD_SWAPD(sbuf.st_mtime);
    linux_sbuf.linux_st_ctime = MD_SWAPD(sbuf.st_ctime);

    /* copy stat() results to simulator memory */
    mem_bcopy(thread_id, mem_fn, mem, Write, /*sbuf*/regs->regs_R.dw[MD_REG_ECX],
        &linux_sbuf, sizeof(struct linux_statbuf64));
  }
  break;

  case X86_SYS_fstat:
  {
    struct linux_statbuf linux_sbuf;
    struct stat sbuf;

    /* fstat() the file */
    /*result*/regs->regs_R.dw[MD_REG_EAX] =
      fstat(/*fd*/regs->regs_R.dw[MD_REG_EBX], &sbuf);

    /* check for an error condition */
    if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
      /* got an error, return details */
      regs->regs_R.dw[MD_REG_EAX] = -errno;

    /* translate the stat structure to host format */
    linux_sbuf.linux_st_dev = MD_SWAPW(sbuf.st_dev);
    linux_sbuf.linux_st_ino = MD_SWAPD(sbuf.st_ino);
    linux_sbuf.linux_st_mode = MD_SWAPW(sbuf.st_mode);
    linux_sbuf.linux_st_nlink = MD_SWAPW(sbuf.st_nlink);
    linux_sbuf.linux_st_uid = MD_SWAPW(sbuf.st_uid);
    linux_sbuf.linux_st_gid = MD_SWAPW(sbuf.st_gid);
    linux_sbuf.linux_st_rdev = MD_SWAPW(sbuf.st_rdev);
    linux_sbuf.linux_st_size = MD_SWAPD(sbuf.st_size);
    linux_sbuf.linux_st_blksize = MD_SWAPD(sbuf.st_blksize);
    linux_sbuf.linux_st_blocks = MD_SWAPD(sbuf.st_blocks);
    linux_sbuf.linux_st_atime = MD_SWAPD(sbuf.st_atime);
    linux_sbuf.linux_st_mtime = MD_SWAPD(sbuf.st_mtime);
    linux_sbuf.linux_st_ctime = MD_SWAPD(sbuf.st_ctime);

    /* copy fstat() results to simulator memory */
    mem_bcopy(thread_id, mem_fn, mem, Write, /*sbuf*/regs->regs_R.dw[MD_REG_ECX],
        &linux_sbuf, sizeof(struct linux_statbuf));
  }
  break;

  case X86_SYS_fstat64:
  {
    struct linux_statbuf64 linux_sbuf;
    struct stat sbuf;

    /* fstat() the file */
    /*result*/regs->regs_R.dw[MD_REG_EAX] =
      fstat(/*fd*/regs->regs_R.dw[MD_REG_EBX], &sbuf);

    /* check for an error condition */
    if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
      /* got an error, return details */
      regs->regs_R.dw[MD_REG_EAX] = -errno;

    /* translate the stat structure to host format */
    linux_sbuf.linux_st_dev = MD_SWAPW(sbuf.st_dev);
    linux_sbuf.linux_st_ino = MD_SWAPQ(sbuf.st_ino);
    linux_sbuf.linux_st_mode = MD_SWAPD(sbuf.st_mode);
    linux_sbuf.linux_st_nlink = MD_SWAPD(sbuf.st_nlink);
    linux_sbuf.linux_st_uid = MD_SWAPD(sbuf.st_uid);
    linux_sbuf.linux_st_gid = MD_SWAPD(sbuf.st_gid);
    linux_sbuf.linux_st_rdev = MD_SWAPW(sbuf.st_rdev);
    linux_sbuf.linux_st_size = MD_SWAPQ(sbuf.st_size);
    linux_sbuf.linux_st_blksize = MD_SWAPD(sbuf.st_blksize);
    linux_sbuf.linux_st_blocks = MD_SWAPD(sbuf.st_blocks);
    linux_sbuf.linux_st_atime = MD_SWAPD(sbuf.st_atime);
    linux_sbuf.linux_st_mtime = MD_SWAPD(sbuf.st_mtime);
    linux_sbuf.linux_st_ctime = MD_SWAPD(sbuf.st_ctime);

    /* copy fstat() results to simulator memory */
    mem_bcopy(thread_id, mem_fn, mem, Write, /*sbuf*/regs->regs_R.dw[MD_REG_ECX],
        &linux_sbuf, sizeof(struct linux_statbuf64));
  }
  break;

#if 0
  case X86_SYS_pipe: /* Be careful with this one */
  {
    int fd[2];

    /* copy pipe descriptors to host memory */;
    mem_bcopy(thread_id, mem_fn, mem, Read, /*fd's*/regs->regs_R.dw[MD_REG_EBX],
        fd, sizeof(fd));

    /* create a pipe */
    /*result*/regs->regs_R[MD_REG_R7] = pipe(fd);

    /* copy descriptor results to result registers */
    /*pipe1*/regs->regs_R.dw[MD_REG_EAX] = fd[0];
    /*pipe 2*/regs->regs_R.dw[MD_REG_ECX] = fd[1];

    /* check for an error condition */
    if (regs->regs_R[MD_REG_R7] == (dword_t)-1)
      /* got an error, return details */
      regs->regs_R.dw[MD_REG_EAX] = -errno;
  }
}
break;
#endif

case X86_SYS_times:
{
  struct linux_tms tms;
  struct tms ltms;
  clock_t result;

  result = times(&ltms);
  /* fprintf(stderr, "ut=%ld, st=%ld...\n", ltms.tms_utime, ltms.tms_stime); */
  tms.tms_utime = ltms.tms_utime;
  tms.tms_stime = ltms.tms_stime;
  tms.tms_cutime = ltms.tms_cutime;
  tms.tms_cstime = ltms.tms_cstime;

  mem_bcopy(thread_id, mem_fn, mem, Write,
      /* buf */regs->regs_R.dw[MD_REG_EBX],
      &tms, sizeof(struct linux_tms));

  if (result != (dword_t)-1)
    regs->regs_R.dw[MD_REG_EAX] = result;
  else
    regs->regs_R.dw[MD_REG_EAX] = -errno; /* got an error, return details */
}
break;

case X86_SYS_getgid:
case X86_SYS_getgid32:
#ifdef _MSC_VER
warn("syscall getgid() not yet implemented for MSC...");
regs->regs_R.dw[MD_REG_EAX] = 0;
#else /* !_MSC_VER */
/* get current group id */
/*first result*/regs->regs_R.dw[MD_REG_EAX] = getgid();

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
#endif /* _MSC_VER */
  break;

  case X86_SYS_getegid:
  case X86_SYS_getegid32:
#ifdef _MSC_VER
  warn("syscall getgid() not yet implemented for MSC...");
  regs->regs_R.dw[MD_REG_EAX] = 0;
#else /* !_MSC_VER */
  /* get current group id */
  /*first result*/regs->regs_R.dw[MD_REG_EAX] = getegid();

  /* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
#endif /* _MSC_VER */
  break;

  case X86_SYS_ioctl: 
{
  /******   current ioctl pass *****/
  /* This works on crafty (so far) 5-6-04 */
  char buf[NUM_IOCTL_BYTES];
  unsigned request = regs->regs_R.dw[MD_REG_ECX];
  unsigned ioctl_size = _IOC_SIZE(request); 
  struct __kernel_termios termios;


  if(request==TCGETS) {
    memset(&termios, 0, sizeof(struct __kernel_termios));

    /*result*/regs->regs_R.dw[MD_REG_EAX] =
      ioctl(/*fd*/regs->regs_R.dw[MD_REG_EBX], 
          request, 
          &termios);

    if(regs->regs_R.dw[MD_REG_EDX]!=0) {
      mem_bcopy(thread_id, mem_fn, mem, Write, regs->regs_R.dw[MD_REG_EDX],
          (void*) &termios, sizeof(struct __kernel_termios));                    
    }
  }
  else {
    /*  not really tested, but doesn't seem to make a 
     * difference 
     */
    /* Read */
    memset(buf, 0, NUM_IOCTL_BYTES);
    if ((_IOC_DIR(request) & _IOC_READ) && (ioctl_size>0)) {
      mem_bcopy(thread_id, mem_fn, mem, Read, /*argp*/regs->regs_R.dw[MD_REG_EDX],
          (void*) buf, ioctl_size);
    }

    /* perform the ioctl() call */
    /*result*/regs->regs_R.dw[MD_REG_EAX] =
      ioctl(/*fd*/regs->regs_R.dw[MD_REG_EBX], 
          request, 
          buf);

    /* if arg ptr exists, copy NUM_IOCTL_BYTES bytes from host mem */
    if ((_IOC_DIR(request) & _IOC_WRITE) && (ioctl_size>0)) {
      mem_bcopy(thread_id, mem_fn, mem, Write, regs->regs_R.dw[MD_REG_EDX],
          (void*) buf, ioctl_size);
    }
  }
  break;
}


/***  5-6-04 skip ****/
switch (/* req */regs->regs_R.dw[MD_REG_ECX])
{
#if !defined(TIOCGETP) && defined(linux)
  case OSF_TIOCGETP: /* <Out,TIOCGETP,6> */
    {
      struct termios lbuf;
      struct osf_sgttyb buf;

      /* result */regs->regs_R.dw[MD_REG_EAX] =
        tcgetattr(/* fd */(int)regs->regs_R.dw[MD_REG_EBX],
            &lbuf);

      /* translate results */
      buf.sg_ispeed = lbuf.c_ispeed;
      buf.sg_ospeed = lbuf.c_ospeed;
      buf.sg_erase = lbuf.c_cc[VERASE];
      buf.sg_kill = lbuf.c_cc[VKILL];
      buf.sg_flags = 0;	/* FIXME: this is wrong... */

      mem_bcopy(thread_id, mem_fn, mem, Write,
          /* buf */regs->regs_R.dw[MD_REG_EDX], &buf,
          sizeof(struct osf_sgttyb));

      if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
        /* got an error, return details */
        regs->regs_R.dw[MD_REG_EAX] = -errno;

    }
    break;
#endif
#ifdef TIOCGETP
  case OSF_TIOCGETP: /* <Out,TIOCGETP,6> */
    {
      struct sgttyb lbuf;
      struct osf_sgttyb buf;

      /* result */regs->regs_R.dw[MD_REG_EAX] =
        ioctl(/* fd */(int)regs->regs_R.dw[MD_REG_EBX],
            /* req */TIOCGETP,
            &lbuf);

      /* translate results */
      buf.sg_ispeed = lbuf.sg_ispeed;
      buf.sg_ospeed = lbuf.sg_ospeed;
      buf.sg_erase = lbuf.sg_erase;
      buf.sg_kill = lbuf.sg_kill;
      buf.sg_flags = lbuf.sg_flags;
      mem_bcopy(thread_id, mem_fn, mem, Write,
          /* buf */regs->regs_R.dw[MD_REG_EDX], &buf,
          sizeof(struct osf_sgttyb));

      if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
        /* got an error, return details */
        regs->regs_R.dw[MD_REG_EAX] = -errno;
    }
    break;
#endif
#ifdef FIONREAD
  case OSF_FIONREAD:
    {
      int nread;

      /* result */regs->regs_R.dw[MD_REG_EAX] =
        ioctl(/* fd */(int)regs->regs_R.dw[MD_REG_EBX],
            /* req */FIONREAD,
            /* arg */&nread);

      mem_bcopy(thread_id, mem_fn, mem, Write,
          /* arg */regs->regs_R.dw[MD_REG_EDX],
          &nread, sizeof(nread));

      if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
        /* got an error, return details */
        regs->regs_R.dw[MD_REG_EAX] = -errno;
    }
    break;
#endif
#ifdef FIONBIO
  case /*FIXME*/FIONBIO:
    {
      int arg = 0;

      if (regs->regs_R.dw[MD_REG_EDX])
        mem_bcopy(thread_id, mem_fn, mem, Read,
            /* arg */regs->regs_R.dw[MD_REG_EDX],
            &arg, sizeof(arg));

#ifdef NOTNOW
      fprintf(stderr, "FIONBIO: %d, %d\n",
          (int)regs->regs_R.dw[MD_REG_EAX],
          arg);
#endif
      /* result */regs->regs_R.dw[MD_REG_EAX] =
        ioctl(/* fd */(int)regs->regs_R.dw[MD_REG_EBX],
            /* req */FIONBIO,
            /* arg */&arg);

      if (regs->regs_R.dw[MD_REG_EDX])
        mem_bcopy(thread_id, mem_fn, mem, Write,
            /* arg */regs->regs_R.dw[MD_REG_EDX],
            &arg, sizeof(arg));

      if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
        /* got an error, return details */
        regs->regs_R.dw[MD_REG_EAX] = -errno;

    }
    break;
#endif
  default:
    warn("unsupported ioctl call: ioctl(%ld, ...)",
        regs->regs_R.dw[MD_REG_ECX]);
    regs->regs_R.dw[MD_REG_EAX] = 0;
    break;
}
break;

#if 0
{
  char buf[NUM_IOCTL_BYTES];
  int local_req = 0;

  /* convert target ioctl() request to host ioctl() request values */
  switch (/*req*/regs->regs_R.dw[MD_REG_ECX]) {
    /* #if !defined(__CYGWIN32__) */
    case SS_IOCTL_TIOCGETP:
      local_req = TIOCGETP;
      break;
    case SS_IOCTL_TIOCSETP:
      local_req = TIOCSETP;
      break;
    case SS_IOCTL_TCGETP:
      local_req = TIOCGETP;
      break;
      /* #endif */
#ifdef TCGETA
    case SS_IOCTL_TCGETA:
      local_req = TCGETA;
      break;
#endif
#ifdef TIOCGLTC
    case SS_IOCTL_TIOCGLTC:
      local_req = TIOCGLTC;
      break;
#endif
#ifdef TIOCSLTC
    case SS_IOCTL_TIOCSLTC:
      local_req = TIOCSLTC;
      break;
#endif
    case SS_IOCTL_TIOCGWINSZ:
      local_req = TIOCGWINSZ;
      break;
#ifdef TCSETAW
    case SS_IOCTL_TCSETAW:
      local_req = TCSETAW;
      break;
#endif
#ifdef TIOCGETC
    case SS_IOCTL_TIOCGETC:
      local_req = TIOCGETC;
      break;
#endif
#ifdef TIOCSETC
    case SS_IOCTL_TIOCSETC:
      local_req = TIOCSETC;
      break;
#endif
#ifdef TIOCLBIC
    case SS_IOCTL_TIOCLBIC:
      local_req = TIOCLBIC;
      break;
#endif
#ifdef TIOCLBIS
    case SS_IOCTL_TIOCLBIS:
      local_req = TIOCLBIS;
      break;
#endif
#ifdef TIOCLGET
    case SS_IOCTL_TIOCLGET:
      local_req = TIOCLGET;
      break;
#endif
#ifdef TIOCLSET
    case SS_IOCTL_TIOCLSET:
      local_req = TIOCLSET;
      break;
#endif
  }

  if (!local_req)
  {
    /* FIXME: could not translate the ioctl() request, just warn user
       and ignore the request */
    warn("syscall: ioctl: ioctl code not supported d=%d, req=%d",
        regs->regs_R.dw[MD_REG_EBX], regs->regs_R.dw[MD_REG_ECX]);
    regs->regs_R.dw[MD_REG_EAX] = 0;
    /*regs->regs_R[7] = 0;*/
  }
  else
  {
    /* ioctl() code was successfully translated to a host code */

    /* if arg ptr exists, copy NUM_IOCTL_BYTES bytes to host mem */
    if (/*argp*/regs->regs_R.dw[MD_REG_EDX] != 0)
      mem_bcopy(thread_id, mem_fn, mem, Read, /*argp*/regs->regs_R.dw[MD_REG_EDX],
          buf, NUM_IOCTL_BYTES);

    /* perform the ioctl() call */
    /*result*/regs->regs_R.dw[MD_REG_EAX] =
      ioctl(/*fd*/regs->regs_R.dw[MD_REG_EBX], local_req, buf);

    /* if arg ptr exists, copy NUM_IOCTL_BYTES bytes from host mem */
    if (/*argp*/regs->regs_R.dw[MD_REG_EDX] != 0)
      mem_bcopy(thread_id, mem_fn, mem, Write, regs->regs_R.dw[MD_REG_EDX],
          buf, NUM_IOCTL_BYTES);

    /* check for an error condition */
    if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
      /* got an error, return details */
      regs->regs_R.dw[MD_REG_EAX] = -errno;
  }
}
break;
#endif

case X86_SYS_setitimer:
/* FIXME: the sigvec system call is ignored */
warn("syscall: setitimer ignored");
regs->regs_R.dw[MD_REG_EAX] = 0;
break;

case X86_SYS_dup:
/* dup() the file descriptor */
regs->regs_R.dw[MD_REG_EAX] =
dup(/*fd*/regs->regs_R.dw[MD_REG_EBX]);

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;

  case X86_SYS_dup2:
  /* dup2() the file descriptor */
  regs->regs_R.dw[MD_REG_EAX] =
  dup2(/*fd1*/regs->regs_R.dw[MD_REG_EBX], /*fd2*/regs->regs_R.dw[MD_REG_ECX]);

  /* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;

  case X86_SYS_fcntl:
  // this looks right based on linux/fs/fcntl.c:356...
  case X86_SYS_fcntl64:
#ifdef _MSC_VER
  warn("syscall fcntl() not yet implemented for MSC...");
  regs->regs_R.dw[MD_REG_EAX] = 0;
#else /* !_MSC_VER */
  /* get fcntl() information on the file */
  regs->regs_R.dw[MD_REG_EAX] =
  fcntl(/*fd*/regs->regs_R.dw[MD_REG_EBX],
      /*cmd*/regs->regs_R.dw[MD_REG_ECX], /*arg*/regs->regs_R.dw[MD_REG_EDX]);

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
#endif /* _MSC_VER */
  break;

  /*-------------------------------Out of X86-------------------------------------*/
#if 0
  case OSF_SYS_sigvec:
  /* FIXME: the sigvec system call is ignored */
  warn("syscall: sigvec ignored");
  regs->regs_R[MD_REG_A3] = 0;
  break;
#endif

#if 0
  case OSF_SYS_sigblock:
  /* FIXME: the sigblock system call is ignored */
  warn("syscall: sigblock ignored");
  regs->regs_R[MD_REG_A3] = 0;
  break;
#endif

#if 0
  case OSF_SYS_sigsetmask:
  /* FIXME: the sigsetmask system call is ignored */
  warn("syscall: sigsetmask ignored");
  regs->regs_R[MD_REG_A3] = 0;
  break;
#endif
  /*----------------------------------------------------------------------------------*/

case X86_SYS_gettimeofday:
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  mem_bcopy(thread_id, mem_fn, mem, Write, /*argp*/regs->regs_R.dw[MD_REG_EBX], &tv, sizeof(struct timeval));
  regs->regs_R.dw[MD_REG_EAX] = 0;
  break;
}

case X86_SYS_settimeofday:
{
  struct timeval tv;
  mem_bcopy(thread_id, mem_fn, mem, Read, /*argp*/regs->regs_R.dw[MD_REG_EBX], &tv, sizeof(struct timeval));
  int retval = settimeofday(&tv,NULL);
  regs->regs_R.dw[MD_REG_EAX] = retval;
  break;
}

case X86_SYS_set_thread_area:
{
  struct user_desc u_info;
  mem_bcopy(thread_id, mem_fn, mem, Read, /*argp*/regs->regs_R.dw[MD_REG_EBX], &u_info, sizeof(struct user_desc));
  int retval = set_thread_area(&u_info);
  regs->regs_R.dw[MD_REG_EAX] = retval;
  break;
}

case X86_SYS_futex:
{
  warnonce("sys_futex not implemented... winging it");
  regs->regs_R.dw[MD_REG_EAX] = 0;
  break;
}

case X86_SYS_getrusage:
#if defined(__svr4__) || defined(__USLC__) || defined(hpux) || defined(__hpux) || defined(_AIX)
{
  struct tms tms_buf;
  struct osf_rusage rusage;

  /* get user and system times */
  if (times(&tms_buf) != (dword_t)-1)
  {
    /* no error */
    regs->regs_R.dw[MD_REG_EAX] = 0;
  }
  else /* got an error, indicate result */
  {
    regs->regs_R.dw[MD_REG_EAX] = -errno;
  }

  /* initialize target rusage result structure */
#if defined(__svr4__)
  memset(&rusage, '\0', sizeof(struct osf_rusage));
#else /* !defined(__svr4__) */
  bzero(&rusage, sizeof(struct osf_rusage));
#endif

  /* convert from host rusage structure to target format */
  rusage.osf_ru_utime.osf_tv_sec = MD_SWAPD(tms_buf.tms_utime/CLK_TCK);
  rusage.osf_ru_utime.osf_tv_sec =
    MD_SWAPD(rusage.osf_ru_utime.osf_tv_sec);
  rusage.osf_ru_utime.osf_tv_usec = 0;
  rusage.osf_ru_stime.osf_tv_sec = MD_SWAPD(tms_buf.tms_stime/CLK_TCK);
  rusage.osf_ru_stime.osf_tv_sec =
    MD_SWAPD(rusage.osf_ru_stime.osf_tv_sec);
  rusage.osf_ru_stime.osf_tv_usec = 0;

  /* copy rusage results into target memory */
  mem_bcopy(thread_id, mem_fn, mem, Write, /*rusage*/regs->regs_R[MD_REG_A1],
      &rusage, sizeof(struct osf_rusage));
}
#elif defined(__unix__)
{
  struct rusage local_rusage;
  struct osf_rusage rusage;

  /* get rusage information */
  /*result*/regs->regs_R.dw[MD_REG_EAX] =
    getrusage(/*who*/regs->regs_R.dw[MD_REG_EBX], &local_rusage);

  /* check for an error condition */
  if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
    /* got an error, return details */
    regs->regs_R.dw[MD_REG_EAX] = -errno;

  /* convert from host rusage structure to target format */
  rusage.osf_ru_utime.osf_tv_sec =
    MD_SWAPD(local_rusage.ru_utime.tv_sec);
  rusage.osf_ru_utime.osf_tv_usec =
    MD_SWAPD(local_rusage.ru_utime.tv_usec);
  rusage.osf_ru_utime.osf_tv_sec =
    MD_SWAPD(local_rusage.ru_utime.tv_sec);
  rusage.osf_ru_utime.osf_tv_usec =
    MD_SWAPD(local_rusage.ru_utime.tv_usec);
  rusage.osf_ru_stime.osf_tv_sec =
    MD_SWAPD(local_rusage.ru_stime.tv_sec);
  rusage.osf_ru_stime.osf_tv_usec =
    MD_SWAPD(local_rusage.ru_stime.tv_usec);
  rusage.osf_ru_stime.osf_tv_sec =
    MD_SWAPD(local_rusage.ru_stime.tv_sec);
  rusage.osf_ru_stime.osf_tv_usec =
    MD_SWAPD(local_rusage.ru_stime.tv_usec);
  rusage.osf_ru_maxrss = MD_SWAPD(local_rusage.ru_maxrss);
  rusage.osf_ru_ixrss = MD_SWAPD(local_rusage.ru_ixrss);
  rusage.osf_ru_idrss = MD_SWAPD(local_rusage.ru_idrss);
  rusage.osf_ru_isrss = MD_SWAPD(local_rusage.ru_isrss);
  rusage.osf_ru_minflt = MD_SWAPD(local_rusage.ru_minflt);
  rusage.osf_ru_majflt = MD_SWAPD(local_rusage.ru_majflt);
  rusage.osf_ru_nswap = MD_SWAPD(local_rusage.ru_nswap);
  rusage.osf_ru_inblock = MD_SWAPD(local_rusage.ru_inblock);
  rusage.osf_ru_oublock = MD_SWAPD(local_rusage.ru_oublock);
  rusage.osf_ru_msgsnd = MD_SWAPD(local_rusage.ru_msgsnd);
  rusage.osf_ru_msgrcv = MD_SWAPD(local_rusage.ru_msgrcv);
  rusage.osf_ru_nsignals = MD_SWAPD(local_rusage.ru_nsignals);
  rusage.osf_ru_nvcsw = MD_SWAPD(local_rusage.ru_nvcsw);
  rusage.osf_ru_nivcsw = MD_SWAPD(local_rusage.ru_nivcsw);

  /* copy rusage results into target memory */
  mem_bcopy(thread_id, mem_fn, mem, Write, /*rusage*/regs->regs_R.dw[MD_REG_ECX],
      &rusage, sizeof(struct osf_rusage));
}
#elif defined(__CYGWIN32__) || defined(_MSC_VER)
warn("syscall: called getrusage\n");
regs->regs_R[7] = 0;
#else
#error No getrusage() implementation!
#endif
break;

case X86_SYS_getrlimit:
case X86_SYS_setrlimit:
#ifdef _MSC_VER
warn("syscall get/setrlimit() not yet implemented for MSC...");
regs->regs_R.dw[MD_REG_EAX] = 0;
#else
{
  warn("syscall: called get/setrlimit\n");
  regs->regs_R.dw[MD_REG_EAX] = 0;
}
#endif
break;

case X86_SYS_mprotect:
{
  int error = 0;

  /* apply protection to any page touched by the range
     given by len...len+prot (must do this on a page by
     page basis to deal with virtual->host translation) */
  md_addr_t vaddr = regs->regs_R.dw[MD_REG_EBX];
  int len = regs->regs_R.dw[MD_REG_ECX];
  int prot = regs->regs_R.dw[MD_REG_EDX];

  md_addr_t start_page = vaddr & ~(MD_PAGE_SIZE-1);
  md_addr_t end_page = (vaddr+len) & ~(MD_PAGE_SIZE-1);
  md_addr_t current_page;

  for(current_page = start_page;
      current_page <= end_page;
      current_page += MD_PAGE_SIZE)
  {
    md_addr_t hostaddr = (md_addr_t) MEM_ADDR2HOST(mem, current_page);
    int retval = mprotect((void*)hostaddr,MD_PAGE_SIZE,prot);
    if(retval)
      error = retval;
  }

  regs->regs_R.dw[MD_REG_EAX] = error;

  break;
}

case X86_SYS_sigprocmask:
{
  static int first = TRUE;

  if (first)
  {
    warn("partially supported sigprocmask() call...");
    first = FALSE;
  }

  /* from klauser@cs.colorado.edu: there are a couple bugs in the
     sigprocmask implementation; here is a fix: the problem comes from an
     impedance mismatch between application/libc interface and
     libc/syscall interface, the former of which you read about in the
     manpage, the latter of which you actually intercept here.  The
     following is mostly correct, but does not capture some minor
     details, which you only get right if you really let the kernel
     handle it. (e.g. you can't really ever block sigkill etc.) */

  /*regs->regs_R[MD_REG_V0] = sigmask;*/

  switch (regs->regs_R.dw[MD_REG_EAX])
  {
    case OSF_SIG_BLOCK:
      sigmask[thread_id] |= regs->regs_R.dw[MD_REG_ECX];
      break;
    case OSF_SIG_UNBLOCK:
      sigmask[thread_id] &= ~regs->regs_R.dw[MD_REG_ECX];
      break;
    case OSF_SIG_SETMASK:
      sigmask[thread_id] = regs->regs_R.dw[MD_REG_ECX];
      break;
    default:
      regs->regs_R.dw[MD_REG_EAX] = -EINVAL;

  }

#if 0 /* FIXME: obsolete... */
  if (regs->regs_R.dw[MD_REG_EDX] > /* FIXME: why? */0x10000000)
    mem_bcopy(thread_id, mem_fn, mem, Write, regs->regs_R.dw[MD_REG_EDX],
        &sigmask, sizeof(sigmask));

  if (regs->regs_R.dw[MD_REG_ECX] != 0)
  {
    switch (regs->regs_R.dw[MD_REG_EBX])
    {
      case OSF_SIG_BLOCK:
        sigmask |= regs->regs_R.dw[MD_REG_ECX];
        break;
      case OSF_SIG_UNBLOCK:
        sigmask &= regs->regs_R.dw[MD_REG_ECX]; /* I think */
        break;
      case OSF_SIG_SETMASK:
        sigmask = regs->regs_R.dw[MD_REG_ECX]; /* I think */
        break;
      default:
        panic("illegal how value to sigprocmask()");
    }
  }
  regs->regs_R.dw[MD_REG_EAX] = 0;

#endif
}
break;

case X86_SYS_sigaction:
{
  int signum;
  static int first = TRUE;

  if (first)
  {
    warn("partially supported sigaction() call...");
    first = FALSE;
  }

  signum = regs->regs_R.dw[MD_REG_EBX];
  if (regs->regs_R.dw[MD_REG_ECX] != 0)
    sigaction_array[thread_id][signum] = regs->regs_R.dw[MD_REG_ECX];

  if (regs->regs_R.dw[MD_REG_EDX])
    regs->regs_R.dw[MD_REG_EDX] = sigaction_array[thread_id][signum];

  regs->regs_R.dw[MD_REG_EAX] = 0;

  /* for some reason, __sigaction expects A3 to have a 0 return value 
     regs->regs_R[MD_REG_A3] = 0; */

  /* FIXME: still need to add code so that on a signal, the 
     correct action is actually taken. */

  /* FIXME: still need to add support for returning the correct
     error messages (EFAULT, EINVAL) */
}
break;

#if 0
case X86_SYS_sigreturn:
{
  mem_bcopy(thread_id, mem_fn, mem, Read, /* sc */regs->regs_R.dw[MD_REG_EBX],
      &sc, sizeof(struct osf_sigcontext));

  sigmask[thread_id] = MD_SWAPQ(sc.sc_mask); /* was: prog_sigmask */
  regs->regs_NPC = MD_SWAPQ(sc.sc_pc);

  /* FIXME: should check for the branch delay bit */
  /* FIXME: current->nextpc = current->pc + 4; not sure about this... */
  for (i=0; i < 32; ++i)
    regs->regs_R[i] = sc.sc_regs[i];
  for (i=0; i < 32; ++i)
    regs->regs_F.q[i] = sc.sc_fpregs[i];
  regs->regs_C.fpcr = sc.sc_fpcr;
}
break;
#endif

/* Is this in arm??? ----*/
#if !defined(MIN_SYSCALL_MODE) && 0 
case OSF_SYS_getdirentries:
{
  int i, cnt, osf_cnt;
  struct dirent *p;
  sdword_t fd = regs->regs_R[MD_REG_A0];
  md_addr_t osf_buf = regs->regs_R[MD_REG_A1];
  char *buf;
  sdword_t osf_nbytes = regs->regs_R[MD_REG_A2];
  md_addr_t osf_pbase = regs->regs_R[MD_REG_A3];
  sqword_t osf_base;
  long base = 0;

  /* number of entries in simulated memory */
  if (!osf_nbytes)
    warn("attempting to get 0 directory entries...");

  /* allocate local memory, whatever fits */
  buf = calloc(1, osf_nbytes);
  if (!buf)
    fatal("out of virtual memory");

  /* get directory entries */
#if defined(__svr4__)
  base = lseek ((int)fd, (off_t)0, SEEK_CUR);
  regs->regs_R[MD_REG_V0] =
    getdents((int)fd, (struct dirent *)buf, (size_t)osf_nbytes);
#else /* !__svr4__ */
  regs->regs_R[MD_REG_V0] =
    getdirentries((int)fd, buf, (size_t)osf_nbytes, &base);
#endif

  /* check for an error condition */
  if (regs->regs_R[MD_REG_V0] != (dword_t)-1)
  {
    regs->regs_R[MD_REG_A3] = 0;

    /* anything to copy back? */
    if (regs->regs_R[MD_REG_V0] > 0)
    {
      /* copy all possible results to simulated space */
      for (i=0, cnt=0, osf_cnt=0, p=(struct dirent *)buf;
          cnt < regs->regs_R[MD_REG_V0] && p->d_reclen > 0;
          i++, cnt += p->d_reclen, p=(struct dirent *)(buf+cnt))
      {
        struct osf_dirent osf_dirent;

        osf_dirent.d_ino = MD_SWAPD(p->d_ino);
        osf_dirent.d_namlen = MD_SWAPW(strlen(p->d_name));
        strcpy(osf_dirent.d_name, p->d_name);
        osf_dirent.d_reclen = MD_SWAPW(OSF_DIRENT_SZ(p->d_name));

        mem_bcopy(thread_id, mem_fn, mem, Write,
            osf_buf + osf_cnt,
            &osf_dirent, OSF_DIRENT_SZ(p->d_name));
        osf_cnt += OSF_DIRENT_SZ(p->d_name);
      }

      if (osf_pbase != 0)
      {
        osf_base = (sqword_t)base;
        mem_bcopy(thread_id, mem_fn, mem, Write, osf_pbase,
            &osf_base, sizeof(osf_base));
      }

      /* update V0 to indicate translated read length */
      regs->regs_R[MD_REG_V0] = osf_cnt;
    }
  }
  else /* got an error, return details */
  {
    regs->regs_R[MD_REG_A3] = -1;
    regs->regs_R[MD_REG_V0] = errno;
  }

  free(buf);
}
break;
#endif
/*-----------------------------------------------------------------------------------------*/


#if !defined(MIN_SYSCALL_MODE)
case X86_SYS_truncate: /*find out what the 64 is for */
{
  char buf[MAXBUFSIZE];

  /* copy filename to host memory */
  mem_strcpy(thread_id, mem_fn, mem, Read, /*fname*/regs->regs_R.dw[MD_REG_EBX], buf);

  /* truncate the file */
  /*result*/regs->regs_R.dw[MD_REG_EAX] =
    truncate(buf, /* length */(size_t)regs->regs_R.dw[MD_REG_ECX]);

  /* check for an error condition */
  if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
    /* got an error, return details */
    regs->regs_R.dw[MD_REG_EAX] = -errno;
}
break;
#endif

#if !defined(__CYGWIN32__) && !defined(_MSC_VER)
case  X86_SYS_ftruncate64:// skumar - FIXME need to implement ftruncate64 more accurately 
case X86_SYS_ftruncate:
/* truncate the file */
/*result*/regs->regs_R.dw[MD_REG_EAX] =
ftruncate(/* fd */(int)regs->regs_R.dw[MD_REG_EBX],
    /* length */(size_t)regs->regs_R.dw[MD_REG_ECX]);

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;
#endif

#if !defined(MIN_SYSCALL_MODE)
  case X86_SYS_statfs:
{
  char buf[MAXBUFSIZE];
  struct osf_statfs osf_sbuf;
  struct statfs sbuf;

  /* copy filename to host memory */
  mem_strcpy(thread_id, mem_fn, mem, Read, /*fName*/regs->regs_R.dw[MD_REG_EBX], buf);

  /* statfs() the fs */
  /*result*/regs->regs_R.dw[MD_REG_EAX] = statfs(buf, &sbuf);

  /* check for an error condition */
  if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
    /* got an error, return details */
    regs->regs_R.dw[MD_REG_EAX] = -errno;

  /* translate from host stat structure to target format */
#if defined(__svr4__) || defined(__osf__)
  osf_sbuf.f_type = MD_SWAPD(0x6969) /* NFS, whatever... */;
#else /* !__svr4__ */
  osf_sbuf.f_type = MD_SWAPD(sbuf.f_type);
#endif
  osf_sbuf.f_bsize = MD_SWAPD(sbuf.f_bsize);
  osf_sbuf.f_blocks = MD_SWAPD(sbuf.f_blocks);
  osf_sbuf.f_bfree = MD_SWAPD(sbuf.f_bfree);
  osf_sbuf.f_bavail = MD_SWAPD(sbuf.f_bavail);
  osf_sbuf.f_files = MD_SWAPD(sbuf.f_files);
  osf_sbuf.f_ffree = MD_SWAPD(sbuf.f_ffree);
  /* osf_sbuf.f_fsid = MD_SWAPD(sbuf.f_fsid); */


  /* copy stat() results to simulator memory */

  mem_bcopy(thread_id, mem_fn, mem, Write, /*sbuf*/regs->regs_R.dw[MD_REG_ECX],		           &osf_sbuf, sizeof(struct osf_statfs)); 

  /*changed osf_statbuf to osf_statfs for arm? */

}
break;
#endif



#if !defined(MIN_SYSCALL_MODE)
case X86_SYS_setregid:
/* set real and effective group ID */

/*result*/regs->regs_R.dw[MD_REG_EAX] =
setregid(/* rgid */(gid_t)regs->regs_R.dw[MD_REG_EBX],
    /* egid */(gid_t)regs->regs_R.dw[MD_REG_ECX]);

fprintf(stderr,"Why??");
/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;
#endif

#if !defined(MIN_SYSCALL_MODE)
  //    case OSF_SYS_setreuid:
  case X86_SYS_setreuid:
  /* set real and effective user ID */

  /*result*/regs->regs_R.dw[MD_REG_EAX] =
  setreuid(/* ruid */(uid_t)regs->regs_R.dw[MD_REG_EBX],
      /* euid */(uid_t)regs->regs_R.dw[MD_REG_ECX]);

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;
#endif

  /* this is a subclass of the socketcall in arm */
#if !defined(MIN_SYSCALL_MODE) && 0
  case X86_SYS_socket:
  int __domain,__type,__protocol;

  /* grab the socket call arguments from simulated memory */
  mem_bcopy(thread_id, mem_fn, mem, Read, regs->regs_R.dw[MD_REG_ECX], &__domain,
      /*nbytes*/sizeof(int));
mem_bcopy(thread_id, mem_fn, mem, Read, (regs->regs_R.dw[MD_REG_ECX]+sizeof(int)), &__type,
    /*nbytes*/sizeof(int));
mem_bcopy(thread_id, mem_fn, mem, Read, (regs->regs_R.dw[MD_REG_ECX]+2*sizeof(int)), &__protocol,
    /*nbytes*/sizeof(int));

/* create an endpoint for communication */

/* result */regs->regs_R.dw[MD_REG_EAX] =
socket(/* domain */xlate_arg(__domain,
      family_map, N_ELT(family_map),
      "socket(family)"),
    /* type */xlate_arg(__type,
      socktype_map, N_ELT(socktype_map),
      "socket(type)"),
    /* protocol */xlate_arg(__protocol,
      family_map, N_ELT(family_map),
      "socket(proto)"));

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;
#endif
  /* This is a subclass of the socketcall in arm linux */
#if !defined(MIN_SYSCALL_MODE) && 0
  case X86_SYS_connect:
{
  struct osf_sockaddr osf_sa;

  /* initiate a connection on a socket */

  /* get the socket address */
  if (regs->regs_R.dw[MD_REG_EDX] > sizeof(struct osf_sockaddr))
  {
    fatal("sockaddr size overflow: addrlen = %d",
        regs->regs_R.dw[MD_REG_EDX]);
  }
  /* copy sockaddr structure to host memory */
  mem_bcopy(thread_id, mem_fn, mem, Read, /* serv_addr */regs->regs_R.dw[MD_REG_ECX],
      &osf_sa, /* addrlen */(int)regs->regs_R.dw[MD_REG_EDX]);
#if 0
  int i;
  sa.sa_family = osf_sa.sa_family;
  for (i=0; i < regs->regs_R.dw[MD_REG_EDX]; i++)
    sa.sa_data[i] = osf_sa.sa_data[i];
#endif
  /* result */regs->regs_R.dw[MD_REG_EAX] =
    connect(/* sockfd */(int)regs->regs_R.dw[MD_REG_EBX],
        (void *)&osf_sa, /* addrlen */(int)regs->regs_R.dw[MD_REG_EDX]);

  /* check for an error condition */
  if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
    /* got an error, return details */
    regs->regs_R.dw[MD_REG_EAX] = -errno;
}
break;
#endif

/* Should olduname and oldolduname be supported ?? ctw*/
case X86_SYS_uname:
/* get name and information about current kernel */
{
  struct utsname local_name;

  int retval = uname( &local_name );
  if (retval == -1)
  {
    warn( "uname failed!" );
    perror( "here's why" );
    regs->regs_R.dw[MD_REG_EAX] = -(errno);
  }
  else
  {
    mem_bcopy(thread_id,  mem_fn, mem, Write, 
        regs->regs_R.dw[MD_REG_EBX], &local_name,
        sizeof( local_name ) );
    regs->regs_R.dw[MD_REG_EAX] = retval;
  }
}
break;

case X86_SYS_writev:
{
  int i;
  int error_return;
  char *buf;
  struct iovec *iov;

  /* allocate host side I/O vectors */
  iov = (struct iovec *)calloc(/* iovcnt */regs->regs_R.dw[MD_REG_EDX],sizeof(struct iovec));
  if (!iov)
    fatal("out of virtual memory in SYS_writev");

  /* copy target side I/O vector buffers to host memory */
  for (i=0; i < /* iovcnt */regs->regs_R.dw[MD_REG_EDX]; i++)
  {
    struct osf_iovec osf_iov;

    /* copy target side pointer data into host side vector */
    mem_bcopy(thread_id, mem_fn, mem, Read,
        (/*iov*/regs->regs_R.dw[MD_REG_ECX]
         + i*sizeof(struct iovec)),
        &osf_iov, sizeof(struct iovec));

    iov[i].iov_len = MD_SWAPD(osf_iov.iov_len);
    if (osf_iov.iov_base != 0 && osf_iov.iov_len != 0)
    {
      buf = (char *)calloc(MD_SWAPD(osf_iov.iov_len), sizeof(char));
      if (!buf)
        fatal("out of virtual memory in SYS_writev");
      mem_bcopy(thread_id, mem_fn, mem, Read, MD_SWAPQ(osf_iov.iov_base),
          buf, MD_SWAPD(osf_iov.iov_len));
      iov[i].iov_base = buf;
    }
    else
      iov[i].iov_base = NULL;
  }

  /* perform the vector'ed write */
  do {
    /*result*/error_return =
      writev(/* fd */(int)regs->regs_R.dw[MD_REG_EBX], iov,
          /* iovcnt */(size_t)regs->regs_R.dw[MD_REG_EDX]);
  } while (/*result*/error_return == -1
      && errno == EAGAIN);

  /* check for an error condition */
  if (error_return != -1) {
    regs->regs_R.dw[MD_REG_EAX] = error_return;
  } /* no error */
  else {
    regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
  }

  /* free all the allocated memory */
  for (i=0; i < /* iovcnt */regs->regs_R.dw[MD_REG_EDX]; i++)
  {
    if (iov[i].iov_base)
    {
      free(iov[i].iov_base);
      iov[i].iov_base = NULL;
    }
  }
  free(iov);
}
break;

case X86_SYS_readv:
{
  int i;
  char *buf = NULL;
  struct osf_iovec *osf_iov;
  struct iovec *iov;
  int error_return;

  /* allocate host side I/O vectors */
  osf_iov =
    calloc(/* iovcnt */regs->regs_R.dw[MD_REG_EDX],
        sizeof(struct osf_iovec));
  if (!osf_iov)
    fatal("out of virtual memory in SYS_readv");

  iov =
    calloc(/* iovcnt */regs->regs_R.dw[MD_REG_EDX], sizeof(struct iovec));
  if (!iov)
    fatal("out of virtual memory in SYS_readv");

  /* copy host side I/O vector buffers */
  for (i=0; i < /* iovcnt */regs->regs_R.dw[MD_REG_EDX]; i++)
  {
    /* copy target side pointer data into host side vector */
    mem_bcopy(thread_id, mem_fn, mem, Read,
        (/*iov*/regs->regs_R.dw[MD_REG_ECX]
         + i*sizeof(struct osf_iovec)),
        &osf_iov[i], sizeof(struct osf_iovec));

    iov[i].iov_len = MD_SWAPD(osf_iov[i].iov_len);
    if (osf_iov[i].iov_base != 0 && osf_iov[i].iov_len != 0)
    {
      buf =
        (char *)calloc(MD_SWAPD(osf_iov[i].iov_len), sizeof(char));
      if (!buf)
        fatal("out of virtual memory in SYS_readv");
      iov[i].iov_base = buf;
    }
    else
      iov[i].iov_base = NULL;
  }

  /* perform the vector'ed read */
  do {
    /*result*/error_return =
      readv(/* fd */(int)regs->regs_R.dw[MD_REG_EBX], iov,
          /* iovcnt */(size_t)regs->regs_R.dw[MD_REG_EDX]);
  } while (/*result*/error_return == -1
      && errno == EAGAIN);

  /* copy target side I/O vector buffers to host memory */
  for (i=0; i < /* iovcnt */regs->regs_R.dw[MD_REG_EDX]; i++)
  {
    if (osf_iov[i].iov_base != 0)
    {
      mem_bcopy(thread_id, mem_fn, mem, Write, MD_SWAPQ(osf_iov[i].iov_base),
          iov[i].iov_base, MD_SWAPD(osf_iov[i].iov_len));
    }
  }

  /* check for an error condition */
  if (error_return != -1) {
    regs->regs_R.dw[MD_REG_EAX] = error_return;
  } /* no error */
  else {
    regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
  }

  /* free all the allocated memory */
  for (i=0; i < /* iovcnt */regs->regs_R.dw[MD_REG_EDX]; i++)
  {
    if (iov[i].iov_base)
    {
      free(iov[i].iov_base);
      iov[i].iov_base = NULL;
    }
  }

  if (osf_iov)
    free(osf_iov);
  if (iov)
    free(iov);
}
break;

/*------------Subcall of socketcall-------------------------------*/
#if !defined(MIN_SYSCALL_MODE) && 0
case OSF_SYS_setsockopt:
{
  char *buf;
  struct xlate_table_t *map;
  int nmap;

  /* set options on sockets */

  /* copy optval to host memory */
  if (/* optval */regs->regs_R[MD_REG_A3] != 0
      && /* optlen */regs->regs_R[MD_REG_A4] != 0)
  {
    buf = calloc(1, /* optlen */(size_t)regs->regs_R[MD_REG_A4]);
    if (!buf)
      fatal("cannot allocate memory in OSF_SYS_setsockopt");

    /* copy target side pointer data into host side vector */
    mem_bcopy(thread_id, mem_fn, mem, Read,
        /* optval */regs->regs_R[MD_REG_A3],
        buf, /* optlen */(int)regs->regs_R[MD_REG_A4]);
  }
  else
    buf = NULL;

  /* pick the correct translation table */
  if ((int)regs->regs_R[MD_REG_A1] == OSF_SOL_SOCKET)
  {
    map = sockopt_map;
    nmap = N_ELT(sockopt_map);
  }
  else if ((int)regs->regs_R[MD_REG_A1] == OSF_SOL_TCP)
  {
    map = tcpopt_map;
    nmap = N_ELT(tcpopt_map);
  }
  else
  {
    warn("no translation map available for `setsockopt()': %d",
        (int)regs->regs_R[MD_REG_A1]);
    map = sockopt_map;
    nmap = N_ELT(sockopt_map);
  }

  /* result */regs->regs_R[MD_REG_V0] =
    setsockopt(/* sock */(int)regs->regs_R[MD_REG_A0],
        /* level */xlate_arg((int)regs->regs_R[MD_REG_A1],
          socklevel_map, N_ELT(socklevel_map),
          "setsockopt(level)"),
        /* optname */xlate_arg((int)regs->regs_R[MD_REG_A2],
          map, nmap,
          "setsockopt(opt)"),
        /* optval */buf,
        /* optlen */regs->regs_R[MD_REG_A4]);

  /* check for an error condition */
  if (regs->regs_R[MD_REG_V0] != (dword_t)-1)
    regs->regs_R[MD_REG_A3] = 0;
  else /* got an error, return details */
  {
    regs->regs_R[MD_REG_A3] = -1;
    regs->regs_R[MD_REG_V0] = errno;
  }

  if (buf != NULL)
    free(buf);
}
break;
#endif

/* subcall of socketcall in arm linux */
#if !defined(MIN_SYSCALL_MODE) && 0
case OSF_SYS_old_getsockname:
{
  /* get socket name */
  char *buf;
  dword_t osf_addrlen;
  int addrlen;

  /* get simulator memory parameters to host memory */
  mem_bcopy(thread_id, mem_fn, mem, Read,
      /* paddrlen */regs->regs_R[MD_REG_A2],
      &osf_addrlen, sizeof(osf_addrlen));
  addrlen = (int)osf_addrlen;
  if (addrlen != 0)
  {
    buf = calloc(1, addrlen);
    if (!buf)
      fatal("cannot allocate memory in OSF_SYS_old_getsockname");
  }
  else
    buf = NULL;

  /* result */regs->regs_R[MD_REG_V0] =
    getsockname(/* sock */(int)regs->regs_R[MD_REG_A0],
        /* name */(struct sockaddr *)buf,
        /* namelen */&addrlen);

  /* check for an error condition */
  if (regs->regs_R[MD_REG_V0] != (dword_t)-1)
    regs->regs_R[MD_REG_A3] = 0;
  else /* got an error, return details */
  {
    regs->regs_R[MD_REG_A3] = -1;
    regs->regs_R[MD_REG_V0] = errno;
  }

  /* copy results to simulator memory */
  if (addrlen != 0)
    mem_bcopy(thread_id, mem_fn, mem, Write,
        /* addr */regs->regs_R[MD_REG_A1],
        buf, addrlen);
  osf_addrlen = (qword_t)addrlen;
  mem_bcopy(thread_id, mem_fn, mem, Write,
      /* paddrlen */regs->regs_R[MD_REG_A2],
      &osf_addrlen, sizeof(osf_addrlen));

  if (buf != NULL)
    free(buf);
}
break;
#endif

/* part socketcall in arm linux */
#if !defined(MIN_SYSCALL_MODE) && 0
case OSF_SYS_old_getpeername:
{
  /* get socket name */
  char *buf;
  dword_t osf_addrlen;
  int addrlen;

  /* get simulator memory parameters to host memory */
  mem_bcopy(thread_id, mem_fn, mem, Read,
      /* paddrlen */regs->regs_R[MD_REG_A2],
      &osf_addrlen, sizeof(osf_addrlen));
  addrlen = (int)osf_addrlen;
  if (addrlen != 0)
  {
    buf = calloc(1, addrlen);
    if (!buf)
      fatal("cannot allocate memory in OSF_SYS_old_getsockname");
  }
  else
    buf = NULL;

  /* result */regs->regs_R[MD_REG_V0] =
    getpeername(/* sock */(int)regs->regs_R[MD_REG_A0],
        /* name */(struct sockaddr *)buf,
        /* namelen */&addrlen);

  /* check for an error condition */
  if (regs->regs_R[MD_REG_V0] != (dword_t)-1)
    regs->regs_R[MD_REG_A3] = 0;
  else /* got an error, return details */
  {
    regs->regs_R[MD_REG_A3] = -1;
    regs->regs_R[MD_REG_V0] = errno;
  }

  /* copy results to simulator memory */
  if (addrlen != 0)
    mem_bcopy(thread_id, mem_fn, mem, Write,
        /* addr */regs->regs_R[MD_REG_A1],
        buf, addrlen);
  osf_addrlen = (qword_t)addrlen;
  mem_bcopy(thread_id, mem_fn, mem, Write,
      /* paddrlen */regs->regs_R[MD_REG_A2],
      &osf_addrlen, sizeof(osf_addrlen));

  if (buf != NULL)
    free(buf);
}
break;
#endif
/*-----------------------------------------------------------------------------------*/

#if !defined(MIN_SYSCALL_MODE)
case X86_SYS_setgid:
/* set group ID */

/*result*/regs->regs_R.dw[MD_REG_EAX] =
setgid(/* gid */(gid_t)regs->regs_R.dw[MD_REG_EBX]);

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;
#endif

#if !defined(MIN_SYSCALL_MODE)
  case X86_SYS_setuid:
  /* set user ID */

  /*result*/regs->regs_R.dw[MD_REG_EAX] =
  setuid(/* uid */(uid_t)regs->regs_R.dw[MD_REG_EBX]);

  /* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;
#endif

#if !defined(MIN_SYSCALL_MODE)
  case X86_SYS_getpriority:
  /* get program scheduling priority */

  /*result*/regs->regs_R.dw[MD_REG_EAX] =
  getpriority(/* which */(int)regs->regs_R.dw[MD_REG_EBX],
      /* who */(int)regs->regs_R.dw[MD_REG_ECX]);

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;
#endif

#if !defined(MIN_SYSCALL_MODE)
  case X86_SYS_setpriority:
  /* set program scheduling priority */

  /*result*/regs->regs_R.dw[MD_REG_EAX] =
  setpriority(/* which */(int)regs->regs_R.dw[MD_REG_EBX],
      /* who */(int)regs->regs_R.dw[MD_REG_ECX],
      /* prio */(int)regs->regs_R.dw[MD_REG_EDX]);

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;
#endif

  case X86_SYS_getdents:
  case X86_SYS_getdents64:
{
  unsigned int fd;
  unsigned int count;
  long return_val;

  fd = (unsigned int) regs->regs_R.dw[MD_REG_EBX];
  count = (unsigned int) regs->regs_R.dw[MD_REG_EDX];

  void *local_buf = calloc( 1,count );

  if (syscode == X86_SYS_getdents)
    return_val = getdents( fd, local_buf, count );
  else
    return_val = getdents64( fd, local_buf, count );
  if (return_val > 0) mem_bcopy(thread_id,  mem_fn, mem, Write, (md_addr_t) regs->regs_R.dw[MD_REG_ECX], local_buf, count );
  regs->regs_R.dw[MD_REG_EAX] = return_val;
  break;
}

case X86_SYS_llseek:
{
  loff_t result;
  int rval;

  rval = _llseek((int)regs->regs_R.dw[MD_REG_EBX],
      (unsigned int) regs->regs_R.dw[MD_REG_ECX],
      (unsigned long) regs->regs_R.dw[MD_REG_EDX],
      &result,
      (unsigned int) regs->regs_R.dw[MD_REG_EDI]);

  mem_bcopy(thread_id, mem_fn, 
      mem,
      Write,
      (md_addr_t) regs->regs_R.dw[MD_REG_ESI],
      (void*) &result,
      sizeof(loff_t));
  regs->regs_R.dw[MD_REG_EAX]=rval;
  break;
}

case X86_SYS_select:
{
  int result;
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;
  struct timeval timeout;

  mem_bcopy(thread_id, mem_fn, 
      mem,
      Read,
      (md_addr_t) regs->regs_R.dw[MD_REG_ECX],
      (void*) &readfds,
      sizeof(fd_set));
  mem_bcopy(thread_id, mem_fn, 
      mem,
      Read,
      (md_addr_t) regs->regs_R.dw[MD_REG_EDX],
      (void*) &writefds,
      sizeof(fd_set));
  mem_bcopy(thread_id, mem_fn, 
      mem,
      Read,
      (md_addr_t) regs->regs_R.dw[MD_REG_ESI],
      (void*) &exceptfds,
      sizeof(fd_set));
  mem_bcopy(thread_id, mem_fn, 
      mem,
      Read,
      (md_addr_t) regs->regs_R.dw[MD_REG_EDI],
      (void*) &timeout,
      sizeof(struct timeval));
  result = select(regs->regs_R.dw[MD_REG_EBX],
      &readfds,
      &writefds,
      &exceptfds,
      &timeout
      );
#if 0          
  result = select(regs->regs_R.dw[MD_REG_EBX],
      (fd_set*) regs->regs_R.dw[MD_REG_ECX],
      (fd_set*) regs->regs_R.dw[MD_REG_EDX], 
      (fd_set*) regs->regs_R.dw[MD_REG_ESI], 
      (struct timeval*) regs->regs_R.dw[MD_REG_EDI]
      );
#endif
  regs->regs_R.dw[MD_REG_EAX] = result;

  break;
}
/* appears to be unimplemented below  */
#if 0
#if !defined(MIN_SYSCALL_MODE)
case X86_SYS_select:
{
  fd_set readfd, writefd, exceptfd;
  fd_set *readfdp, *writefdp, *exceptfdp;
  struct timeval timeout, *timeoutp;

  /* copy read file descriptor set into host memory */
  if (/* readfds */regs->regs_R.dw[MD_REG_ECX] != 0)
  {
    mem_bcopy(thread_id, mem_fn, mem, Read,
        /* readfds */regs->regs_R.dw[MD_REG_ECX],
        &readfd, sizeof(fd_set));
    readfdp = &readfd;
  }
  else
    readfdp = NULL;

  /* copy write file descriptor set into host memory */
  if (/* writefds */regs->regs_R.dw[MD_REG_EDX] != 0)
  {
    mem_bcopy(thread_id, mem_fn, mem, Read,
        /* writefds */regs->regs_R.dw[MD_REG_EDX],
        &writefd, sizeof(fd_set));
    writefdp = &writefd;
  }
  else
    writefdp = NULL;

  /* copy exception file descriptor set into host memory */
  if (/* exceptfds */regs->regs_R.dw[MD_REG_EDXx] != 0)
  {
    mem_bcopy(thread_id, mem_fn, mem, Read,
        /* exceptfds */regs->regs_R.dw[MD_REG_EDXx],
        &exceptfd, sizeof(fd_set));
    exceptfdp = &exceptfd;
  }
  else
    exceptfdp = NULL;

  /* copy timeout value into host memory */
  if (/* timeout */regs->regs_R[MD_REG_R4] != 0)
  {
    mem_bcopy(thread_id, mem_fn, mem, Read,
        /* timeout */regs->regs_R[MD_REG_R4],
        &timeout, sizeof(struct timeval));
    timeoutp = &timeout;
  }
  else
    timeoutp = NULL;

#if defined(hpux) || defined(__hpux)
  /* select() on the specified file descriptors */
  /* result */regs->regs_R.dw[MD_REG_EAX] =
    select(/* nfds */regs->regs_R.dw[MD_REG_EBX],
        (int *)readfdp, (int *)writefdp, (int *)exceptfdp, timeoutp);
#else
  /* select() on the specified file descriptors */
  /* result */regs->regs_R.dw[MD_REG_EAX] =
    select(/* nfds */regs->regs_R.dw[MD_REG_EBX],
        readfdp, writefdp, exceptfdp, timeoutp);
#endif

  /* check for an error condition */
  if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
    /* got an error, return details */
    regs->regs_R.dw[MD_REG_EAX] = -errno;

  /* copy read file descriptor set to target memory */
  if (/* readfds */regs->regs_R.dw[MD_REG_ECX] != 0)
    mem_bcopy(thread_id, mem_fn, mem, Write,
        /* readfds */regs->regs_R.dw[MD_REG_ECX],
        &readfd, sizeof(fd_set));

  /* copy write file descriptor set to target memory */
  if (/* writefds */regs->regs_R.dw[MD_REG_EDX] != 0)
    mem_bcopy(thread_id, mem_fn, mem, Write,
        /* writefds */regs->regs_R.dw[MD_REG_EDX],
        &writefd, sizeof(fd_set));

  /* copy exception file descriptor set to target memory */
  if (/* exceptfds */regs->regs_R.dw[MD_REG_EDXx] != 0)
    mem_bcopy(thread_id, mem_fn, mem, Write,
        /* exceptfds */regs->regs_R.dw[MD_REG_EDXx],
        &exceptfd, sizeof(fd_set));

  /* copy timeout value result to target memory */
  if (/* timeout */regs->regs_R[MD_REG_R4] != 0)
    mem_bcopy(thread_id, mem_fn, mem, Write,
        /* timeout */regs->regs_R[MD_REG_R4],
        &timeout, sizeof(struct timeval));
}
break;
#endif
#endif
/* part of socketcall in arm linux */
#if !defined(MIN_SYSCALL_MODE) && 0
case X86_SYS_shutdown:
/* shuts down socket send and receive operations */

/*result*/regs->regs_R.dw[MD_REG_EAX] =
shutdown(/* sock */(int)regs->regs_R.dw[MD_REG_EBX],
    /* how */(int)regs->regs_R.dw[MD_REG_ECX]);

/* check for an error condition */
if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
  /* got an error, return details */
  regs->regs_R.dw[MD_REG_EAX] = -errno;
  break;
#endif

#if !defined(MIN_SYSCALL_MODE)
  case X86_SYS_poll:
{
  int i;
  int error_return;
  struct pollfd *fds;

  /* allocate host side I/O vectors */
  fds = calloc(/* nfds */regs->regs_R.dw[MD_REG_ECX], sizeof(struct pollfd));
  if (!fds)
    fatal("out of virtual memory in SYS_poll");

  /* copy target side I/O vector buffers to host memory */
  for (i=0; i < /* nfds */regs->regs_R.dw[MD_REG_ECX]; i++)
  {
    /* copy target side pointer data into host side vector */
    mem_bcopy(thread_id, mem_fn, mem, Read,
        (/* fds */regs->regs_R.dw[MD_REG_EBX]
         + i*sizeof(struct pollfd)),
        &fds[i], sizeof(struct pollfd));
  }

  /* perform the vector'ed write */
  /* result */error_return =
    poll(/* fds */fds,
        /* nfds */(unsigned long)regs->regs_R.dw[MD_REG_ECX],
        /* timeout */(int)regs->regs_R.dw[MD_REG_EDX]);

  /* copy target side I/O vector buffers to host memory */
  for (i=0; i < /* nfds */regs->regs_R.dw[MD_REG_ECX]; i++)
  {
    /* copy target side pointer data into host side vector */
    mem_bcopy(thread_id, mem_fn, mem, Write,
        (/* fds */regs->regs_R.dw[MD_REG_EBX]
         + i*sizeof(struct pollfd)),
        &fds[i], sizeof(struct pollfd));
  }

  /* check for an error condition */
  if (error_return != -1) {
    regs->regs_R.dw[MD_REG_EAX] = error_return;
  } /* no error */
  else {
    regs->regs_R.dw[MD_REG_EAX] = -(errno); /* negative of the error number is returned in r0 */
  }



  /* free all the allocated memory */
  free(fds);
}
break;
#endif

/* part of socketcall in arm linux */
#if !defined(MIN_SYSCALL_MODE) && 0
case OSF_SYS_gethostname:
/* get program scheduling priority */
{
  char *buf;

  buf = calloc(1,/* len */(size_t)regs->regs_R[MD_REG_A1]);
  if (!buf)
    fatal("out of virtual memory in gethostname()");

  /* result */regs->regs_R[MD_REG_V0] =
    gethostname(/* name */buf,
        /* len */(size_t)regs->regs_R[MD_REG_A1]);

  /* check for an error condition */
  if (regs->regs_R[MD_REG_V0] != (dword_t)-1)
    regs->regs_R[MD_REG_A3] = 0;
  else /* got an error, return details */
  {
    regs->regs_R[MD_REG_A3] = -1;
    regs->regs_R[MD_REG_V0] = errno;
  }

  /* copy string back to simulated memory */
  mem_bcopy(thread_id, mem_fn, mem, Write,
      /* name */regs->regs_R[MD_REG_A0],
      buf, /* len */regs->regs_R[MD_REG_A1]);
}
break;
#endif

/* The entry into the sockets function calls!!! */
#if !defined(MIN_SYSCALL_MODE)
case X86_SYS_socketcall:
/* the first argument is the socket call function type */
switch((int)regs->regs_R.dw[MD_REG_EBX]){
  case X86_SYS_SOCKET:
    {
      /* create an endpoint for communication */
      int __domain,__type,__protocol;
      /* grab the socket call arguments from simulated memory */
      mem_bcopy(thread_id, mem_fn, mem, Read, regs->regs_R.dw[MD_REG_ECX], &__domain,
          /*nbytes*/sizeof(int));
      mem_bcopy(thread_id, mem_fn, mem, Read, (regs->regs_R.dw[MD_REG_ECX]+sizeof(int)), &__type,
          /*nbytes*/sizeof(int));
      mem_bcopy(thread_id, mem_fn, mem, Read, (regs->regs_R.dw[MD_REG_ECX]+2*sizeof(int)), &__protocol,
          /*nbytes*/sizeof(int));


      /* result */regs->regs_R.dw[MD_REG_EAX] =
        socket(/* domain */xlate_arg(__domain,
              family_map, N_ELT(family_map),
              "socket(family)"),
            /* type */xlate_arg(__type,
              socktype_map, N_ELT(socktype_map),
              "socket(type)"),
            /* protocol */xlate_arg(__protocol,
              family_map, N_ELT(family_map),
              "socket(proto)"));


      /* check for an error condition */
      if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
        /* got an error, return details */
        regs->regs_R.dw[MD_REG_EAX] = -errno;
    }
    break;

  case X86_SYS_BIND:
    {
      const struct sockaddr a_sock;
      int __sockfd,__addrlen,__sim_a_sock;

      /* grab the function call arguments from memory */
      mem_bcopy(thread_id, mem_fn, mem, Read,regs->regs_R.dw[MD_REG_EBX], 
          &__sockfd,sizeof(int));
      mem_bcopy(thread_id, mem_fn, mem, Read,(regs->regs_R.dw[MD_REG_EBX]+sizeof(int)),
          &__sim_a_sock, sizeof(int));
      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_EBX]+2*sizeof(int)),
          &__addrlen, sizeof(int));

      /* copy the sockadd to real memory */
      mem_bcopy(thread_id, mem_fn, mem, Read,__sim_a_sock,
          &a_sock, sizeof(struct sockaddr));

      regs->regs_R.dw[MD_REG_EAX] =
        bind(__sockfd, &a_sock,__addrlen);

      /* check for an error condition */

      /*NOT sure if the commented code is done since it is a subcall?*/
      //if (regs->regs_R.dw[MD_REG_EAX] != (dword_t)-1)
      //regs->regs_R.dw[MD_REG_EDXx] = 0;
      //else /* got an error, return details */
      //{
      //   regs->regs_R.dw[MD_REG_EDXx] = -1;
      //   regs->regs_R.dw[MD_REG_EAX] = errno;
      //}
    }
    break;

  case X86_SYS_CONNECT:
    {
      struct osf_sockaddr osf_sa;
      int __sockfd,__addrlen,__sim_addr;
      /* initiate a connection on a socket */

      /*copy the arguments from simulated memory */ 
      mem_bcopy(thread_id, mem_fn, mem, Read,regs->regs_R.dw[MD_REG_EBX], 
          &__sockfd,sizeof(int));
      mem_bcopy(thread_id, mem_fn, mem, Read,(regs->regs_R.dw[MD_REG_EBX]+sizeof(int)),
          &__sim_addr, sizeof(int));
      mem_bcopy(thread_id, mem_fn, mem, Read,(regs->regs_R.dw[MD_REG_EBX]+2*sizeof(int)),
          &__addrlen, sizeof(int));


      /* copy sockaddr structure to host memory */
      mem_bcopy(thread_id, mem_fn, mem, Read, /* serv_addr */__sim_addr,
          &osf_sa, sizeof(struct osf_sockaddr));

#if 0
      int i;
      sa.sa_family = osf_sa.sa_family;
      for (i=0; i < regs->regs_R.dw[MD_REG_EDX]; i++)
        sa.sa_data[i] = osf_sa.sa_data[i];
#endif
      /* result */regs->regs_R.dw[MD_REG_EAX] =
        connect(/* sockfd */__sockfd,
            (void *)&osf_sa, __addrlen);

      /* check for an error condition */
      if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
        /* got an error, return details */
        regs->regs_R.dw[MD_REG_EAX] = -errno;
    }
    break;


  case X86_SYS_LISTEN:
    {
      warn("invalid/unimplemented socket function, PC=0x%08p, winging it"
          , regs->regs_PC);
      abort();
    }
    break;
  case X86_SYS_ACCEPT:
    {
      warn("invalid/unimplemented socket function, PC=0x%08p, winging it"
          , regs->regs_PC);
      abort();
    }
    break;
  case X86_SYS_GETSOCKNAME:
    {
      /* get socket name */
      char *buf;
      dword_t osf_addrlen;
      int addrlen;
      int __s, __name, __namelen;
      /*copy the arguments from simulated memory */ 
      mem_bcopy(thread_id, mem_fn, mem, Read,regs->regs_R.dw[MD_REG_EBX], 
          &__s,sizeof(int));
      mem_bcopy(thread_id, mem_fn, mem, Read,(regs->regs_R.dw[MD_REG_EBX]+sizeof(int)),
          &__name, sizeof(int));
      mem_bcopy(thread_id, mem_fn, mem, Read,(regs->regs_R.dw[MD_REG_EBX]+2*sizeof(int)),
          &__namelen, sizeof(int));

      /* get simulator memory parameters to host memory */
      mem_bcopy(thread_id, mem_fn, mem, Read,
          /* paddrlen */__namelen,
          &osf_addrlen, sizeof(osf_addrlen));
      addrlen = (int)osf_addrlen;
      if (addrlen != 0)
      {
        buf = calloc(1, addrlen);
        if (!buf)
          fatal("cannot allocate memory in OSF_SYS_old_getsockname");
      }
      else
        buf = NULL;

      /* do the actual system call on the bative machine */	
      /* result */regs->regs_R[MD_REG_V0] =
        getsockname(/* sock */__s,
            /* name */(struct sockaddr *)buf,
            /* namelen */&addrlen);

      /* check for an error condition */
      if (regs->regs_R.dw[MD_REG_EAX] != (dword_t)-1)
        ;
      //	      regs->regs_R[MD_REG_A3] = 0;
      else /* got an error, return details */
      {
        //regs->regs_R[MD_REG_A3] = -1;
        regs->regs_R[MD_REG_V0] = errno;
      }

      /* copy results to simulator memory */
      if (addrlen != 0)
        mem_bcopy(thread_id, mem_fn, mem, Write,
            /* addr */regs->regs_R[MD_REG_A1],
            buf, addrlen);

      osf_addrlen = (qword_t)addrlen;
      mem_bcopy(thread_id, mem_fn, mem, Write,
          /* paddrlen */__namelen,
          &osf_addrlen, sizeof(osf_addrlen));

      if (buf != NULL)
        free(buf);
    }	  
    break;

  case X86_SYS_GETPEERNAME:
    {
      /* get socket name */
      char *buf;
      dword_t osf_addrlen;
      int addrlen;
      int __s, __name, __namelen;
      /*grab the function call arguments from sim memory*/
      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]),
          &__s, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+sizeof(int)),
          &__name, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+2*sizeof(int)),
          &__namelen, sizeof(int));


      /* get simulator memory parameters to host memory */
      mem_bcopy(thread_id, mem_fn, mem, Read,
          /* paddrlen */__namelen,
          &osf_addrlen, sizeof(osf_addrlen));
      addrlen = (int)osf_addrlen;

      /* allocate host memory for system call result */
      if (addrlen != 0)
      {
        buf = calloc(1, addrlen);
        if (!buf)
          fatal("cannot allocate memory in OSF_SYS_old_getsockname");
      }
      else
        buf = NULL;

      /* result */regs->regs_R.dw[MD_REG_EAX] =
        getpeername(/* sock */__s,
            /* name */(struct sockaddr *)buf,
            /* namelen */&addrlen);

      /* check for an error condition */
      /* NOT sure how to handle this yet ??
         do we set memory?? in arm*/
      if (regs->regs_R.dw[MD_REG_EAX] != (dword_t)-1);
      //regs->regs_R[MD_REG_A3] = 0;
      else /* got an error, return details */
        //  {
        //    regs->regs_R[MD_REG_A3] = -1;
        regs->regs_R.dw[MD_REG_EAX] = errno;
      //  }

      /* copy results to simulator memory */
      if (addrlen != 0)
        mem_bcopy(thread_id, mem_fn, mem, Write,
            /* addr */__name,
            buf, addrlen);

      osf_addrlen = (qword_t)addrlen;
      mem_bcopy(thread_id, mem_fn, mem, Write,
          /* paddrlen */__namelen,
          &osf_addrlen, sizeof(osf_addrlen));

      if (buf != NULL)
        free(buf);
    }
    break;

  case X86_SYS_SOCKETPAIR:
    {
      warn("invalid/unimplemented socket function, PC=0x%08p, winging it"
          , regs->regs_PC);
      abort();
    }
    break;          

  case X86_SYS_SEND:
    {

      warn("invalid/unimplemented socket function, PC=0x%08p, winging it"
          , regs->regs_PC);
      abort();
    }
    break;

  case X86_SYS_RECV:
    {
      warn("invalid/unimplemented socket function, PC=0x%08p, winging it"
          , regs->regs_PC);
      abort();
    }
    break;

  case X86_SYS_SENDTO:
    {
      char *buf = NULL;
      struct sockaddr d_sock;
      int buf_len = 0;
      int __s, __msg, __len, __flags, __to, __tolen;
      /*grab the function call arguments from sim memory*/
      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]),
          &__s, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+sizeof(int)),
          &__msg, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+2*sizeof(int)),
          &__len, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+3*sizeof(int)),
          &__flags, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+4*sizeof(int)),
          &__to, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+5*sizeof(int)),
          &__tolen, sizeof(int));


      buf_len = __len;
      /* make a buffer in host memory for system call */
      if (buf_len > 0)
        buf = (char *) calloc(buf_len,sizeof(char));

      /* copy the message from simualted memory to host memory */
      mem_bcopy(thread_id, mem_fn, mem, Read, /* serv_addr */__msg,
          buf, /* addrlen */__len);

      if (__tolen > 0) 
        mem_bcopy(thread_id, mem_fn, mem, Read, __to,
            &d_sock, __tolen);

      /* make the actual system call */
      regs->regs_R.dw[MD_REG_EAX] =
        sendto(__s,buf,__len,__flags,&d_sock,__tolen);

      mem_bcopy(thread_id, mem_fn, mem, Write, /* serv_addr*/__msg,
          buf, /* addrlen */__len);

      /* maybe copy back whole size of sockaddr */
      if (__tolen > 0)
        mem_bcopy(thread_id, mem_fn, mem, Write, __to,
            &d_sock, __tolen);

      /* Not sure what to do with the error conditions yet */
      /* check for an error condition */
      if (regs->regs_R.dw[MD_REG_EAX] != (dword_t)-1)
        ;

      //  regs->regs_R[MD_REG_A3] = 0;
      else /* got an error, return details */
      {
        //regs->regs_R[MD_REG_A3] = -1;
        regs->regs_R.dw[MD_REG_EAX] = errno;
      }

      if (buf != NULL) 
        free(buf);
    }	 
    break;

  case X86_SYS_RECVFROM:
    {
      int addr_len;
      char *buf;
      struct sockaddr *a_sock;
      int __s, __buf, __len, __flags, __from, __fromlen;          
      /*grab the function call arguments from sim memory*/
      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]),
          &__s, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+sizeof(int)),
          &__buf, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+2*sizeof(int)),
          &__len, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+3*sizeof(int)),
          &__flags, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+4*sizeof(int)),
          &__from, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+5*sizeof(int)),
          &__fromlen, sizeof(int));

      buf = (char *) calloc(1,(__len));

      mem_bcopy(thread_id, mem_fn, mem, Read, /* serv_addr */__buf,
          buf, /* addrlen */__len);

      mem_bcopy(thread_id, mem_fn, mem, Read, /* serv_addr */__fromlen,
          &addr_len, sizeof(int));

      /* make a buffer in host memory for the socket address */
      a_sock = (struct sockaddr *)calloc(1,addr_len);

      mem_bcopy(thread_id, mem_fn, mem, Read, __from,
          a_sock, addr_len);

      /* make the actual system call */
      regs->regs_R.dw[MD_REG_EAX] =
        recvfrom(__s, buf,__len,__flags, a_sock,&addr_len);

      mem_bcopy(thread_id, mem_fn, mem, Write, __buf,
          buf, (int) regs->regs_R.dw[MD_REG_EBX]);

      mem_bcopy(thread_id, mem_fn, mem, Write, /* serv_addr */__fromlen,
          &addr_len, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Write, __from,
          a_sock, addr_len);

      /* check for an error condition */
      if (regs->regs_R.dw[MD_REG_EAX] != (dword_t)-1)
        ;
      //regs->regs_R[MD_REG_A3] = 0;
      else /* got an error, return details */
      {
        // regs->regs_R[MD_REG_A3] = -1;
        regs->regs_R[MD_REG_V0] = errno;
      }
      if (buf != NULL)
        free(buf);
    }
    break;

  case X86_SYS_SHUTDOWN:
    {
      /* can't find docs on this winging it!! */
      int __arg1, __arg2;
      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]),
          &__arg1, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+sizeof(int)),
          &__arg2, sizeof(int));
      /* shuts down socket send and receive operations */

      /*result*/regs->regs_R.dw[MD_REG_EAX] =
        shutdown(/* sock */__arg1,
            /* how */__arg2);

      /* check for an error condition */
      if (regs->regs_R.dw[MD_REG_EAX] == (dword_t)-1)
        /* got an error, return details */
        regs->regs_R.dw[MD_REG_EAX] = -errno;
    }
    break;

  case X86_SYS_SETSOCKOPT:
    {
      char *buf;
      struct xlate_table_t *map;
      int nmap;
      int __s, __level, __optname, __optval, __optlen;
      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]),
          &__s, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+sizeof(int)),
          &__level, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+2*sizeof(int)),
          &__optname, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+3*sizeof(int)),
          &__optval, sizeof(int));

      mem_bcopy(thread_id, mem_fn, mem, Read,
          (regs->regs_R.dw[MD_REG_ECX]+4*sizeof(int)),
          &__optlen, sizeof(int));


      /* set options on sockets */

      /* copy optval to host memory */
      if (/* optval */__optval != 0
          && /* optlen */__optlen != 0)
      {
        buf = calloc(1, /* optlen */(size_t)__optlen);
        if (!buf)
          fatal("cannot allocate memory in OSF_SYS_setsockopt");

        /* copy target side pointer data into host side vector */
        mem_bcopy(thread_id, mem_fn, mem, Read,
            /* optval */__optval,
            buf, /* optlen */__optlen);
      }
      else
        buf = NULL;

      /* pick the correct translation table */
      if (__level == OSF_SOL_SOCKET)
      {
        map = sockopt_map;
        nmap = N_ELT(sockopt_map);
      }
      else if (__level == OSF_SOL_TCP)
      {
        map = tcpopt_map;
        nmap = N_ELT(tcpopt_map);
      }
      else
      {
        warn("no translation map available for `setsockopt()': %d",
            __level);
        map = sockopt_map;
        nmap = N_ELT(sockopt_map);
      }

      /* result */regs->regs_R.dw[MD_REG_EAX] =
        setsockopt(/* sock */__s,
            /* level */xlate_arg(__level,
              socklevel_map, N_ELT(socklevel_map),
              "setsockopt(level)"),
            /* optname */xlate_arg(__optname,
              map, nmap,
              "setsockopt(opt)"),
            /* optval */buf,
            /* optlen */__optlen);

      /*not sure how to handle errors yet */        
      /* check for an error condition */
      if (regs->regs_R.dw[MD_REG_EAX] != (dword_t)-1)
        ;
      //  regs->regs_R[MD_REG_A3] = 0;
      else /* got an error, return details */
      {
        //  regs->regs_R[MD_REG_A3] = -1;
        regs->regs_R.dw[MD_REG_EAX] = errno;
      }

      if (buf != NULL)
        free(buf);
    }	 
    break;

  case X86_SYS_GETSOCKOPT:
    {
      warn("invalid/unimplemented socket function, PC=0x%08p, winging it"
          , regs->regs_PC);
      abort();
    }
    break;          

  case X86_SYS_SENDMSG:
    {
      warn("invalid/unimplemented socket function, PC=0x%08p, winging it"
          , regs->regs_PC);
      abort();
    }
    break;

  case X86_SYS_RECVMSG:
    {
      warn("invalid/unimplemented socket function, PC=0x%08p, winging it"
          , regs->regs_PC);
      abort();
    }
    break;
  default:
    warn("invalid/unimplemented socket function, PC=0x%08p, winging it"
        , regs->regs_PC);
    abort();
}
break;
#endif

default:
warn("invalid/unimplemented syscall %d, PC=0x%08p, winging it",
    (int)syscode, regs->regs_PC);
/* declare an error condition */
regs->regs_R.dw[MD_REG_EAX] = 0;
}
#ifdef DEBUG
if (debugging)
  myfprintf(stderr, "syscall(%d) @ %n: returned 0x%08x(%d)...\n",
      (int)syscode, thread->stat.num_insn, regs->regs_R.dw[MD_REG_EAX], regs->regs_R.dw[MD_REG_EAX]);
#endif
}
