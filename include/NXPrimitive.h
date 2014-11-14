#ifndef _NXPRIMITIVE_H
#define _NXPRIMITIVE_H

#ifndef likely
#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)
#endif

#endif
