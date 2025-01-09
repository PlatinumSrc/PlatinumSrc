#include <stdlib.h>  /* malloc, free */
#include <string.h>
#include <assert.h>
#include "lz4.h"
#include "lz4cb.h"

static LZ4F_errorCode_t returnErrorCode(LZ4F_errorCodes code)
{
    return (LZ4F_errorCode_t)-(ptrdiff_t)code;
}
#undef RETURN_ERROR
#define RETURN_ERROR(e) return returnErrorCode(LZ4F_ERROR_ ## e)

/* =====   read API   ===== */

struct LZ4_readCB_s {
  LZ4F_dctx* dctxPtr;
  LZ4CB_readcb cb;
  void* ctx;
  LZ4_byte* srcBuf;
  size_t srcBufNext;
  size_t srcBufSize;
  size_t srcBufMaxSize;
};

static void LZ4CB_freeReadCB(LZ4_readCB_t* lz4cbRead)
{
  if (lz4cbRead==NULL) return;
  LZ4F_freeDecompressionContext(lz4cbRead->dctxPtr);
  free(lz4cbRead->srcBuf);
  free(lz4cbRead);
}

static void LZ4CB_freeAndNullReadCB(LZ4_readCB_t** statePtr)
{
  assert(statePtr != NULL);
  LZ4CB_freeReadCB(*statePtr);
  *statePtr = NULL;
}

LZ4F_errorCode_t LZ4CB_readOpen(LZ4_readCB_t** lz4cbRead, LZ4CB_readcb cb, void* ctx)
{
  char buf[LZ4F_HEADER_SIZE_MAX];
  size_t consumedSize;
  LZ4F_errorCode_t ret;

  if (cb == NULL || lz4cbRead == NULL) {
    RETURN_ERROR(parameter_null);
  }

  *lz4cbRead = (LZ4_readCB_t*)calloc(1, sizeof(LZ4_readCB_t));
  if (*lz4cbRead == NULL) {
    RETURN_ERROR(allocation_failed);
  }

  ret = LZ4F_createDecompressionContext(&(*lz4cbRead)->dctxPtr, LZ4F_VERSION);
  if (LZ4F_isError(ret)) {
    LZ4CB_freeAndNullReadCB(lz4cbRead);
    return ret;
  }

  (*lz4cbRead)->cb = cb;
  (*lz4cbRead)->ctx = ctx;
  consumedSize = cb(ctx, sizeof(buf), buf);
  if (consumedSize < LZ4F_HEADER_SIZE_MIN + LZ4F_ENDMARK_SIZE) {
    LZ4CB_freeAndNullReadCB(lz4cbRead);
    RETURN_ERROR(io_read);
  }

  { LZ4F_frameInfo_t info;
    LZ4F_errorCode_t const r = LZ4F_getFrameInfo((*lz4cbRead)->dctxPtr, &info, buf, &consumedSize);
    if (LZ4F_isError(r)) {
      LZ4CB_freeAndNullReadCB(lz4cbRead);
      return r;
    }

    switch (info.blockSizeID) {
      case LZ4F_default :
      case LZ4F_max64KB :
        (*lz4cbRead)->srcBufMaxSize = 64 * 1024;
        break;
      case LZ4F_max256KB:
        (*lz4cbRead)->srcBufMaxSize = 256 * 1024;
        break;
      case LZ4F_max1MB:
        (*lz4cbRead)->srcBufMaxSize = 1 * 1024 * 1024;
        break;
      case LZ4F_max4MB:
        (*lz4cbRead)->srcBufMaxSize = 4 * 1024 * 1024;
        break;
      default:
        LZ4CB_freeAndNullReadCB(lz4cbRead);
        RETURN_ERROR(maxBlockSize_invalid);
    }
  }

  (*lz4cbRead)->srcBuf = (LZ4_byte*)malloc((*lz4cbRead)->srcBufMaxSize);
  if ((*lz4cbRead)->srcBuf == NULL) {
    LZ4CB_freeAndNullReadCB(lz4cbRead);
    RETURN_ERROR(allocation_failed);
  }

  (*lz4cbRead)->srcBufSize = sizeof(buf) - consumedSize;
  memcpy((*lz4cbRead)->srcBuf, buf + consumedSize, (*lz4cbRead)->srcBufSize);

  return ret;
}

size_t LZ4CB_read(LZ4_readCB_t* lz4cbRead, void* buf, size_t size)
{
  LZ4_byte* p = (LZ4_byte*)buf;
  size_t next = 0;

  if (lz4cbRead == NULL || buf == NULL)
    RETURN_ERROR(parameter_null);

  while (next < size) {
    size_t srcsize = lz4cbRead->srcBufSize - lz4cbRead->srcBufNext;
    size_t dstsize = size - next;
    size_t ret;

    if (srcsize == 0) {
      ret = lz4cbRead->cb(lz4cbRead->ctx, lz4cbRead->srcBufMaxSize, lz4cbRead->srcBuf);
      if (ret > 0) {
        lz4cbRead->srcBufSize = ret;
        srcsize = lz4cbRead->srcBufSize;
        lz4cbRead->srcBufNext = 0;
      } else if (ret == 0) {
        break;
      } else {
        RETURN_ERROR(io_read);
      }
    }

    ret = LZ4F_decompress(lz4cbRead->dctxPtr,
                          p, &dstsize,
                          lz4cbRead->srcBuf + lz4cbRead->srcBufNext,
                          &srcsize,
                          NULL);
    if (LZ4F_isError(ret)) {
        return ret;
    }

    lz4cbRead->srcBufNext += srcsize;
    next += dstsize;
    p += dstsize;
  }

  return next;
}

LZ4F_errorCode_t LZ4CB_readClose(LZ4_readCB_t* lz4cbRead)
{
  if (lz4cbRead == NULL)
    RETURN_ERROR(parameter_null);
  LZ4CB_freeReadCB(lz4cbRead);
  return LZ4F_OK_NoError;
}

/* =====   write API   ===== */

struct LZ4_writeCB_s {
  LZ4F_cctx* cctxPtr;
  LZ4CB_writecb cb;
  void* ctx;
  LZ4_byte* dstBuf;
  size_t maxWriteSize;
  size_t dstBufMaxSize;
  LZ4F_errorCode_t errCode;
};

static void LZ4CB_freeWriteCB(LZ4_writeCB_t* state)
{
  if (state == NULL) return;
  LZ4F_freeCompressionContext(state->cctxPtr);
  free(state->dstBuf);
  free(state);
}

static void LZ4CB_freeAndNullWriteCB(LZ4_writeCB_t** statePtr)
{
  assert(statePtr != NULL);
  LZ4CB_freeWriteCB(*statePtr);
  *statePtr = NULL;
}

LZ4F_errorCode_t LZ4CB_writeOpen(LZ4_writeCB_t** lz4cbWrite, LZ4CB_writecb cb, void* ctx, const LZ4F_preferences_t* prefsPtr)
{
  LZ4_byte buf[LZ4F_HEADER_SIZE_MAX];
  size_t ret;

  if (cb == NULL || lz4cbWrite == NULL)
    RETURN_ERROR(parameter_null);

  *lz4cbWrite = (LZ4_writeCB_t*)calloc(1, sizeof(LZ4_writeCB_t));
  if (*lz4cbWrite == NULL) {
    RETURN_ERROR(allocation_failed);
  }
  if (prefsPtr != NULL) {
    switch (prefsPtr->frameInfo.blockSizeID) {
      case LZ4F_default :
      case LZ4F_max64KB :
        (*lz4cbWrite)->maxWriteSize = 64 * 1024;
        break;
      case LZ4F_max256KB:
        (*lz4cbWrite)->maxWriteSize = 256 * 1024;
        break;
      case LZ4F_max1MB:
        (*lz4cbWrite)->maxWriteSize = 1 * 1024 * 1024;
        break;
      case LZ4F_max4MB:
        (*lz4cbWrite)->maxWriteSize = 4 * 1024 * 1024;
        break;
      default:
        LZ4CB_freeAndNullWriteCB(lz4cbWrite);
        RETURN_ERROR(maxBlockSize_invalid);
      }
    } else {
      (*lz4cbWrite)->maxWriteSize = 64 * 1024;
    }

  (*lz4cbWrite)->dstBufMaxSize = LZ4F_compressBound((*lz4cbWrite)->maxWriteSize, prefsPtr);
  (*lz4cbWrite)->dstBuf = (LZ4_byte*)malloc((*lz4cbWrite)->dstBufMaxSize);
  if ((*lz4cbWrite)->dstBuf == NULL) {
    LZ4CB_freeAndNullWriteCB(lz4cbWrite);
    RETURN_ERROR(allocation_failed);
  }

  ret = LZ4F_createCompressionContext(&(*lz4cbWrite)->cctxPtr, LZ4F_VERSION);
  if (LZ4F_isError(ret)) {
      LZ4CB_freeAndNullWriteCB(lz4cbWrite);
      return ret;
  }

  ret = LZ4F_compressBegin((*lz4cbWrite)->cctxPtr, buf, LZ4F_HEADER_SIZE_MAX, prefsPtr);
  if (LZ4F_isError(ret)) {
      LZ4CB_freeAndNullWriteCB(lz4cbWrite);
      return ret;
  }

  if (ret != cb(ctx, ret, buf)) {
    LZ4CB_freeAndNullWriteCB(lz4cbWrite);
    RETURN_ERROR(io_write);
  }

  (*lz4cbWrite)->cb = cb;
  (*lz4cbWrite)->ctx = ctx;
  (*lz4cbWrite)->errCode = LZ4F_OK_NoError;
  return LZ4F_OK_NoError;
}

size_t LZ4CB_write(LZ4_writeCB_t* lz4cbWrite, const void* buf, size_t size)
{
  const LZ4_byte* p = (const LZ4_byte*)buf;
  size_t remain = size;
  size_t chunk;
  size_t ret;

  if (lz4cbWrite == NULL || buf == NULL)
    RETURN_ERROR(parameter_null);
  while (remain) {
    if (remain > lz4cbWrite->maxWriteSize)
      chunk = lz4cbWrite->maxWriteSize;
    else
      chunk = remain;

    ret = LZ4F_compressUpdate(lz4cbWrite->cctxPtr,
                              lz4cbWrite->dstBuf, lz4cbWrite->dstBufMaxSize,
                              p, chunk,
                              NULL);
    if (LZ4F_isError(ret)) {
      lz4cbWrite->errCode = ret;
      return ret;
    }

    if (ret != lz4cbWrite->cb(lz4cbWrite->ctx, ret, lz4cbWrite->dstBuf)) {
      lz4cbWrite->errCode = returnErrorCode(LZ4F_ERROR_io_write);
      RETURN_ERROR(io_write);
    }

    p += chunk;
    remain -= chunk;
  }

  return size;
}

LZ4F_errorCode_t LZ4CB_writeClose(LZ4_writeCB_t* lz4cbWrite)
{
  LZ4F_errorCode_t ret = LZ4F_OK_NoError;

  if (lz4cbWrite == NULL) {
    RETURN_ERROR(parameter_null);
  }

  if (lz4cbWrite->errCode == LZ4F_OK_NoError) {
    ret =  LZ4F_compressEnd(lz4cbWrite->cctxPtr,
                            lz4cbWrite->dstBuf, lz4cbWrite->dstBufMaxSize,
                            NULL);
    if (LZ4F_isError(ret)) {
      goto out;
    }

    if (ret != lz4cbWrite->cb(lz4cbWrite->ctx, ret, lz4cbWrite->dstBuf)) {
      ret = returnErrorCode(LZ4F_ERROR_io_write);
    }
  }

out:
  LZ4CB_freeWriteCB(lz4cbWrite);
  return ret;
}
