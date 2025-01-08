#include <stdlib.h>  /* malloc, free */
#include <string.h>
#include <assert.h>
#include "lz4.h"
#include "lz4ds.h"

static LZ4F_errorCode_t returnErrorCode(LZ4F_errorCodes code)
{
    return (LZ4F_errorCode_t)-(ptrdiff_t)code;
}
#undef RETURN_ERROR
#define RETURN_ERROR(e) return returnErrorCode(LZ4F_ERROR_ ## e)

/* =====   read API   ===== */

struct LZ4_readDS_s {
  LZ4F_dctx* dctxPtr;
  PSRC_DATASTREAM_T ds;
  LZ4_byte* srcBuf;
  size_t srcBufNext;
  size_t srcBufSize;
  size_t srcBufMaxSize;
};

static void LZ4DS_freeReadDS(LZ4_readDS_t* lz4dsRead)
{
  if (lz4dsRead==NULL) return;
  LZ4F_freeDecompressionContext(lz4dsRead->dctxPtr);
  free(lz4dsRead->srcBuf);
  free(lz4dsRead);
}

static void LZ4DS_freeAndNullReadDS(LZ4_readDS_t** statePtr)
{
  assert(statePtr != NULL);
  LZ4DS_freeReadDS(*statePtr);
  *statePtr = NULL;
}

LZ4F_errorCode_t LZ4DS_readOpen(LZ4_readDS_t** lz4dsRead, PSRC_DATASTREAM_T ds)
{
  char buf[LZ4F_HEADER_SIZE_MAX];
  size_t consumedSize;
  LZ4F_errorCode_t ret;

  if (ds == NULL || lz4dsRead == NULL) {
    RETURN_ERROR(parameter_null);
  }

  *lz4dsRead = (LZ4_readDS_t*)calloc(1, sizeof(LZ4_readDS_t));
  if (*lz4dsRead == NULL) {
    RETURN_ERROR(allocation_failed);
  }

  ret = LZ4F_createDecompressionContext(&(*lz4dsRead)->dctxPtr, LZ4F_VERSION);
  if (LZ4F_isError(ret)) {
    LZ4DS_freeAndNullReadDS(lz4dsRead);
    return ret;
  }

  (*lz4dsRead)->ds = ds;
  consumedSize = ds_read((*lz4dsRead)->ds, sizeof(buf), buf);
  if (consumedSize < LZ4F_HEADER_SIZE_MIN + LZ4F_ENDMARK_SIZE) {
    LZ4DS_freeAndNullReadDS(lz4dsRead);
    RETURN_ERROR(io_read);
  }

  { LZ4F_frameInfo_t info;
    LZ4F_errorCode_t const r = LZ4F_getFrameInfo((*lz4dsRead)->dctxPtr, &info, buf, &consumedSize);
    if (LZ4F_isError(r)) {
      LZ4DS_freeAndNullReadDS(lz4dsRead);
      return r;
    }

    switch (info.blockSizeID) {
      case LZ4F_default :
      case LZ4F_max64KB :
        (*lz4dsRead)->srcBufMaxSize = 64 * 1024;
        break;
      case LZ4F_max256KB:
        (*lz4dsRead)->srcBufMaxSize = 256 * 1024;
        break;
      case LZ4F_max1MB:
        (*lz4dsRead)->srcBufMaxSize = 1 * 1024 * 1024;
        break;
      case LZ4F_max4MB:
        (*lz4dsRead)->srcBufMaxSize = 4 * 1024 * 1024;
        break;
      default:
        LZ4DS_freeAndNullReadDS(lz4dsRead);
        RETURN_ERROR(maxBlockSize_invalid);
    }
  }

  (*lz4dsRead)->srcBuf = (LZ4_byte*)malloc((*lz4dsRead)->srcBufMaxSize);
  if ((*lz4dsRead)->srcBuf == NULL) {
    LZ4DS_freeAndNullReadDS(lz4dsRead);
    RETURN_ERROR(allocation_failed);
  }

  (*lz4dsRead)->srcBufSize = sizeof(buf) - consumedSize;
  memcpy((*lz4dsRead)->srcBuf, buf + consumedSize, (*lz4dsRead)->srcBufSize);

  return ret;
}

size_t LZ4DS_read(LZ4_readDS_t* lz4dsRead, void* buf, size_t size)
{
  LZ4_byte* p = (LZ4_byte*)buf;
  size_t next = 0;

  if (lz4dsRead == NULL || buf == NULL)
    RETURN_ERROR(parameter_null);

  while (next < size) {
    size_t srcsize = lz4dsRead->srcBufSize - lz4dsRead->srcBufNext;
    size_t dstsize = size - next;
    size_t ret;

    if (srcsize == 0) {
      ret = ds_read(lz4dsRead->ds, lz4dsRead->srcBufMaxSize, lz4dsRead->srcBuf);
      if (ret > 0) {
        lz4dsRead->srcBufSize = ret;
        srcsize = lz4dsRead->srcBufSize;
        lz4dsRead->srcBufNext = 0;
      } else if (ret == 0) {
        break;
      } else {
        RETURN_ERROR(io_read);
      }
    }

    ret = LZ4F_decompress(lz4dsRead->dctxPtr,
                          p, &dstsize,
                          lz4dsRead->srcBuf + lz4dsRead->srcBufNext,
                          &srcsize,
                          NULL);
    if (LZ4F_isError(ret)) {
        return ret;
    }

    lz4dsRead->srcBufNext += srcsize;
    next += dstsize;
    p += dstsize;
  }

  return next;
}

LZ4F_errorCode_t LZ4DS_readClose(LZ4_readDS_t* lz4dsRead)
{
  if (lz4dsRead == NULL)
    RETURN_ERROR(parameter_null);
  LZ4DS_freeReadDS(lz4dsRead);
  return LZ4F_OK_NoError;
}

#if 0

/* =====   write API   ===== */

struct LZ4_writeFile_s {
  LZ4F_cctx* cctxPtr;
  FILE* fp;
  LZ4_byte* dstBuf;
  size_t maxWriteSize;
  size_t dstBufMaxSize;
  LZ4F_errorCode_t errCode;
};

static void LZ4F_freeWriteFile(LZ4_writeFile_t* state)
{
  if (state == NULL) return;
  LZ4F_freeCompressionContext(state->cctxPtr);
  free(state->dstBuf);
  free(state);
}

static void LZ4F_freeAndNullWriteFile(LZ4_writeFile_t** statePtr)
{
  assert(statePtr != NULL);
  LZ4F_freeWriteFile(*statePtr);
  *statePtr = NULL;
}

LZ4F_errorCode_t LZ4F_writeOpen(LZ4_writeFile_t** lz4fWrite, FILE* fp, const LZ4F_preferences_t* prefsPtr)
{
  LZ4_byte buf[LZ4F_HEADER_SIZE_MAX];
  size_t ret;

  if (fp == NULL || lz4fWrite == NULL)
    RETURN_ERROR(parameter_null);

  *lz4fWrite = (LZ4_writeFile_t*)calloc(1, sizeof(LZ4_writeFile_t));
  if (*lz4fWrite == NULL) {
    RETURN_ERROR(allocation_failed);
  }
  if (prefsPtr != NULL) {
    switch (prefsPtr->frameInfo.blockSizeID) {
      case LZ4F_default :
      case LZ4F_max64KB :
        (*lz4fWrite)->maxWriteSize = 64 * 1024;
        break;
      case LZ4F_max256KB:
        (*lz4fWrite)->maxWriteSize = 256 * 1024;
        break;
      case LZ4F_max1MB:
        (*lz4fWrite)->maxWriteSize = 1 * 1024 * 1024;
        break;
      case LZ4F_max4MB:
        (*lz4fWrite)->maxWriteSize = 4 * 1024 * 1024;
        break;
      default:
        LZ4F_freeAndNullWriteFile(lz4fWrite);
        RETURN_ERROR(maxBlockSize_invalid);
      }
    } else {
      (*lz4fWrite)->maxWriteSize = 64 * 1024;
    }

  (*lz4fWrite)->dstBufMaxSize = LZ4F_compressBound((*lz4fWrite)->maxWriteSize, prefsPtr);
  (*lz4fWrite)->dstBuf = (LZ4_byte*)malloc((*lz4fWrite)->dstBufMaxSize);
  if ((*lz4fWrite)->dstBuf == NULL) {
    LZ4F_freeAndNullWriteFile(lz4fWrite);
    RETURN_ERROR(allocation_failed);
  }

  ret = LZ4F_createCompressionContext(&(*lz4fWrite)->cctxPtr, LZ4F_VERSION);
  if (LZ4F_isError(ret)) {
      LZ4F_freeAndNullWriteFile(lz4fWrite);
      return ret;
  }

  ret = LZ4F_compressBegin((*lz4fWrite)->cctxPtr, buf, LZ4F_HEADER_SIZE_MAX, prefsPtr);
  if (LZ4F_isError(ret)) {
      LZ4F_freeAndNullWriteFile(lz4fWrite);
      return ret;
  }

  if (ret != fwrite(buf, 1, ret, fp)) {
    LZ4F_freeAndNullWriteFile(lz4fWrite);
    RETURN_ERROR(io_write);
  }

  (*lz4fWrite)->fp = fp;
  (*lz4fWrite)->errCode = LZ4F_OK_NoError;
  return LZ4F_OK_NoError;
}

size_t LZ4F_write(LZ4_writeFile_t* lz4fWrite, const void* buf, size_t size)
{
  const LZ4_byte* p = (const LZ4_byte*)buf;
  size_t remain = size;
  size_t chunk;
  size_t ret;

  if (lz4fWrite == NULL || buf == NULL)
    RETURN_ERROR(parameter_null);
  while (remain) {
    if (remain > lz4fWrite->maxWriteSize)
      chunk = lz4fWrite->maxWriteSize;
    else
      chunk = remain;

    ret = LZ4F_compressUpdate(lz4fWrite->cctxPtr,
                              lz4fWrite->dstBuf, lz4fWrite->dstBufMaxSize,
                              p, chunk,
                              NULL);
    if (LZ4F_isError(ret)) {
      lz4fWrite->errCode = ret;
      return ret;
    }

    if (ret != fwrite(lz4fWrite->dstBuf, 1, ret, lz4fWrite->fp)) {
      lz4fWrite->errCode = returnErrorCode(LZ4F_ERROR_io_write);
      RETURN_ERROR(io_write);
    }

    p += chunk;
    remain -= chunk;
  }

  return size;
}

LZ4F_errorCode_t LZ4F_writeClose(LZ4_writeFile_t* lz4fWrite)
{
  LZ4F_errorCode_t ret = LZ4F_OK_NoError;

  if (lz4fWrite == NULL) {
    RETURN_ERROR(parameter_null);
  }

  if (lz4fWrite->errCode == LZ4F_OK_NoError) {
    ret =  LZ4F_compressEnd(lz4fWrite->cctxPtr,
                            lz4fWrite->dstBuf, lz4fWrite->dstBufMaxSize,
                            NULL);
    if (LZ4F_isError(ret)) {
      goto out;
    }

    if (ret != fwrite(lz4fWrite->dstBuf, 1, ret, lz4fWrite->fp)) {
      ret = returnErrorCode(LZ4F_ERROR_io_write);
    }
  }

out:
  LZ4F_freeWriteFile(lz4fWrite);
  return ret;
}

#endif
