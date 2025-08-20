// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "xv6fs.h"
#include "file.h"
#include "user.h"
#include "fcntl.h"
#include "debug.h"

char musl_basic_dir[] = "/musl/basic";
char glibc_basic_dir[] = "/glibc/basic";
char musl_dir[] = "/musl";
char glibc_dir[] = "/glibc";
char glibc_busybox_path[] = "/glibc/busybox";
char musl_busybox_path[] = "/musl/busybox";

char musl_basic_start_str[] = "#### OS COMP TEST GROUP START basic-musl ####\n";
char glibc_basic_start_str[] = "#### OS COMP TEST GROUP START basic-glibc ####\n";
char musl_basic_end_str[] = "#### OS COMP TEST GROUP END basic-musl ####\n";
char glibc_basic_end_str[] = "#### OS COMP TEST GROUP END basic-glibc ####\n";
char *musl_basic_start_echo_argv[] = {"/musl/busybox", "echo", musl_basic_start_str, NULL};
char *glibc_basic_start_echo_argv[] = {"/glibc/busybox"," echo", glibc_basic_start_str, NULL};
char *musl_basic_end_echo_argv[] = {"/musl/busybox", "echo", musl_basic_end_str, NULL};
char *glibc_basic_end_echo_argv[] = {"/glibc/busybox", "echo", glibc_basic_end_str, NULL};
char *glibc_basic_test_argv[] = {"/glibc/busybox_unstripped", "sh", "/glibc/basic/run-all.sh", NULL};
char *glibc_busybox_test_argv[] = {"/glibc/busybox", "sh", "/glibc/busybox_testcode.sh", NULL };
char *musl_basic_test_argv[] = {"/musl/busybox", "sh", "/musl/basic/run-all.sh", NULL };
char *musl_busybox_test_argv[] = {"/musl/busybox", "sh", "/musl/busybox_testcode.sh", NULL };
char *musl_git_test_argv[] = {"/musl/busybox", "sh", "/musl/git_testcode.sh", NULL };
char *glibc_shell_argv[] = {"/glibc/busybox", "sh", NULL };
char *musl_shell_argv[] = {"/musl/busybox", "sh", NULL };

char *musl_libctest_static_cmds[] = {
  "echo '#### OS COMP TEST GROUP START libctest-musl ####'",

  "/musl/runtest.exe -w entry-static.exe argv",
  "/musl/runtest.exe -w entry-static.exe basename",
  "/musl/runtest.exe -w entry-static.exe clocale_mbfuncs",
  "/musl/runtest.exe -w entry-static.exe clock_gettime",
  "/musl/runtest.exe -w entry-static.exe dirname",
  "/musl/runtest.exe -w entry-static.exe env",
  "/musl/runtest.exe -w entry-static.exe fdopen",
  "/musl/runtest.exe -w entry-static.exe fnmatch",
  "/musl/runtest.exe -w entry-static.exe fscanf",
  "/musl/runtest.exe -w entry-static.exe fwscanf",
  "/musl/runtest.exe -w entry-static.exe iconv_open",
  "/musl/runtest.exe -w entry-static.exe inet_pton",
  "/musl/runtest.exe -w entry-static.exe mbc",
  "/musl/runtest.exe -w entry-static.exe memstream",
  // "/musl/runtest.exe -w entry-static.exe pthread_cancel_points",!
  "/musl/runtest.exe -w entry-static.exe pthread_cancel",
  // "/musl/runtest.exe -w entry-static.exe pthread_cond", !
  "/musl/runtest.exe -w entry-static.exe qsort",
  "/musl/runtest.exe -w entry-static.exe random",
  "/musl/runtest.exe -w entry-static.exe search_hsearch",
  "/musl/runtest.exe -w entry-static.exe search_insque",
  "/musl/runtest.exe -w entry-static.exe search_lsearch",
  "/musl/runtest.exe -w entry-static.exe search_tsearch",
  "/musl/runtest.exe -w entry-static.exe snprintf",
  "/musl/runtest.exe -w entry-static.exe sscanf",
  "/musl/runtest.exe -w entry-static.exe sscanf_long",
  "/musl/runtest.exe -w entry-static.exe strftime",
  "/musl/runtest.exe -w entry-static.exe string",
  "/musl/runtest.exe -w entry-static.exe string_memcpy",
  "/musl/runtest.exe -w entry-static.exe string_memmem",
  "/musl/runtest.exe -w entry-static.exe string_memset",
  "/musl/runtest.exe -w entry-static.exe string_strchr",
  "/musl/runtest.exe -w entry-static.exe string_strcspn",
  "/musl/runtest.exe -w entry-static.exe string_strstr",
  "/musl/runtest.exe -w entry-static.exe strptime",
  "/musl/runtest.exe -w entry-static.exe strtod",
  "/musl/runtest.exe -w entry-static.exe strtod_simple",
  "/musl/runtest.exe -w entry-static.exe strtof",
  "/musl/runtest.exe -w entry-static.exe strtol",
  "/musl/runtest.exe -w entry-static.exe strtold",
  "/musl/runtest.exe -w entry-static.exe swprintf",
  "/musl/runtest.exe -w entry-static.exe tgmath",
  "/musl/runtest.exe -w entry-static.exe time",
  "/musl/runtest.exe -w entry-static.exe tls_align",
  "/musl/runtest.exe -w entry-static.exe udiv",
  "/musl/runtest.exe -w entry-static.exe ungetc",
  "/musl/runtest.exe -w entry-static.exe wcsstr",
  "/musl/runtest.exe -w entry-static.exe wcstol",
  "/musl/runtest.exe -w entry-static.exe daemon_failure",
  "/musl/runtest.exe -w entry-static.exe dn_expand_empty",
  "/musl/runtest.exe -w entry-static.exe dn_expand_ptr_0",
  "/musl/runtest.exe -w entry-static.exe fgets_eof",
  "/musl/runtest.exe -w entry-static.exe fgetwc_buffering",
  "/musl/runtest.exe -w entry-static.exe fpclassify_invalid_ld80",
  "/musl/runtest.exe -w entry-static.exe ftello_unflushed_append",
  "/musl/runtest.exe -w entry-static.exe getpwnam_r_crash",
  "/musl/runtest.exe -w entry-static.exe getpwnam_r_errno",
  "/musl/runtest.exe -w entry-static.exe iconv_roundtrips",
  "/musl/runtest.exe -w entry-static.exe inet_ntop_v4mapped",
  "/musl/runtest.exe -w entry-static.exe inet_pton_empty_last_field",
  "/musl/runtest.exe -w entry-static.exe iswspace_null",
  "/musl/runtest.exe -w entry-static.exe lrand48_signextend",
  "/musl/runtest.exe -w entry-static.exe lseek_large",
  "/musl/runtest.exe -w entry-static.exe malloc_0",
  "/musl/runtest.exe -w entry-static.exe mbsrtowcs_overflow",
  "/musl/runtest.exe -w entry-static.exe memmem_oob_read",
  "/musl/runtest.exe -w entry-static.exe memmem_oob",
  "/musl/runtest.exe -w entry-static.exe mkdtemp_failure",
  "/musl/runtest.exe -w entry-static.exe mkstemp_failure",
  "/musl/runtest.exe -w entry-static.exe printf_1e9_oob",
  "/musl/runtest.exe -w entry-static.exe printf_fmt_g_round",
  "/musl/runtest.exe -w entry-static.exe printf_fmt_g_zeros",
  "/musl/runtest.exe -w entry-static.exe printf_fmt_n",
  "/musl/runtest.exe -w entry-static.exe pthread_condattr_setclock",
  "/musl/runtest.exe -w entry-static.exe pthread_exit_cancel", 
  // "/musl/runtest.exe -w entry-static.exe pthread_rwlock_ebusy", !
  "/musl/runtest.exe -w entry-static.exe putenv_doublefree",
  "/musl/runtest.exe -w entry-static.exe regex_backref_0",
  "/musl/runtest.exe -w entry-static.exe regex_bracket_icase",
  "/musl/runtest.exe -w entry-static.exe regex_ere_backref",
  "/musl/runtest.exe -w entry-static.exe regex_escaped_high_byte",
  "/musl/runtest.exe -w entry-static.exe regex_negated_range",
  "/musl/runtest.exe -w entry-static.exe regexec_nosub",
  "/musl/runtest.exe -w entry-static.exe rewind_clear_error",
  "/musl/runtest.exe -w entry-static.exe rlimit_open_files",
  "/musl/runtest.exe -w entry-static.exe scanf_bytes_consumed",
  "/musl/runtest.exe -w entry-static.exe scanf_match_literal_eof",
  "/musl/runtest.exe -w entry-static.exe scanf_nullbyte_char",
  "/musl/runtest.exe -w entry-static.exe setvbuf_unget",
  "/musl/runtest.exe -w entry-static.exe sigprocmask_internal",
  "/musl/runtest.exe -w entry-static.exe sscanf_eof",
  "/musl/runtest.exe -w entry-static.exe statvfs",
  "/musl/runtest.exe -w entry-static.exe strverscmp",
  "/musl/runtest.exe -w entry-static.exe uselocale_0",
  "/musl/runtest.exe -w entry-static.exe wcsncpy_read_overflow",
  "/musl/runtest.exe -w entry-static.exe wcsstr_false_negative",

  // "echo '#### OS COMP TEST GROUP END libctest-musl ####'",
  NULL
};

char *musl_libc_test_dynamic_cmds[] = {

"/musl/runtest.exe -w entry-dynamic.exe argv",
"/musl/runtest.exe -w entry-dynamic.exe basename",
"/musl/runtest.exe -w entry-dynamic.exe clocale_mbfuncs",
"/musl/runtest.exe -w entry-dynamic.exe clock_gettime",
"/musl/runtest.exe -w entry-dynamic.exe dirname",
// "/musl/runtest.exe -w entry-dynamic.exe dlopen", !
"/musl/runtest.exe -w entry-dynamic.exe env",
"/musl/runtest.exe -w entry-dynamic.exe fdopen",
"/musl/runtest.exe -w entry-dynamic.exe fnmatch",
"/musl/runtest.exe -w entry-dynamic.exe fscanf",
"/musl/runtest.exe -w entry-dynamic.exe fwscanf",
"/musl/runtest.exe -w entry-dynamic.exe iconv_open",
"/musl/runtest.exe -w entry-dynamic.exe inet_pton",
"/musl/runtest.exe -w entry-dynamic.exe mbc",
"/musl/runtest.exe -w entry-dynamic.exe memstream",
// "/musl/runtest.exe -w entry-dynamic.exe pthread_cancel_points",!
"/musl/runtest.exe -w entry-dynamic.exe pthread_cancel",
// "/musl/runtest.exe -w entry-dynamic.exe pthread_cond", !
// "/musl/runtest.exe -w entry-dynamic.exe pthread_tsd", !
"/musl/runtest.exe -w entry-dynamic.exe qsort",
"/musl/runtest.exe -w entry-dynamic.exe random",
"/musl/runtest.exe -w entry-dynamic.exe search_hsearch",
"/musl/runtest.exe -w entry-dynamic.exe search_insque",
"/musl/runtest.exe -w entry-dynamic.exe search_lsearch",
"/musl/runtest.exe -w entry-dynamic.exe search_tsearch",
// "/musl/runtest.exe -w entry-dynamic.exe sem_init",!
// "/musl/runtest.exe -w entry-dynamic.exe setjmp", !
"/musl/runtest.exe -w entry-dynamic.exe snprintf",
// "/musl/runtest.exe -w entry-dynamic.exe socket", !
"/musl/runtest.exe -w entry-dynamic.exe sscanf",
"/musl/runtest.exe -w entry-dynamic.exe sscanf_long",
// "/musl/runtest.exe -w entry-dynamic.exe stat", !
"/musl/runtest.exe -w entry-dynamic.exe strftime",
"/musl/runtest.exe -w entry-dynamic.exe string",
"/musl/runtest.exe -w entry-dynamic.exe string_memcpy",
"/musl/runtest.exe -w entry-dynamic.exe string_memmem",
"/musl/runtest.exe -w entry-dynamic.exe string_memset",
"/musl/runtest.exe -w entry-dynamic.exe string_strchr",
"/musl/runtest.exe -w entry-dynamic.exe string_strcspn",
"/musl/runtest.exe -w entry-dynamic.exe string_strstr",
"/musl/runtest.exe -w entry-dynamic.exe strptime",
"/musl/runtest.exe -w entry-dynamic.exe strtod",
"/musl/runtest.exe -w entry-dynamic.exe strtod_simple",
"/musl/runtest.exe -w entry-dynamic.exe strtof",
"/musl/runtest.exe -w entry-dynamic.exe strtol",
"/musl/runtest.exe -w entry-dynamic.exe strtold",
"/musl/runtest.exe -w entry-dynamic.exe swprintf",
"/musl/runtest.exe -w entry-dynamic.exe tgmath",
"/musl/runtest.exe -w entry-dynamic.exe time",
// "/musl/runtest.exe -w entry-dynamic.exe tls_init",!
// "/musl/runtest.exe -w entry-dynamic.exe tls_local_exec", !
"/musl/runtest.exe -w entry-dynamic.exe udiv",
"/musl/runtest.exe -w entry-dynamic.exe ungetc",
// "/musl/runtest.exe -w entry-dynamic.exe utime", !
"/musl/runtest.exe -w entry-dynamic.exe wcsstr",
"/musl/runtest.exe -w entry-dynamic.exe wcstol",
"/musl/runtest.exe -w entry-dynamic.exe daemon_failure",
"/musl/runtest.exe -w entry-dynamic.exe dn_expand_empty",
"/musl/runtest.exe -w entry-dynamic.exe dn_expand_ptr_0",
// "/musl/runtest.exe -w entry-dynamic.exe fflush_exit", !
"/musl/runtest.exe -w entry-dynamic.exe fgets_eof",
"/musl/runtest.exe -w entry-dynamic.exe fgetwc_buffering",
"/musl/runtest.exe -w entry-dynamic.exe fpclassify_invalid_ld80",
"/musl/runtest.exe -w entry-dynamic.exe ftello_unflushed_append",
"/musl/runtest.exe -w entry-dynamic.exe getpwnam_r_crash",
"/musl/runtest.exe -w entry-dynamic.exe getpwnam_r_errno",
"/musl/runtest.exe -w entry-dynamic.exe iconv_roundtrips",
"/musl/runtest.exe -w entry-dynamic.exe inet_ntop_v4mapped",
"/musl/runtest.exe -w entry-dynamic.exe inet_pton_empty_last_field",
"/musl/runtest.exe -w entry-dynamic.exe iswspace_null",
"/musl/runtest.exe -w entry-dynamic.exe lrand48_signextend",
"/musl/runtest.exe -w entry-dynamic.exe lseek_large",
"/musl/runtest.exe -w entry-dynamic.exe malloc_0",
"/musl/runtest.exe -w entry-dynamic.exe mbsrtowcs_overflow",
"/musl/runtest.exe -w entry-dynamic.exe memmem_oob_read",
"/musl/runtest.exe -w entry-dynamic.exe memmem_oob",
"/musl/runtest.exe -w entry-dynamic.exe mkdtemp_failure",
"/musl/runtest.exe -w entry-dynamic.exe mkstemp_failure",
"/musl/runtest.exe -w entry-dynamic.exe printf_1e9_oob",
"/musl/runtest.exe -w entry-dynamic.exe printf_fmt_g_round",
"/musl/runtest.exe -w entry-dynamic.exe printf_fmt_g_zeros",
"/musl/runtest.exe -w entry-dynamic.exe printf_fmt_n",
// "/musl/runtest.exe -w entry-dynamic.exe pthread_robust_detach", !
// "/musl/runtest.exe -w entry-dynamic.exe pthread_cond_smasher", !
"/musl/runtest.exe -w entry-dynamic.exe pthread_condattr_setclock",
// "/musl/runtest.exe -w entry-dynamic.exe pthread_exit_cancel", !
// "/musl/runtest.exe -w entry-dynamic.exe pthread_once_deadlock", !
// "/musl/runtest.exe -w entry-dynamic.exe pthread_rwlock_ebusy", !
"/musl/runtest.exe -w entry-dynamic.exe putenv_doublefree",
"/musl/runtest.exe -w entry-dynamic.exe regex_backref_0",
"/musl/runtest.exe -w entry-dynamic.exe regex_bracket_icase",
"/musl/runtest.exe -w entry-dynamic.exe regex_ere_backref",
"/musl/runtest.exe -w entry-dynamic.exe regex_escaped_high_byte",
"/musl/runtest.exe -w entry-dynamic.exe regex_negated_range",
"/musl/runtest.exe -w entry-dynamic.exe regexec_nosub",
"/musl/runtest.exe -w entry-dynamic.exe rewind_clear_error",
"/musl/runtest.exe -w entry-dynamic.exe rlimit_open_files",
"/musl/runtest.exe -w entry-dynamic.exe scanf_bytes_consumed",
"/musl/runtest.exe -w entry-dynamic.exe scanf_match_literal_eof",
"/musl/runtest.exe -w entry-dynamic.exe scanf_nullbyte_char",
"/musl/runtest.exe -w entry-dynamic.exe setvbuf_unget",
"/musl/runtest.exe -w entry-dynamic.exe sigprocmask_internal",
"/musl/runtest.exe -w entry-dynamic.exe sscanf_eof",
"/musl/runtest.exe -w entry-dynamic.exe statvfs",
"/musl/runtest.exe -w entry-dynamic.exe strverscmp",
// "/musl/runtest.exe -w entry-dynamic.exe syscall_sign_extend", !
// "/musl/runtest.exe -w entry-dynamic.exe tls_get_new_dtv", !
"/musl/runtest.exe -w entry-dynamic.exe uselocale_0",
"/musl/runtest.exe -w entry-dynamic.exe wcsncpy_read_overflow",
"/musl/runtest.exe -w entry-dynamic.exe wcsstr_false_negative",

"echo '#### OS COMP TEST GROUP END libctest-musl ####'",
  NULL
};

char * musl_final_test_cmd[] = {
  "/musl/splice_testcode.sh",
  // "/musl/copy-file-range_testcode.sh",
  "/musl/interrupts_testcode.sh",
  NULL
};

char * glibc_final_test_cmd[] = {
  "/glibc/splice_testcode.sh",
  // "/musl/copy-file-range_testcode.sh",
  "/glibc/interrupts_testcode.sh",
  NULL
};

char *busybox_envp[] = {
  "LC_ALL=C",
  "LANG=C",
  "CHARSET=C",
  "HOME=/",
  "PATH=/",
  NULL
};

#define SHELL
// #define LIBCTEST
// #define FINAL_ONLINE
// #define GIT

int main(void)
{
  dev(O_RDWR, CONSOLE, 0);
  dup(0); // stdout
  dup(0); // stderr
  printf("======================== init: starting rexvapor init !!! ========================\n\n");
  mkdir("/dev", 0755);
  mkdir("/tmp", 0755);

  mknod("/dev/null", S_IFCHR | 0666, 0);
  int pid;
  
#ifdef SHELL
  mkdir("/musl/proj", 0755);
  pid = fork();
  if(pid == 0) {
    if(chdir("musl/proj") < 0) {
      printf("init: chdir %s failed\n", "musl/proj");
      exit(-1);
    }
    int ret = execve("/musl/busybox", musl_shell_argv, busybox_envp);
    printf("execve returned %d\n", ret);
    exit(-1);
  } else {
    wait(0);
  }
#elif defined(LIBCTEST)

  // musl basic
  printf(musl_basic_start_str);

  pid = fork();
  if(pid == 0) {
    if(chdir(musl_basic_dir) < 0) {
      printf("init: chdir %s failed\n", musl_basic_dir);
      exit(-1);
    }
    int ret = execve(musl_busybox_path, musl_basic_test_argv, busybox_envp);
    printf("execve returned %d\n", ret);
  } else {
    wait(0);
    printf(musl_basic_end_str);
  }

  // glibc basic
  printf(glibc_basic_start_str);

  pid = fork();
  if(pid == 0) {
    if(chdir(glibc_basic_dir) < 0) {
      printf("init: chdir %s failed\n", glibc_basic_dir);
      exit(-1);
    }
    int ret = execve(glibc_busybox_path, glibc_basic_test_argv, busybox_envp);
    printf("execve returned %d\n", ret);
  } else {
    wait(0);
    printf(glibc_basic_end_str);
  }

  // musl busybox test
  pid = fork();
  if(pid == 0) {
    if(chdir(musl_dir) < 0) {
      printf("init: chdir %s failed\n", musl_dir);
      exit(-1);
    }
    int ret = execve(musl_busybox_path, musl_busybox_test_argv, busybox_envp);
    printf("execve returned %d\n", ret);
  } else { 
    wait(0);
  }

  // glibc busybox test
  pid = fork();
  if(pid == 0) {
    if(chdir(glibc_dir) < 0) {
      printf("init: chdir %s failed\n", glibc_dir);
      exit(-1);
    }
    int ret = execve(glibc_busybox_path, glibc_busybox_test_argv, busybox_envp);
    printf("execve returned %d\n", ret);
  } else {
    wait(0);
  }

  // musl libctest-static
  for (int i = 0; musl_libctest_static_cmds[i] != NULL; i++) {
    pid = fork();
    if (pid == 0) {
      if (chdir(musl_dir) < 0) {
        printf("init: chdir %s failed\n", musl_dir);
        exit(-1);
      }
      char *argv[] = {"/musl/busybox", "sh", "-c", musl_libctest_static_cmds[i], NULL};
      int ret = execve("/musl/busybox", argv, busybox_envp);
      printf("execve returned %d\n", ret);
      exit(-1);
    } else {
      wait(0);
    }
  }

  // musl libctest-dynamic
  for (int i = 0; musl_libc_test_dynamic_cmds[i] != NULL; i++) {
    pid = fork();
    if (pid == 0) {
      if (chdir(musl_dir) < 0) {
        printf("init: chdir %s failed\n", musl_dir);
        exit(-1);
      }
      char *argv[] = {"/musl/busybox", "sh", "-c", musl_libc_test_dynamic_cmds[i], NULL};
      int ret = execve("/musl/busybox", argv, busybox_envp);
      printf("execve returned %d\n", ret);
      exit(-1);
    } else {
      wait(0);
    }
  }


#elif defined(FINAL_ONLINE)

    for (int i = 0; musl_final_test_cmd[i] != NULL; i++) {
      pid = fork();
      if (pid == 0) {
        if (chdir(musl_dir) < 0) {
          printf("init: chdir %s failed\n", musl_dir);
          exit(-1);
        }
        char *argv[] = {"/musl/busybox", "sh", musl_final_test_cmd[i], NULL};
        int ret = execve("/musl/busybox", argv, busybox_envp);
        printf("execve returned %d\n", ret);
        exit(-1);
      } else {
        wait(0);
      }
    }

  for (int i = 0; glibc_final_test_cmd[i] != NULL; i++) {
    pid = fork();
    if (pid == 0) {
      if (chdir(glibc_dir) < 0) {
        printf("init: chdir %s failed\n", glibc_dir);
        exit(-1);
      }
      char *argv[] = {"/glibc/busybox", "sh", glibc_final_test_cmd[i], NULL};
      int ret = execve("/glibc/busybox", argv, busybox_envp);
      printf("execve returned %d\n", ret);
      exit(-1);
    } else {
      wait(0);
    }
  }

#elif defined(GIT)

    int fd;
    mkdir("/etc", 0755);
    if((fd = open("/etc/gitconfig", O_CREAT, 0)) < 0) {
      printf("create /etc/gitconfig failed");
    } else {
      // printf("create /etc/gitconfig successfully !");
      close(fd);
    }
    if((fd = open("/.gitconfig", O_CREAT, 0)) < 0) {
      printf("create /.gitconfig failed");
    } else {
      // printf("create /.gitconfig successfully !");
      close(fd);
    }
    mkdir("/musl/.git", 0755);
    // musl busybox test
    pid = fork();
    if(pid == 0) {
      if(chdir(musl_dir) < 0) {
        printf("init: chdir %s failed\n", musl_dir);
        exit(-1);
      }
      int ret = execve(musl_busybox_path, musl_git_test_argv, busybox_envp);
      printf("execve returned %d\n", ret);
    } else { 
      wait(0);
    }
  
#endif

  printf("\n=================== all tests ended ===================\n");
  poweroff(0);
  return 0;
}
