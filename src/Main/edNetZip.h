#ifndef ED_NET_ZIP_H
#define ED_NET_ZIP_H
#include "edNet.h"
#include "../../lib/zlib/zlib.h"
// A bounded zlib stream, with exact output length and no trailing payload.
static unsigned char *edNetZip(const void *data,uint32_t length,uint32_t *size){
    if(!length||length>ED_NET_LIMIT/2)return NULL;
    uLongf capacity=compressBound(length);unsigned char *out=edAlloc(capacity);if(!out)return NULL;
    if(compress2(out,&capacity,(const Bytef*)data,length,6)!=Z_OK){edFree(out);return NULL;}
    *size=(uint32_t)capacity;return out;
}
static unsigned char *edNetUnzip(const void *data,uint32_t size,uint32_t length){
    if(!size||size>ED_NET_LIMIT||!length||length>ED_NET_LIMIT/2)return NULL;
    unsigned char *out=edAlloc(length);if(!out)return NULL;
    z_stream stream={};stream.next_in=(Bytef*)data;stream.avail_in=size;stream.next_out=out;stream.avail_out=length;
    if(inflateInit(&stream)!=Z_OK){edFree(out);return NULL;}
    int result=inflate(&stream,Z_FINISH);int ok=result==Z_STREAM_END&&stream.total_out==length&&stream.total_in==size;
    inflateEnd(&stream);if(!ok){edFree(out);return NULL;}return out;
}
static unsigned char *edNetZipDelta(const unsigned char *base,uint32_t baseSize,const unsigned char *next,uint32_t size,uint32_t *packedSize){
    if(!size||size>ED_NET_LIMIT/2)return NULL;unsigned char *delta=edAlloc(size);if(!delta)return NULL;
    for(uint32_t i=0;i<size;i++)delta[i]=next[i]^(i<baseSize?base[i]:0);
    unsigned char *packed=edNetZip(delta,size,packedSize);edFree(delta);return packed;
}
#endif
