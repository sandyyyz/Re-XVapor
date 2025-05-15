#ifndef __WINSIZE_H
#define __WINSIZE_H
#include "types.h"

struct winsize
{
    uint16 ws_row;    /* rows， in character */
    uint16 ws_col;    /* columns, in characters */
    uint16 ws_xpixel; /* horizontal size, pixels (unused) */
    uint16 ws_ypixel; /* vertical size, pixels (unused) */
};

#endif /* __WINSIZE_H */