#ifndef GLOBALDEF_H
#define GLOBALDEF_H

#ifndef GLOBALDEF_H
#define	GLOBALDEF_H

#define TRUE 1
#define FALSE 0

typedef unsigned char byte;
typedef unsigned short word;

#ifdef __GNUC__
typedef char int8;
typedef short int16;
typedef int int32;
typedef long int64;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

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
#endif	/* GLOBALDEF_H */

#define PROGRAM_REVISION 0

#endif
