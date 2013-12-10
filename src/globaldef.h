#ifndef GLOBALDEF_H
#define GLOBALDEF_H

#include <cstdint>

#define TRUE 1
#define FALSE 0

typedef unsigned char byte;
typedef unsigned short word;

#ifdef __GNUC__
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#else // Assume this is XC8
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;

typedef char int8;
typedef short int16;
typedef long int32;
#endif

#ifdef __GNUC__
#define PACKED_STRUCT __attribute__((packed))
#else
#define PACKED_STRUCT
#endif
//#endif	/* GLOBALDEF_H */

#define PROGRAM_REVISION 0

#endif
