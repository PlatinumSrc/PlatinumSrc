#include <stdlib.h>  /* malloc, free */
#include <string.h>
#include <assert.h>
#include "lz4cb.h"

/* =====   Error Handling   ===== */
static inline LZ4F_errorCode_t returnErrorCode(LZ4F_errorCodes code)
{
    return (LZ4F_errorCode_t)-(ptrdiff_t)code;
}

#undef RETURN_ERROR
#define RETURN_ERROR(e) return returnErrorCode(LZ4F_ERROR_ ## e)

/* =====    Read API Implementation    ===== */

static inline void freeReadCBResources(LZ4_readCB_t* lz4cbRead)
{
  #if 0
  if (lz4cbRead==NULL) return;
  #endif
  LZ4F_freeDecompressionContext(lz4cbRead->dctxPtr);
  free(lz4cbRead->srcBuf);
}

static LZ4F_errorCode_t readAndParseHeader(LZ4_readCB_t* readCB)
{
    char headerBuf[LZ4F_HEADER_SIZE_MAX];
    LZ4F_frameInfo_t frameInfo;
    size_t consumedSize;

    /* Read the header from stream */
    const size_t bytesRead = readCB->cb(readCB->ctx, sizeof(headerBuf), headerBuf);
    if (bytesRead < LZ4F_HEADER_SIZE_MIN + LZ4F_ENDMARK_SIZE) {
        RETURN_ERROR(io_read);
    }

    /* Parse frame information */
    consumedSize = bytesRead;
    { const LZ4F_errorCode_t result = LZ4F_getFrameInfo(readCB->dctxPtr, &frameInfo, headerBuf, &consumedSize);
      if (LZ4F_isError(result)) {
          return result;
    } }

    /* Determine buffer size based on block size */
    { const size_t blockSize = LZ4F_getBlockSize(frameInfo.blockSizeID);
      if (blockSize == 0) {
          RETURN_ERROR(maxBlockSize_invalid);
      }
      readCB->srcBufMaxSize = blockSize;
    }

    /* Allocate source buffer */
    assert(readCB->srcBuf == NULL); /* Should be NULL from calloc */
    readCB->srcBuf = (LZ4_byte*)malloc(readCB->srcBufMaxSize);
    if (readCB->srcBuf == NULL) {
        RETURN_ERROR(allocation_failed);
    }

    /* Store remaining header data in buffer */
    readCB->srcBufSize = bytesRead - consumedSize;
    if (readCB->srcBufSize > 0) {
        memcpy(readCB->srcBuf, headerBuf + consumedSize, readCB->srcBufSize);
    }
    readCB->srcBufNext = 0;

    return LZ4F_OK_NoError;
}

LZ4F_errorCode_t LZ4CB_readOpen(LZ4_readCB_t* readCB)
{
    /* Initialize decompression context */
    { LZ4F_errorCode_t const result = LZ4F_createDecompressionContext(&readCB->dctxPtr, LZ4F_VERSION);
      if (LZ4F_isError(result)) {
          freeReadCBResources(readCB);
          return result;
    } }

    /* Read and parse the header */
    { LZ4F_errorCode_t const result = readAndParseHeader(readCB);
      if (LZ4F_isError(result)) {
          freeReadCBResources(readCB);
          return result;
    } }

    return LZ4F_OK_NoError;
}

size_t LZ4CB_read(LZ4_readCB_t* lz4cbRead, void* buf, size_t size)
{
  LZ4_byte* outPtr = (LZ4_byte*)buf;
  size_t totalBytesRead = 0;

  if (lz4cbRead == NULL || buf == NULL)
    RETURN_ERROR(parameter_null);

  while (totalBytesRead < size) {
    size_t srcBytes = lz4cbRead->srcBufSize - lz4cbRead->srcBufNext;
    size_t dstBytes = size - totalBytesRead;

    if (srcBytes == 0) {
      size_t const bytesRead = lz4cbRead->cb(lz4cbRead->ctx, sizeof(lz4cbRead->srcBufMaxSize), lz4cbRead->srcBuf);
      if (bytesRead == 0) {
        break; /* end of input reached */
      }
      /* success: ret > 0 */
      lz4cbRead->srcBufSize = bytesRead;
      srcBytes = lz4cbRead->srcBufSize;
      lz4cbRead->srcBufNext = 0;
    }

    { size_t const decStatus = LZ4F_decompress(
                          lz4cbRead->dctxPtr,
                          outPtr, &dstBytes,
                          lz4cbRead->srcBuf + lz4cbRead->srcBufNext,
                          &srcBytes,
                          NULL);
      if (LZ4F_isError(decStatus)) {
          return decStatus;
    } }

    lz4cbRead->srcBufNext += srcBytes;
    totalBytesRead += dstBytes;
    outPtr += dstBytes;
  }

  return totalBytesRead;
}

LZ4F_errorCode_t LZ4CB_readClose(LZ4_readCB_t* lz4cbRead)
{
  if (lz4cbRead == NULL)
    RETURN_ERROR(parameter_null);
  freeReadCBResources(lz4cbRead);
  return LZ4F_OK_NoError;
}

/* =====   write API   ===== */

static inline void freeWriteCBResources(LZ4_writeCB_t* state)
{
  #if 0
  if (state == NULL) return;
  #endif
  LZ4F_freeCompressionContext(state->cctxPtr);
  free(state->dstBuf);
  free(state);
}

static LZ4F_errorCode_t writeHeader(LZ4_writeCB_t* writeCB,
            const LZ4F_preferences_t* prefsPtr)
{
  LZ4_byte headerBuf[LZ4F_HEADER_SIZE_MAX];

  /* Generate header */
  LZ4F_errorCode_t const headerSize = LZ4F_compressBegin(writeCB->cctxPtr,
                                  headerBuf, LZ4F_HEADER_SIZE_MAX, prefsPtr);
  if (LZ4F_isError(headerSize)) {
    return headerSize;
  }

  /* Write header to stream */
  if (headerSize != writeCB->cb(writeCB->ctx, headerSize, headerBuf)) {
    RETURN_ERROR(io_write);
  }

  return LZ4F_OK_NoError;
}

LZ4F_errorCode_t LZ4CB_writeOpen(LZ4_writeCB_t* writeCB, const LZ4F_preferences_t* prefsPtr)
{
  size_t blockSize;

  /* Validate block size */
  { LZ4F_blockSizeID_t const blockSizeID = prefsPtr ? prefsPtr->frameInfo.blockSizeID : LZ4F_default;
    blockSize = LZ4F_getBlockSize(blockSizeID);
    if (blockSize == 0) {
        RETURN_ERROR(maxBlockSize_invalid);
  } }

  writeCB->errCode = LZ4F_OK_NoError;
  writeCB->maxWriteSize = blockSize;

  /* Calculate and allocate destination buffer */
  writeCB->dstBufMaxSize = LZ4F_compressBound(0, prefsPtr);
  writeCB->dstBuf = (LZ4_byte*)malloc(writeCB->dstBufMaxSize);
  if (writeCB->dstBuf == NULL) {
    freeWriteCBResources(writeCB);
    RETURN_ERROR(allocation_failed);
  }

  /* Initialize compression context */
  { LZ4F_errorCode_t const status = LZ4F_createCompressionContext(&writeCB->cctxPtr, LZ4F_VERSION);
    if (LZ4F_isError(status)) {
        freeWriteCBResources(writeCB);
        return status;
  } }

    /* Write header to stream */
  { LZ4F_errorCode_t const writeStatus = writeHeader(writeCB, prefsPtr);
    if (LZ4F_isError(writeStatus)) {
        freeWriteCBResources(writeCB);
        return writeStatus;
  } }

  return LZ4F_OK_NoError;
}

size_t LZ4CB_write(LZ4_writeCB_t* lz4cbWrite, const void* buf, size_t size)
{
  const LZ4_byte* p = (const LZ4_byte*)buf;
  size_t remainingBytes = size;

  /* Validate parameters */
  if (lz4cbWrite == NULL || buf == NULL)
    RETURN_ERROR(parameter_null);

  while (remainingBytes) {
    size_t const chunkSize = (remainingBytes > lz4cbWrite->maxWriteSize) ? lz4cbWrite->maxWriteSize : remainingBytes;

    /* Compress and write chunk */
    size_t cSize = LZ4F_compressUpdate(lz4cbWrite->cctxPtr,
                              lz4cbWrite->dstBuf, lz4cbWrite->dstBufMaxSize,
                              p, chunkSize,
                              NULL);
    if (LZ4F_isError(cSize)) {
      lz4cbWrite->errCode = cSize;
      return cSize;
    }

    if (cSize != lz4cbWrite->cb(lz4cbWrite->ctx, cSize, lz4cbWrite->dstBuf)) {
      lz4cbWrite->errCode = returnErrorCode(LZ4F_ERROR_io_write);
      RETURN_ERROR(io_write);
    }

    /* Update positions */
    p += chunkSize;
    remainingBytes -= chunkSize;
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
      goto cleanup;
    }

    if (ret != lz4cbWrite->cb(lz4cbWrite->ctx, ret, lz4cbWrite->dstBuf)) {
      ret = returnErrorCode(LZ4F_ERROR_io_write);
    }
  }

cleanup:
  freeWriteCBResources(lz4cbWrite);
  return ret;
}
