#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

static const char *prog;

static int quit = 0;

static struct {
    int sig;
    struct sigaction old;
} sigconfig[] = {
    { SIGINT,  {{0}, {{0}}, 0, 0} },
    { SIGTERM, {{0}, {{0}}, 0, 0} },
    { -1,      {{0}, {{0}}, 0, 0} },
};

static void sighandler(int sig)
{
    (void) sig;	/* unused */
    quit = 1;
}

static void sigconfigure()
{
    struct sigaction act;

    for (int i = 0; sigconfig[i].sig >= 0; i++) {
        int signum = sigconfig[i].sig;

        sigaction(signum, NULL, &act);
        sigconfig[i].old = act;

        for (int j = 0; sigconfig[j].sig >= 0; j++)
            sigaddset(&act.sa_mask, sigconfig[j].sig);

        act.sa_flags &= ~SA_NODEFER;
        act.sa_flags &= ~SA_ONSTACK;
        act.sa_flags &= ~SA_RESETHAND;
        act.sa_flags &= ~SA_RESTART;
        act.sa_flags &= ~SA_SIGINFO;
        act.sa_handler = sighandler;

        sigaction(signum, &act, NULL);
    }
}

static void sigrestore()
{
    for (int i = 0; sigconfig[i].sig >= 0; i++) {
        sigaction(sigconfig[i].sig, &sigconfig[i].old, NULL);
    }
}

__attribute__ ((format (printf, 1, 2)))
static void errorf(const char *format, ...)
{
    char c, *buf;
    int size;
    va_list ap;

    if (format) {
        c = format[0];
        if (c == '~')
            format++;
    } else
        return;

    va_start(ap, format);
    size = vasprintf(&buf, format, ap);
    if (size > 0)
        fprintf(stderr, "%s: ERROR: %s\n", prog, buf);
    if (size != -1)
        free(buf);
    va_end(ap);

    if (c == '~')
        return;
    exit(1);
}

__attribute__ ((format (printf, 1, 2)))
static void perrorf(const char *format, ...)
{
    int e = errno;
    char c, *buf;
    int size;
    va_list ap;

    if (format) {
        c = format[0];
        if (c == '~')
            format++;
    } else
        return;

    va_start(ap, format);
    size = vasprintf(&buf, format, ap);
    if (size > 0)
        fprintf(stderr, "%s: ERROR: %s: %s\n", prog, buf, sys_errlist[e]);
    if (size != -1)
        free(buf);
    va_end(ap);

    if (c == '~')
        return;
    exit(1);
}

struct cmd {
    struct cmd *prev;
    struct cmd *next;
    char **argv;
    int pipe[2];
    pid_t pid;
    struct {
        int fd[2][2];
    } sync;
};

static struct cmd *start = NULL;

static struct cmd *newcmd(char *argv[])
{
    struct cmd *new;
    int ret;

    new = malloc(sizeof(struct cmd));
    if (new == NULL)
        perrorf("malloc()");
    memset(new, 0, sizeof(struct cmd));

    new->argv = argv;

    ret = pipe(new->pipe);
    if (ret != 0)
        perrorf("pipe()");

    ret = pipe2(new->sync.fd[0], O_CLOEXEC);
    if (ret != 0)
        perrorf("pipe2(O_CLOEXEC)");

    ret = pipe2(new->sync.fd[1], 0);
    if (ret != 0)
        perrorf("pipe2()");

    if (start == NULL) {
        new->prev = new;
        new->next = new;
        start = new;
    } else {
        new->prev = start->prev;
        new->next = start;
        start->prev->next = new;
        start->prev = new;
    }
    return new;
}

static void free_cmds()
{
    static struct cmd *p, *tmp;

    p = start;
    do {
        tmp = p->next;
        free(p);
        p = tmp;
    } while (p != start);
    start = NULL;
}

#define foreach(start, var)	for (struct cmd *_flag = NULL, *var = (start); \
				     var != (_flag == NULL ? NULL : (start));  \
				     _flag = (struct cmd *) 1, var = var->next)

/* returns sync fd for read() on child side */
static int crfd(struct cmd *p)
{
    return p->sync.fd[1][0];
}

/* returns sync fd for write() on child side */
static int cwfd(struct cmd *p)
{
    return p->sync.fd[0][1];
}

/* returns sync fd for read() on parent side */
static int prfd(struct cmd *p)
{
    return p->sync.fd[0][0];
}

/* returns sync fd for write() on parent side */
static int pwfd(struct cmd *p)
{
    return p->sync.fd[1][1];
}

static int read_one_byte(int fd, int exp)
{
    char buf[1];
    int n;

    n = read(fd, buf, 1);
    if (n < 0)
        perrorf("read()");
    if (n != 1)
        errorf("read() returned %d bytes, expected 1", n);
    if (exp >= 0 && buf[0] != exp)
        errorf("read() returned char(0x%02x), expected '%c' (0x%02x)",
               buf[0], exp, exp);
    return buf[0];
}

static const char *const default_separator = "--";
static const struct option long_options[] = {
    { "help",      no_argument,       NULL, 'h' },
    { "separator", required_argument, NULL, 's' },
    { NULL, 0, NULL, 0 }
};

static void usage(FILE *out, int code)
{
    fprintf(out,
"usage: %s [-s str] [--] cmd1 [args ...] -- cmd2 [args ..] [-- cmd3 ...]]\n"
"options:\n"
"    --help, -h               print this usage message and exit\n"
"    --separator str, -s str  use str as command separator (default is '%s')\n"
"remarks:\n"
"    Even though it is really confusing, if a separator is used immediately\n"
"    befor the first command (cmd1), it must be strictly '--' and not the one\n"
"    specified by the --separator or -s option. This limitation is due to the\n"
"    getopt_long(3) function.\n",
    prog, default_separator);
    exit(code);
}

int main(int argc, char *argv[])
{
    int ret, i, n, m, ncmds, exit_code = 0;
    char *separator = (char *) default_separator;
    char buf[1];

    prog = strrchr(argv[0], '/');
    if (prog)
        prog++;
    else
        prog = argv[0];

    optind = 0;
    do {
        opterr = 0;
        ret = getopt_long(argc, argv, "+:hs:", long_options, NULL);
        switch (ret) {
        case 'h':
            usage(stdout, 0);
        case 's':
            separator = argv[optind - 1];
            break;
        case '?':
            errorf("unknown option: %s", argv[optind - 1]);
        case ':':
            errorf("option requires an argument: %s", argv[optind - 1]);
        case -1:
            break;
        default:
            errorf("getopt_long() returns unexpected value %d", ret);
        }
    } while (ret != -1);
    argv += optind;

    ncmds = 0;
    while (1) {
        if (*argv == NULL)
            usage(stderr, 1);

        newcmd(argv);
        ncmds++;

        for (argv++; *argv; argv++)
            if (strcmp(*argv, separator) == 0)
                break;
        if (*argv == NULL)
            break;
        else
            *argv++ = NULL;
    }

#ifdef DEBUG
    foreach(start, p)
        for (i = 0; p->argv[i]; i++)
            printf("p->argv[%d]=|%s|\n", i, p->argv[i]);
#endif

    sigconfigure();

    foreach(start, p) {
        p->pid = fork();
        if (p->pid < 0)
            perrorf("fork()");

        if (p->pid == 0) {
            /* child side */

            sigrestore();

            ret = dup2(p->prev->pipe[0], 0);
            if (ret < 0)
                perrorf("pipe()");
            ret = dup2(p->pipe[1], 1);
            if (ret < 0)
                perrorf("pipe()");
            argv = p->argv;

            foreach(p, e) {
                close(e->pipe[0]);
                close(e->pipe[1]);

                close(prfd(e));
                if (p != e) {
                   close(cwfd(e));
                   close(crfd(e));
                }
                close(pwfd(e));
            }

            /* この子プロセスでの配管が終わったことを通知 */
            buf[0] = 'P';
            n = write(cwfd(p), buf, 1);
            if (n != 1)
                errorf("write()");
            /* O_CLOEXEC will close cwfd(p) */

            /* 全配管終了の通知待ち */
            n = read(crfd(p), buf, 1);
            if (n != 0)
                errorf("read()");
            close(crfd(p));

            free_cmds();

            execvp(argv[0], argv);
            perrorf("~execvp(%s)", argv[0]);

            /* コマンド起動失敗を通知 */
            buf[0] = 'E';
            n = write(cwfd(p), buf, 1);
            if (n != 1)
                errorf("write()");
            close(cwfd(p));
            exit(127);
        }
    }
    /* parent side */

    foreach(start, p) {
        close(p->pipe[0]);
        close(p->pipe[1]);
        close(cwfd(p));
        close(crfd(p));
    }

    struct pollfd *fds = malloc(sizeof(struct pollfd) * ncmds);
    if (fds == NULL)
        errorf("malloc()");

    /* 個々の子プロセスでの配管終了通知待ち */
    i = 0;
    foreach(start, p) {
        fds[i].fd = prfd(p);
        fds[i].events = POLLIN | POLLHUP;
        fds[i].revents = 0;
        i++;
    }
    m = ncmds;
    while (m > 0) {
        ret = poll(fds, ncmds, -1);
        if (ret < 0)
            perrorf("poll()");

        for (i = 0; i < ncmds; i++) {
            if (fds[i].revents == 0)
                continue;

            if (fds[i].revents == POLLIN) {
                read_one_byte(fds[i].fd, 'P');
                fds[i].fd = -1;
                fds[i].revents = 0;
                m--;
                continue;
            }
            errorf("poll() returned unexpected events 0x%04x", fds[i].revents);
        }
    }

    /* 全配管終了を全子プロセスに通知 */
    foreach(start, p)
        close(pwfd(p));

    /* 個々の子プロセスからコマンド起動結果通知待ち */
    i = 0;
    foreach(start, p) {
        fds[i].fd = prfd(p);
        fds[i].events = POLLIN | POLLHUP;
        fds[i].revents = 0;
        i++;
    }
    m = ncmds;
    while (m > 0) {
        ret = poll(fds, ncmds, -1);
        if (ret < 0)
            perrorf("poll()");

        for (i = 0; i < ncmds; i++) {
            if (fds[i].revents == 0)
                continue;

            if (fds[i].revents & POLLHUP)
                m--;

            if (fds[i].revents & POLLIN) {
                read_one_byte(fds[i].fd, 'E');
                m = 0;
                quit = 1;
                exit_code = 127;
                break;
            }

            if (fds[i].revents & (POLLHUP | POLLIN)) {
                fds[i].fd = -1;
                fds[i].revents = 0;
                continue;
            }
            errorf("poll() returned unexpected events 0x%04x", fds[i].revents);
        }
    }
    free(fds);

    foreach(start, p)
        close(prfd(p));

    /* fork() からここまでのシーケンス図的なもの                                
                                                                                
                       |                                                        
                     fork() --------------------+ child                         
                       |                        |                               
                parent |                      dup2() ディスクリプタの           
                       |                        |    スゲカエ (配管)            
                       |                        |                               
    不要なディスクリ close()                  close() 不要なディスク            
    クリプタを閉じる   |                        |     リプタを閉じる            
                       |                        |                               
                       |                        |  自分の配管終了を通知         
                     poll() <------------- write(cwfd(), 'P')                   
                       |                        |                               
                 read(prfd(), 'P')              |                               
                       |                        |                               
    全配管完了を通知   |                        |                               
                 close(pwfd()) ----------> read(crfd(), buf) = EOF              
                       |                        |                               
                       |                   close(crfd()))                       
                       |                        |                               
                       |               +----- exec()    cwfd() には O_CLOEXEC   
                       |          成功 |        |       がセットされているの    
                       |               |        |       で自動的に閉じられる    
                       |               |        | 失敗                          
           POLLHUP     |               v        |                               
             +------ poll() <------------- write(cwfd(), 'E') 失敗を通知        
             |         |                        |                               
             |         | POLLIN             close(cwfd())                       
             |         |                        |                               
             |   read(prfd(), 'E')           exit(127)                          
             |         |                                                        
             +-------> |                                                        
                       |                                                        
                  close(prfd())                                                 
                       |                                                        
                       v                                                        
                                                                                
                crfd(p) { return p->sync.fd[1][0]; }                            
                cwfd(p) { return p->sync.fd[0][1]; }                            
                prfd(p) { return p->sync.fd[0][0]; }                            
                pwfd(p) { return p->sync.fd[1][1]; }                            
    */

    int t = 0;
    int options = 0;
    m = ncmds;
    do {
        int status;

        if (quit && t == 0) {
            t++;
            foreach(start, p)
                if (p->pid > 0)
                    kill(p->pid, SIGTERM);	/* first */

            options |= WNOHANG;
        }

        ret = waitpid(-1, &status, options);

        if (ret < 0) {
            if (errno == EINTR)
                continue;
            perrorf("wait()");
        }

        if (t > 0 && ret == 0) {
            t++;
            if (t == 10)
                foreach(start, p)
                    if (p->pid > 0)
                        kill(p->pid, SIGTERM);	/* second */
            if (t == 20)
                foreach(start, p)
                    if (p->pid > 0)
                        kill(p->pid, SIGKILL);	/* third (last) */

            usleep(0.1 * 1000 * 1000);		/* wait for 0.1 sec */
            continue;
        }

        foreach(start, p) {
            if (p->pid == ret && (WIFEXITED(status) || WIFSIGNALED(status))) {
                p->pid = ret = -1;
                break;
            }
        }
        if (ret != -1)
            errorf("unexpected return from waitpid(): %d", ret);

        m--;
    } while (m > 0);
    free_cmds();

    return exit_code;
}

/* TODO & Known Bugs

- SIGINT, SIGTERM をブロックする子プロセスがいてもちゃんと止まることを確認

*/

/* ----- 8< -------- 8< -------- 8< -------- 8< -------- 8< -------- 8< --------
nc (netcat) のオンラインマニュアル記載の使用例に触発されて作成した。例では名前
付き FIFO を使用しているがこのプログラムを使用するとその部分も名前なしパイプで
実現できる。それ以上のメリットはないと思う

nc の使用例のと同じことをこのプログラムで実現:

    server$ ring-pipe /bin/sh -c 'exec /bin/sh -i 2>&1' -- nc -l localhost 1234
    client$ nc localhost 1234

    2>&1 の処理がエレガントではない。こうなることは今気がついた。sh -i を実行す
    ると SIGINT とかが効かなくなるので注意

つまらない使用例 (その 1):

    server$ ring-pipe nc -l localhost 1234
    client$ nc localhost 1234

    これはただのエコーサーバー

つまらない使用例 (その 2):

    server$ ring-pipe nc -l localhost 1234 -- cat -- cat -- cat -- cat \
            -- cat -- cat -- cat -- stdbuf -i0 -o0 tr 'a-zA-Z' 'A-Za-z'
    client$ nc localhost 1234

    入出力を数珠のようにつなげることができる。tr は stdbuf でバッファリングを
    解除しないとダメ。コマンドを 3 つ以上つなげて意味のある例が思いつかない


コンパイル例
------------
gcc -Wall -Wextra -Werror -std=c99 -o ring-pipe ring-pipe.c


Makefile 例
-----------
cflags	= -Wall -Wextra -Werror
extra	=
target	= ring-pipe
source	= $(target).c

$(target): $(source)
	gcc $(cflags) $(extra) -std=c99 -o $@ $^

.PHONY:	clean test1 test2 test3
clean:
	rm -f $(target) a.out *.o

test1:	$(target)
	which nc
	./$(target) nc -l localhost 1234

test2:	$(target)
	which nc
	./$(target) /bin/sh -c 'exec /bin/sh -i 2>&1' -- nc -l localhost 1234

test3:	$(target)
	which nc
	./$(target) --separator +++			\
		-- nc -l localhost 1234			\
		+++ cat					\
		+++ cat					\
		+++ stdbuf -i0 -o0 tr 'a-zA-Z' 'A-Za-z'	\
		+++ cat
-------- >8 -------- >8 -------- >8 -------- >8 -------- >8 -------- >8 ----- */
