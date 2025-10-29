#if defined (__cplusplus)
extern "C" {
#endif

#ifndef LZ4CB_H
#define LZ4CB_H

#include "lz4.h"
#include "lz4frame_static.h"

typedef size_t (*LZ4CB_readcb)(void* ctx, size_t len, void* buf);
typedef size_t (*LZ4CB_writecb)(void* ctx, size_t len, void* buf);

typedef struct LZ4_readCB_s {
  LZ4F_dctx* dctxPtr;
  LZ4CB_readcb cb;
  void* ctx;
  LZ4_byte* srcBuf;
  size_t srcBufNext;
  size_t srcBufSize;
  size_t srcBufMaxSize;
} LZ4_readCB_t;
typedef struct LZ4_writeCB_s {
  LZ4F_cctx* cctxPtr;
  LZ4CB_writecb cb;
  void* ctx;
  LZ4_byte* dstBuf;
  size_t maxWriteSize;
  size_t dstBufMaxSize;
  LZ4F_errorCode_t errCode;
} LZ4_writeCB_t;

LZ4FLIB_STATIC_API LZ4F_errorCode_t LZ4CB_readOpen(LZ4_readCB_t* lz4cbRead);
LZ4FLIB_STATIC_API size_t LZ4CB_read(LZ4_readCB_t* lz4cbRead, void* buf, size_t size);
LZ4FLIB_STATIC_API LZ4F_errorCode_t LZ4CB_readClose(LZ4_readCB_t* lz4cbRead);

LZ4FLIB_STATIC_API LZ4F_errorCode_t LZ4CB_writeOpen(LZ4_writeCB_t* lz4cbWrite, const LZ4F_preferences_t* prefsPtr);
LZ4FLIB_STATIC_API size_t LZ4CB_write(LZ4_writeCB_t* lz4cbWrite, const void* buf, size_t size);
LZ4FLIB_STATIC_API LZ4F_errorCode_t LZ4CB_writeClose(LZ4_writeCB_t* lz4cbWrite);

#endif /* LZ4FILE_H */

#if defined (__cplusplus)
}
#endif
