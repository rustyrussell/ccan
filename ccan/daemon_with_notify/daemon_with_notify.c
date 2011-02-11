/*-
 * Copyright (c) 1990, 1993
 *    The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2010
 *    Stewart Smith
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>

#include <ccan/daemon_with_notify/daemon_with_notify.h>

void sigusr1_handler(int sig);

pid_t parent_pid;

void sigusr1_handler(int sig)
{
  if (sig == SIGUSR1)
    _exit(EXIT_SUCCESS);
}

int daemon_is_ready(void)
{
  kill(parent_pid, SIGUSR1);
  return 0;
}

int daemonize(int nochdir, int noclose, int wait_sigusr1)
{
    int fd;
    pid_t child= -1;

    parent_pid= getpid();
    signal(SIGUSR1, sigusr1_handler);

    child= fork();

    switch (child)
    {
    case -1:
        return (-1);
    case 0:
        break;
    default:
      if (wait_sigusr1)
      {
        /* parent */
        int exit_code= -1;
        int status;
        while (waitpid(child, &status, 0) != child);
        if (WIFEXITED(status))
        {
          exit_code= WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status))
        {
          exit_code= -1;
        }
        _exit(exit_code);
      }
      else
      {
        _exit(EXIT_SUCCESS);
      }
    }

    /* child */
    if (setsid() == -1)
        return (-1);

    if (nochdir == 0) {
        if(chdir("/") != 0) {
            perror("chdir");
            return (-1);
        }
    }

    if (noclose == 0 && (fd = open("/dev/null", O_RDWR, 0)) != -1) {
        if(dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 stdin");
            return (-1);
        }
        if(dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 stdout");
            return (-1);
        }
        if(dup2(fd, STDERR_FILENO) < 0) {
            perror("dup2 stderr");
            return (-1);
        }

        if (fd > STDERR_FILENO) {
            if(close(fd) < 0) {
                perror("close");
                return (-1);
            }
        }
    }
    return (0);
}
