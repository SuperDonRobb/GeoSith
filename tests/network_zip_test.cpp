#include "../src/Main/edNetZip.h"
#include <vector>
#include <cstdio>
static void check(bool ok){if(!ok){puts("Compression test failed");exit(1);}}
int main(){
    std::vector<unsigned char> input(2*1024*1024);for(size_t i=0;i<input.size();i++)input[i]=(unsigned char)((i%4096)*31+(i%256));
    uint32_t size=0;unsigned char *zip=edNetZip(input.data(),(uint32_t)input.size(),&size);check(zip&&size<input.size()/20);
    unsigned char *decoded=edNetUnzip(zip,size,(uint32_t)input.size());check(decoded&&!memcmp(decoded,input.data(),input.size()));edFree(decoded);
    check(!edNetUnzip(zip,size-1,(uint32_t)input.size()));check(!edNetUnzip(zip,size,(uint32_t)input.size()-1));check(!edNetUnzip(zip,size,(uint32_t)input.size()+1));
    std::vector<unsigned char> trailing(zip,zip+size);trailing.push_back(0);check(!edNetUnzip(trailing.data(),size+1,(uint32_t)input.size()));
    zip[size/2]^=0x55;check(!edNetUnzip(zip,size,(uint32_t)input.size()));edFree(zip);
    std::vector<unsigned char> next=input;for(int i=100;i<200;i++)next[i]^=7;
    zip=edNetZipDelta(input.data(),(uint32_t)input.size(),next.data(),(uint32_t)next.size(),&size);check(zip);decoded=edNetUnzip(zip,size,(uint32_t)next.size());check(decoded);
    for(size_t i=0;i<next.size();i++)decoded[i]^=input[i];check(!memcmp(decoded,next.data(),next.size()));edFree(decoded);edFree(zip);
    check(!edNetUnzip("x",1,ED_NET_LIMIT));puts("PASS: texture compression, exact bounds, corruption/truncation/trailing rejection, deltas");
}
