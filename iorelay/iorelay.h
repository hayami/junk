/* '##__VA_ARGS__' is a GNU C extention */
#define  NARGS(...)	_NARGS(0, ##__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define _NARGS(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...)	N

/* SYNOPSIS
        int iorelay(int rfd, int wfd1, ...);

   DESCRIPTION
        The iorelay() function reads data from file descriptor specified by
        the rfd and writes it to file descriptors specified after the second
        argument (wfd1, ...). It terminates internal thread when it can no
        longer be read from the rfd or when all file descriptors specified
        after the second argument can not be written. All file descriptors
        that are not prefixed with a tilde (~) operator are closed on thread
        eixt.

   EXAMPLE
        ret = iorelay(rfd, wfd1, ~wfd2);

        The rfd and the wfd1 will be closed on internal thread exit, but the
        wfd2 will not be closed by the iorelay().

*/
#define  iorelay(rfd, ...)   viorelay(rfd, __VA_ARGS__)
#define viorelay(rfd, ...) _vniorelay(rfd, NARGS(__VA_ARGS__), __VA_ARGS__)
int   _vniorelay(int rfd, int nwfds, ...);
int     niorelay(int rfd, int nwfds, int wfd[]);

/* vim: set et sw=4 sts=4: */
