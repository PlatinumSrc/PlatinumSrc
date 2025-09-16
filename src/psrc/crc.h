#ifndef PSRC_CRC_H
#define PSRC_CRC_H

#include <stdint.h>
#include <stddef.h>

uint32_t crc32(const void*, size_t);
uint64_t crc64(const void*, size_t);
uint32_t strcrc32(const char*);
uint64_t strcrc64(const char*);
uint32_t strcasecrc32(const char*);
uint64_t strcasecrc64(const char*);
uint32_t strncasecrc32(const char*, size_t);
uint64_t strncasecrc64(const char*, size_t);
uint32_t ccrc32(uint32_t, const void*, size_t);
uint64_t ccrc64(uint64_t, const void*, size_t);
uint32_t cstrcrc32(uint32_t, const char*);
uint64_t cstrcrc64(uint64_t, const char*);
uint32_t cstrcasecrc32(uint32_t, const char*);
uint64_t cstrcasecrc64(uint64_t, const char*);
uint32_t cstrncasecrc32(uint32_t, const char*, size_t);
uint64_t cstrncasecrc64(uint64_t, const char*, size_t);

#endif
