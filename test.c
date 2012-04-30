#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <signal.h>

#ifdef HAVE_PTY_H
int isatty(int fd) {
  return 1;
}
#define setup_pty()
#endif

void test_vfprintf(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
}


// Complete hackery debauchery but I'm done fighting with assanine tty
// excentricies to make my tests output results to the test runner. On
// BSD based system we'll make an actual pty and on GLIBC we'll just fake
// the isatty function call;
#ifdef HAVE_UTIL_H
#include <util.h>

// When our child attacked to the pty exits, we can exit
void signal_handler(int sig) {
  if (sig == SIGCHLD) {
    int status;
    wait(&status);
    exit WEXITSTATUS(status);
  }
}

// Because we check if the file descriptor is a tty before outputing
// the terminal escape sequences, we need to fake being a tty through
// the pseudo ttys.
void setup_pty() {
  int child;
  char pty[512];
  size_t len;
  fd_set wait;
  struct winsize win = { .ws_col = 80, .ws_row = 24,
                         .ws_xpixel = 480, .ws_ypixel = 192 };
  struct sigaction act;
  act.sa_handler = signal_handler;
  act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
  sigaction(SIGCHLD, &act, 0);

  switch (forkpty(&child, NULL, NULL, &win)) {
    case -1:
      perror("Failed starting pseudo tty");
      exit -1;
    case 0:
      break;
    default:
      FD_ZERO(&wait);
      FD_SET(child, &wait);
      while (select(child + 1, &wait, NULL, NULL, NULL) > 0) {
        do {
          len = read(child, pty, sizeof(pty));
        } while (len < 0 && len == EINTR);

        if (len == 0) break;

        size_t total = 0, written = 0;
        while (len > 0) {
          do {
            written = write(STDOUT_FILENO, pty + total, len);
          } while (written < 0 && written == EINTR);

          if (written == (size_t)-1) break;
          if (written == 0) {
            errno = ENOSPC;
            break;
          }

          total += written;
          len -= written;
        }
      }
      exit errno;
  }

}
#endif

int main() {
  setup_pty();
  setenv("STDERRED_ESC_CODE", ">", 1);
  setenv("STDERRED_END_CODE", "<", 1);
  dup2(STDOUT_FILENO, STDERR_FILENO);

  printf("1 printf\n");
  fflush(stdout);

  write(2, "2 write\n", 8);

  fprintf(stderr, "2 fprintf\n");

#ifdef HAVE_FPRINTF_UNLOCKED
  fprintf_unlocked(stderr, "2 fprintf_unlocked\n");
#endif

  fwrite("2 fwrite\n", 9, 1, stderr);

#ifdef HAVE_FWRITE_UNLOCKED
  fwrite_unlocked("2 fwrite_unlocked\n", 18, 1, stderr);
#endif

  fputc(0x32, stderr); fflush(stderr); printf(" <= fputc\n"); fflush(stdout);

#ifdef HAVE_FPUTC_UNLOCKED
  fputc_unlocked(0x32, stderr); fflush(stderr);
  printf(" <= fputc_unlocked\n"); fflush(stdout);
#endif

  fputs("2 fputs\n", stderr);

#ifdef HAVE_FPUTS_UNLOCKED
  fputs_unlocked("2 fputs_unlocked\n", stderr);
#endif

  test_vfprintf("2 %s\n", "vfprintf");

  errno = ENOSYS; perror("2 perror");

#ifdef HAVE_ERROR
  error(0, ENOSYS, "2 error");
#endif

#ifdef HAVE_ERROR_AT_LINE
  error_at_line(0, ENOENT, __FILE__, __LINE__, "2 error_at_line");
#endif

  return 0;
}
