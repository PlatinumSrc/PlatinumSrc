#if defined (__cplusplus)
extern "C" {
#endif

#ifndef LZ4DS_H
#define LZ4DS_H

#include "../psrc/common/datastream.h"  /* PSRC_DATASTREAM_T */
#include "lz4frame_static.h"

typedef struct LZ4_readDS_s LZ4_readDS_t;
//typedef struct LZ4_writeFile_s LZ4_writeFile_t;

/*! LZ4DS_readOpen() :
 * Set read lz4file handle.
 * `lz4f` will set a lz4file handle.
 * `fp` must be the return value of the lz4 file opened by fopen.
 */
LZ4FLIB_STATIC_API LZ4F_errorCode_t LZ4DS_readOpen(LZ4_readDS_t** lz4dsRead, PSRC_DATASTREAM_T ds);

/*! LZ4F_read() :
 * Read lz4file content to buffer.
 * `lz4f` must use LZ4_readOpen to set first.
 * `buf` read data buffer.
 * `size` read data buffer size.
 */
LZ4FLIB_STATIC_API size_t LZ4DS_read(LZ4_readDS_t* lz4dsRead, void* buf, size_t size);

/*! LZ4F_readClose() :
 * Close lz4file handle.
 * `lz4f` must use LZ4_readOpen to set first.
 */
LZ4FLIB_STATIC_API LZ4F_errorCode_t LZ4DS_readClose(LZ4_readDS_t* lz4dsRead);

#if 0

/*! LZ4F_writeOpen() :
 * Set write lz4file handle.
 * `lz4f` will set a lz4file handle.
 * `fp` must be the return value of the lz4 file opened by fopen.
 */
LZ4FLIB_STATIC_API LZ4F_errorCode_t LZ4F_writeOpen(LZ4_writeFile_t** lz4fWrite, FILE* fp, const LZ4F_preferences_t* prefsPtr);

/*! LZ4F_write() :
 * Write buffer to lz4file.
 * `lz4f` must use LZ4F_writeOpen to set first.
 * `buf` write data buffer.
 * `size` write data buffer size.
 */
LZ4FLIB_STATIC_API size_t LZ4F_write(LZ4_writeFile_t* lz4fWrite, const void* buf, size_t size);

/*! LZ4F_writeClose() :
 * Close lz4file handle.
 * `lz4f` must use LZ4F_writeOpen to set first.
 */
LZ4FLIB_STATIC_API LZ4F_errorCode_t LZ4F_writeClose(LZ4_writeFile_t* lz4fWrite);

#endif

#endif /* LZ4FILE_H */

#if defined (__cplusplus)
}
#endif
