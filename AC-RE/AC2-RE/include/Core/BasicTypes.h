#pragma once

#include <cstdint>
#include <cstddef>

typedef unsigned char		byte;		// 8 bits
typedef unsigned short		word;		// 16 bits
typedef unsigned int		dword;		// 32 bits
typedef unsigned int		uint;
typedef unsigned long		ulong;

typedef signed char			int8;
typedef unsigned char		uint8;
typedef short int			int16;
typedef unsigned short int	uint16;
typedef int					int32;
typedef unsigned int		uint32;
typedef long long			int64;
typedef unsigned long long	uint64;

#define assert_sizeof(type, size) static_assert(sizeof(type) == size, "Size mismatch for " #type)
#define assert_offsetof(type, field, offset) static_assert(offsetof(type, field) == offset, "Offset mismatch for " #type "::" #field)

assert_sizeof(bool, 1);
assert_sizeof(char, 1);
assert_sizeof(short, 2);
assert_sizeof(int, 4);
assert_sizeof(float, 4);
assert_sizeof(byte, 1);
assert_sizeof(int8, 1);
assert_sizeof(uint8, 1);
assert_sizeof(int16, 2);
assert_sizeof(uint16, 2);
assert_sizeof(int32, 4);
assert_sizeof(uint32, 4);
assert_sizeof(int64, 8);
assert_sizeof(uint64, 8);
