/**
 * collectd - src/collectd.c
 * Copyright (C) 2005-2007  Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at collectd.org>
 *   Alvaro Barcellos <alvaro.barcellos at gmail.com>
 **/

#include "cmd.h"
#include "collectd.h"

#include "common.h"
#include "configfile.h"
#include "plugin.h"

#include <netdb.h>
#include <sys/types.h>

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#if HAVE_STATGRAB_H
#include <statgrab.h>
#endif

#if HAVE_KSTAT_H
#include <kstat.h>
#endif

#ifndef COLLECTD_LOCALE
#define COLLECTD_LOCALE "C"
#endif

<<<<<<< HEAD
static int loop;

static void *do_flush(void __attribute__((unused)) * arg) {
  INFO("Flushing all data.");
  plugin_flush(/* plugin = */ NULL,
               /* timeout = */ 0,
               /* ident = */ NULL);
  INFO("Finished flushing all data.");
  pthread_exit(NULL);
  return NULL;
}

static void sig_int_handler(int __attribute__((unused)) signal) { loop++; }

static void sig_term_handler(int __attribute__((unused)) signal) { loop++; }
=======
#ifdef WIN32
#undef COLLECT_DAEMON
#include <unistd.h>
#undef gethostname
#include <locale.h>
#include <winsock2.h>
#endif
>>>>>>> 95389ffa1005b6e450e62e66cf4a97cdbf7779b8

static int loop;

static int init_hostname(void) {
  const char *str = global_option_get("Hostname");
  if (str && str[0] != '\0') {
    hostname_set(str);
    return 0;
  }

#ifdef WIN32
  long hostname_len = NI_MAXHOST;
#else
  long hostname_len = sysconf(_SC_HOST_NAME_MAX);
  if (hostname_len == -1) {
    hostname_len = NI_MAXHOST;
  }
#endif /* WIN32 */
  char hostname[hostname_len];

  if (gethostname(hostname, hostname_len) != 0) {
    fprintf(stderr, "`gethostname' failed and no "
                    "hostname was configured.\n");
    return -1;
  }

  hostname_set(hostname);

  str = global_option_get("FQDNLookup");
  if (IS_FALSE(str))
    return 0;

  struct addrinfo *ai_list;
  struct addrinfo ai_hints = {.ai_flags = AI_CANONNAME};

  int status = getaddrinfo(hostname, NULL, &ai_hints, &ai_list);
  if (status != 0) {
    ERROR("Looking up \"%s\" failed. You have set the "
          "\"FQDNLookup\" option, but I cannot resolve "
          "my hostname to a fully qualified domain "
          "name. Please fix the network "
          "configuration.",
          hostname);
    return -1;
  }

  for (struct addrinfo *ai_ptr = ai_list; ai_ptr; ai_ptr = ai_ptr->ai_next) {
    if (ai_ptr->ai_canonname == NULL)
      continue;

    hostname_set(ai_ptr->ai_canonname);
    break;
  }

  freeaddrinfo(ai_list);
  return 0;
} /* int init_hostname */

static int init_global_variables(void) {
  interval_g = cf_get_default_interval();
  assert(interval_g > 0);
  DEBUG("interval_g = %.3f;", CDTIME_T_TO_DOUBLE(interval_g));

  const char *str = global_option_get("Timeout");
  if (str == NULL)
    str = "2";
  timeout_g = atoi(str);
  if (timeout_g <= 1) {
    fprintf(stderr, "Cannot set the timeout to a correct value.\n"
                    "Please check your settings.\n");
    return -1;
  }
  DEBUG("timeout_g = %i;", timeout_g);

  if (init_hostname() != 0)
    return -1;
  DEBUG("hostname_g = %s;", hostname_g);

  return 0;
} /* int init_global_variables */

static int change_basedir(const char *orig_dir, bool create) {
  char *dir = strdup(orig_dir);
  if (dir == NULL) {
    ERROR("strdup failed: %s", STRERRNO);
    return -1;
  }

  size_t dirlen = strlen(dir);
  while ((dirlen > 0) && (dir[dirlen - 1] == '/'))
    dir[--dirlen] = '\0';

  if (dirlen == 0) {
    free(dir);
    return -1;
  }

  int status = chdir(dir);
  if (status == 0) {
    free(dir);
    return 0;
  } else if (!create || (errno != ENOENT)) {
    ERROR("change_basedir: chdir (%s): %s", dir, STRERRNO);
    free(dir);
    return -1;
  }

  status = mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
  if (status != 0) {
    ERROR("change_basedir: mkdir (%s): %s", dir, STRERRNO);
    free(dir);
    return -1;
  }

  status = chdir(dir);
  if (status != 0) {
    ERROR("change_basedir: chdir (%s): %s", dir, STRERRNO);
    free(dir);
    return -1;
  }

  free(dir);
  return 0;
} /* static int change_basedir (char *dir) */

#if HAVE_LIBKSTAT
extern kstat_ctl_t *kc;
static void update_kstat(void) {
  if (kc == NULL) {
    if ((kc = kstat_open()) == NULL)
      ERROR("Unable to open kstat control structure");
  } else {
    kid_t kid = kstat_chain_update(kc);
    if (kid > 0) {
      INFO("kstat chain has been updated");
      plugin_init_all();
    } else if (kid < 0)
      ERROR("kstat chain update failed");
    /* else: everything works as expected */
  }

  return;
} /* static void update_kstat (void) */
#endif /* HAVE_LIBKSTAT */

__attribute__((noreturn)) static void exit_usage(int status) {
  printf("Usage: " PACKAGE_NAME " [OPTIONS]\n\n"

         "Available options:\n"
         "  General:\n"
         "    -C <file>       Configuration file.\n"
         "                    Default: " CONFIGFILE "\n"
         "    -t              Test config and exit.\n"
         "    -T              Test plugin read and exit.\n"
         "    -P <file>       PID-file.\n"
         "                    Default: " PIDFILE "\n"
#if COLLECT_DAEMON
         "    -f              Don't fork to the background.\n"
#endif
         "    -B              Don't create the BaseDir\n"
         "    -h              Display help (this message)\n"
         "\nBuiltin defaults:\n"
         "  Config file       " CONFIGFILE "\n"
         "  PID file          " PIDFILE "\n"
         "  Plugin directory  " PLUGINDIR "\n"
         "  Data directory    " PKGLOCALSTATEDIR "\n"
         "\n" PACKAGE_NAME " " PACKAGE_VERSION ", http://collectd.org/\n"
         "by Florian octo Forster <octo@collectd.org>\n"
         "for contributions see `AUTHORS'\n");
  exit(status);
} /* static void exit_usage (int status) */

static int do_init(void) {
#if HAVE_SETLOCALE
  if (setlocale(LC_NUMERIC, COLLECTD_LOCALE) == NULL)
    WARNING("setlocale (\"%s\") failed.", COLLECTD_LOCALE);

  /* Update the environment, so that libraries that are calling
   * setlocale(LC_NUMERIC, "") don't accidentally revert these changes. */
  unsetenv("LC_ALL");
  setenv("LC_NUMERIC", COLLECTD_LOCALE, /* overwrite = */ 1);
#endif

#if HAVE_LIBKSTAT
  kc = NULL;
  update_kstat();
#endif

#if HAVE_LIBSTATGRAB
  if (sg_init(
#if HAVE_LIBSTATGRAB_0_90
          0
#endif
          )) {
    ERROR("sg_init: %s", sg_str_error(sg_get_error()));
    return -1;
  }

  if (sg_drop_privileges()) {
    ERROR("sg_drop_privileges: %s", sg_str_error(sg_get_error()));
    return -1;
  }
#endif

  return plugin_init_all();
} /* int do_init () */

static int do_loop(void) {
  cdtime_t interval = cf_get_default_interval();
  cdtime_t wait_until = cdtime() + interval;

  while (loop == 0) {
#if HAVE_LIBKSTAT
    update_kstat();
#endif

    /* Issue all plugins */
    plugin_read_all();

    cdtime_t now = cdtime();
    if (now >= wait_until) {
      WARNING("Not sleeping because the next interval is "
              "%.3f seconds in the past!",
              CDTIME_T_TO_DOUBLE(now - wait_until));
      wait_until = now + interval;
      continue;
    }

    struct timespec ts_wait = CDTIME_T_TO_TIMESPEC(wait_until - now);
    wait_until = wait_until + interval;

    while ((loop == 0) && (nanosleep(&ts_wait, &ts_wait) != 0)) {
      if (errno != EINTR) {
        ERROR("nanosleep failed: %s", STRERRNO);
        return -1;
      }
    }
  } /* while (loop == 0) */

  return 0;
} /* int do_loop */

static int do_shutdown(void) {
  return plugin_shutdown_all();
} /* int do_shutdown */

<<<<<<< HEAD
#if COLLECT_DAEMON
static int pidfile_create(void) {
  FILE *fh;
  const char *file = global_option_get("PIDFile");

  if ((fh = fopen(file, "w")) == NULL) {
    ERROR("fopen (%s): %s", file, STRERRNO);
    return 1;
  }

  fprintf(fh, "%i\n", (int)getpid());
  fclose(fh);

  return 0;
} /* static int pidfile_create (const char *file) */

static int pidfile_remove(void) {
  const char *file = global_option_get("PIDFile");
  if (file == NULL)
    return 0;

  return unlink(file);
} /* static int pidfile_remove (const char *file) */
#endif /* COLLECT_DAEMON */

#ifdef KERNEL_LINUX
static int notify_upstart(void) {
  char const *upstart_job = getenv("UPSTART_JOB");

  if (upstart_job == NULL)
    return 0;

  if (strcmp(upstart_job, "collectd") != 0) {
    WARNING("Environment specifies unexpected UPSTART_JOB=\"%s\", expected "
            "\"collectd\". Ignoring the variable.",
            upstart_job);
    return 0;
  }

  NOTICE("Upstart detected, stopping now to signal readiness.");
  raise(SIGSTOP);
  unsetenv("UPSTART_JOB");

  return 1;
}

static int notify_systemd(void) {
  size_t su_size;
  const char *notifysocket = getenv("NOTIFY_SOCKET");
  if (notifysocket == NULL)
    return 0;

  if ((strlen(notifysocket) < 2) ||
      ((notifysocket[0] != '@') && (notifysocket[0] != '/'))) {
    ERROR("invalid notification socket NOTIFY_SOCKET=\"%s\": path must be "
          "absolute",
          notifysocket);
    return 0;
  }
  NOTICE("Systemd detected, trying to signal readiness.");

  unsetenv("NOTIFY_SOCKET");

  int fd;
#if defined(SOCK_CLOEXEC)
  fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, /* protocol = */ 0);
#else
  fd = socket(AF_UNIX, SOCK_DGRAM, /* protocol = */ 0);
#endif
  if (fd < 0) {
    ERROR("creating UNIX socket failed: %s", STRERRNO);
    return 0;
  }

  struct sockaddr_un su = {0};
  su.sun_family = AF_UNIX;
  if (notifysocket[0] != '@') {
    /* regular UNIX socket */
    sstrncpy(su.sun_path, notifysocket, sizeof(su.sun_path));
    su_size = sizeof(su);
  } else {
    /* Linux abstract namespace socket: specify address as "\0foo", i.e.
     * start with a null byte. Since null bytes have no special meaning in
     * that case, we have to set su_size correctly to cover only the bytes
     * that are part of the address. */
    sstrncpy(su.sun_path, notifysocket, sizeof(su.sun_path));
    su.sun_path[0] = 0;
    su_size = sizeof(sa_family_t) + strlen(notifysocket);
    if (su_size > sizeof(su))
      su_size = sizeof(su);
  }

  const char buffer[] = "READY=1\n";
  if (sendto(fd, buffer, strlen(buffer), MSG_NOSIGNAL, (void *)&su,
             (socklen_t)su_size) < 0) {
    ERROR("sendto(\"%s\") failed: %s", notifysocket, STRERRNO);
    close(fd);
    return 0;
  }

  unsetenv("NOTIFY_SOCKET");
  close(fd);
  return 1;
}
#endif /* KERNEL_LINUX */

struct cmdline_config {
  bool test_config;
  bool test_readall;
  bool create_basedir;
  const char *configfile;
  bool daemonize;
};

=======
>>>>>>> 95389ffa1005b6e450e62e66cf4a97cdbf7779b8
static void read_cmdline(int argc, char **argv, struct cmdline_config *config) {
  /* read options */
  while (1) {
    int c = getopt(argc, argv, "BhtTC:"
#if COLLECT_DAEMON
                               "fP:"
#endif
                   );

    if (c == -1)
      break;

    switch (c) {
    case 'B':
      config->create_basedir = false;
      break;
    case 'C':
      config->configfile = optarg;
      break;
    case 't':
      config->test_config = true;
      break;
    case 'T':
      config->test_readall = true;
      global_option_set("ReadThreads", "-1", 1);
#if COLLECT_DAEMON
      config->daemonize = false;
#endif /* COLLECT_DAEMON */
      break;
#if COLLECT_DAEMON
    case 'P':
      global_option_set("PIDFile", optarg, 1);
      break;
    case 'f':
      config->daemonize = false;
      break;
#endif /* COLLECT_DAEMON */
    case 'h':
      exit_usage(0);
    default:
      exit_usage(1);
    } /* switch (c) */
  }   /* while (1) */
}

static int configure_collectd(struct cmdline_config *config) {
  /*
   * Read options from the config file, the environment and the command
   * line (in that order, with later options overwriting previous ones in
   * general).
   * Also, this will automatically load modules.
   */
  if (cf_read(config->configfile)) {
    fprintf(stderr, "Error: Reading the config file failed!\n"
                    "Read the logs for details.\n");
    return 1;
  }

  /*
   * Change directory. We do this _after_ reading the config and loading
   * modules to relative paths work as expected.
   */
  const char *basedir;
  if ((basedir = global_option_get("BaseDir")) == NULL) {
    fprintf(stderr,
            "Don't have a basedir to use. This should not happen. Ever.");
    return 1;
  } else if (change_basedir(basedir, config->create_basedir)) {
    fprintf(stderr, "Error: Unable to change to directory `%s'.\n", basedir);
    return 1;
  }

  /*
   * Set global variables or, if that fails, exit. We cannot run with
   * them being uninitialized. If nothing is configured, then defaults
   * are being used. So this means that the user has actually done
   * something wrong.
   */
  if (init_global_variables() != 0)
    return 1;

  return 0;
}

<<<<<<< HEAD
int main(int argc, char **argv) {
  int exit_status = 0;
=======
void stop_collectd(void) { loop++; }
>>>>>>> 95389ffa1005b6e450e62e66cf4a97cdbf7779b8

struct cmdline_config init_config(int argc, char **argv) {
  struct cmdline_config config = {
      .daemonize = true, .create_basedir = true, .configfile = CONFIGFILE,
  };

  read_cmdline(argc, argv, &config);

  if (config.test_config)
    exit(EXIT_SUCCESS);

  if (optind < argc)
    exit_usage(1);

  plugin_init_ctx();

  if (configure_collectd(&config) != 0)
    exit(EXIT_FAILURE);

<<<<<<< HEAD
#if COLLECT_DAEMON
  /*
   * fork off child
   */
  struct sigaction sig_chld_action = {.sa_handler = SIG_IGN};

  sigaction(SIGCHLD, &sig_chld_action, NULL);

  /*
   * Only daemonize if we're not being supervised
   * by upstart or systemd (when using Linux).
   */
  if (config.daemonize
#ifdef KERNEL_LINUX
      && notify_upstart() == 0 && notify_systemd() == 0
#endif
      ) {
    pid_t pid;
    if ((pid = fork()) == -1) {
      /* error */
      fprintf(stderr, "fork: %s", STRERRNO);
      return 1;
    } else if (pid != 0) {
      /* parent */
      /* printf ("Running (PID %i)\n", pid); */
      return 0;
    }

    /* Detach from session */
    setsid();

    /* Write pidfile */
    if (pidfile_create())
      exit(2);

    /* close standard descriptors */
    close(2);
    close(1);
    close(0);

    int status = open("/dev/null", O_RDWR);
    if (status != 0) {
      ERROR("Error: Could not connect `STDIN' to `/dev/null' (status %d)",
            status);
      return 1;
    }

    status = dup(0);
    if (status != 1) {
      ERROR("Error: Could not connect `STDOUT' to `/dev/null' (status %d)",
            status);
      return 1;
    }

    status = dup(0);
    if (status != 2) {
      ERROR("Error: Could not connect `STDERR' to `/dev/null', (status %d)",
            status);
      return 1;
    }
  }    /* if (config.daemonize) */
#endif /* COLLECT_DAEMON */

  struct sigaction sig_pipe_action = {.sa_handler = SIG_IGN};

  sigaction(SIGPIPE, &sig_pipe_action, NULL);

  /*
   * install signal handlers
   */
  struct sigaction sig_int_action = {.sa_handler = sig_int_handler};

  if (sigaction(SIGINT, &sig_int_action, NULL) != 0) {
    ERROR("Error: Failed to install a signal handler for signal INT: %s",
          STRERRNO);
    return 1;
  }

  struct sigaction sig_term_action = {.sa_handler = sig_term_handler};

  if (sigaction(SIGTERM, &sig_term_action, NULL) != 0) {
    ERROR("Error: Failed to install a signal handler for signal TERM: %s",
          STRERRNO);
    return 1;
  }

  struct sigaction sig_usr1_action = {.sa_handler = sig_usr1_handler};

  if (sigaction(SIGUSR1, &sig_usr1_action, NULL) != 0) {
    ERROR("Error: Failed to install a signal handler for signal USR1: %s",
          STRERRNO);
    return 1;
  }
=======
  return config;
}

int run_loop(bool test_readall) {
  int exit_status = 0;
>>>>>>> 95389ffa1005b6e450e62e66cf4a97cdbf7779b8

  if (do_init() != 0) {
    ERROR("Error: one or more plugin init callbacks failed.");
    exit_status = 1;
  }

  if (test_readall) {
    if (plugin_read_all_once() != 0) {
      ERROR("Error: one or more plugin read callbacks failed.");
      exit_status = 1;
    }
  } else {
    INFO("Initialization complete, entering read-loop.");
    do_loop();
  }

  /* close syslog */
  INFO("Exiting normally.");

  if (do_shutdown() != 0) {
    ERROR("Error: one or more plugin shutdown callbacks failed.");
    exit_status = 1;
  }

  return exit_status;
} /* int run_loop */
