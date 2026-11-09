#include "../src/Platform/Networking/GNS/gnsAddress.h"
#include "../src/Platform/Networking/GNS/gnsPosition.h"
#include <cstdio>
#include <cstdlib>
static void check(bool value,const char *message="network address parser"){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
static void testPositionCapacity(){
    std::map<uint64_t,int64_t> positions;
    for(uint32_t guid=0;guid<4096;guid++)
        check(gnsAcceptPosition(positions,2,guid,guid+1),"fill position history");

    // Refresh a low-GUID active thing so eviction must follow sequence age,
    // rather than map key order or the time when the thing first appeared.
    int64_t active=10000;
    check(gnsAcceptPosition(positions,2,0,active),"refresh active position at capacity");
    check(gnsAcceptPosition(positions,2,4096,++active),"admit a new thing at capacity");
    check(!gnsAcceptPosition(positions,2,0,10000),"retain duplicate rejection after eviction");
    check(!gnsAcceptPosition(positions,2,0,9999),"retain stale rejection after eviction");
    check(positions.size()==4096,"keep position history bounded without clearing it");

    // Clients see several senders on the same transport connection. The same
    // GUID from another sender must have its own sequence history.
    check(gnsAcceptPosition(positions,3,0,++active),"admit another sender with the same GUID");
    const int64_t other=active;
    check(!gnsAcceptPosition(positions,3,0,other),"reject duplicate from the other sender");
    check(!gnsAcceptPosition(positions,2,0,10000),"preserve the original sender history");

    for(uint32_t guid=4097;guid<8193;guid++){
        check(gnsAcceptPosition(positions,2,0,++active),"refresh active thing during repeated evictions");
        const int64_t latest=active;
        check(gnsAcceptPosition(positions,3,0,++active),"refresh independent sender during repeated evictions");
        check(gnsAcceptPosition(positions,2,guid,++active),"admit new things during repeated evictions");
        check(positions.size()==4096,"bound repeated position evictions");
        check(!gnsAcceptPosition(positions,2,0,latest),"retain active duplicate rejection across churn");
        check(!gnsAcceptPosition(positions,2,0,latest-1),"retain active stale rejection across churn");
        check(!gnsAcceptPosition(positions,3,0,active-1),"retain the other sender history across churn");
    }
    check(!gnsAcceptPosition(positions,2,0,0),"reject zero sequence for an existing thing");
    check(!gnsAcceptPosition(positions,2,0,-1),"reject negative sequence for an existing thing");
    check(!gnsAcceptPosition(positions,4,9000,0),"reject zero sequence for a new thing");
    check(!gnsAcceptPosition(positions,4,9000,-1),"reject negative sequence for a new thing");
    check(positions.size()==4096,"invalid sequences do not evict history");
    check(!gnsAcceptPosition(positions,2,0,active-2),"invalid sequences preserve active history");
    std::puts("PASS: bounded position history admits new things while preserving recent senders across repeated evictions");
}
int main(){std::string host,port,error;
check(gnsParseAddress("127.0.0.1",host,port,error)&&host=="127.0.0.1"&&port=="27020");
check(gnsParseAddress(" example.com:3456 ",host,port,error)&&host=="example.com"&&port=="3456");
check(gnsParseAddress("[2001:db8::1]:1234",host,port,error)&&host=="2001:db8::1"&&port=="1234");
check(gnsParseAddress("2001:db8::1",host,port,error)&&host=="2001:db8::1"&&port=="27020");
for(const char *bad:{"","  ","host:0","host:65536","host:","host:no","[::1","[::1]garbage","[::1]:-1","http://host","host name"})check(!gnsParseAddress(bad,host,port,error)&&!error.empty());
std::map<uint64_t,int64_t> positions;check(gnsAcceptPosition(positions,2,100,20));check(!gnsAcceptPosition(positions,2,100,19));check(!gnsAcceptPosition(positions,2,100,20));check(gnsAcceptPosition(positions,2,101,18));check(gnsAcceptPosition(positions,3,100,1));check(gnsAcceptPosition(positions,2,100,21));check(!gnsAcceptPosition(positions,2,100,0));
std::puts("PASS: sequenced positions reject stale/duplicate packets without dropping other things or senders");
testPositionCapacity();
std::puts("PASS: IPv4, hostnames, IPv6, whitespace, invalid ports and malformed addresses");}
