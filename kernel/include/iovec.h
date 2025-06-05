#ifndef __IOVEC_H
#define __IOVEC_H

#include "types.h"

struct iovec {
    void *iov_base; /* Starting address */
    size_t iov_len; /* Number of bytes to transfer */
};

#endif
