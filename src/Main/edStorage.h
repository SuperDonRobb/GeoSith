#ifndef ED_STORAGE_H
#define ED_STORAGE_H
#include <deque>
#include <array>
#include <vector>
#include <initializer_list>
#include <type_traits>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <climits>
#include <cstdint>
/* Value-owned editor storage. Appending never invalidates references to an
 * existing sector/face. Copies own their data independently (undo/candidates). */
template<class T> class EdBuffer {
    std::deque<T> values;
public:
    EdBuffer() = default;
    EdBuffer(const EdBuffer&) = default;
    EdBuffer& operator=(const EdBuffer&) = default;
    EdBuffer& operator=(EdBuffer&& other) noexcept {values.swap(other.values);return *this;}
    EdBuffer(std::initializer_list<T> init):values(init) {}
    T& operator[](size_t i) { if(i>=values.size()) values.resize(i+1); return values[i]; }
    const T& operator[](size_t i) const { static const T empty{}; return i<values.size()?values[i]:empty; }
    size_t size() const { return values.size(); }
    void clear() { values.clear(); }
    void resize(size_t n) { values.resize(n); }
};
/* Typed bridge for the editor's C-style temporary allocations. Nontrivial
 * documents/cells require constructors and destructors; byte buffers do not. */
struct EdAllocation {
    size_t bytes; bool zero;
    template<class T> operator T*() const {
        if constexpr (!std::is_trivial_v<T>) return new T[bytes/sizeof(T)]{};
        else return static_cast<T*>(zero?std::calloc(1,bytes):std::malloc(bytes));
    }
};
inline EdAllocation edAlloc(size_t n) { return {n,false}; }
inline EdAllocation edCalloc(size_t n,size_t size) { if(size&&n>SIZE_MAX/size) throw std::bad_alloc(); return {n*size,true}; }
template<class T> void edFree(T* p) { if constexpr (!std::is_trivial_v<T>) delete[] p; else std::free(p); }
inline void edFree(void* p) { std::free(p); }
template<class T> void edZero(T* p,int value,size_t bytes) {
    if constexpr (!std::is_trivial_v<T>) { if(value||bytes%sizeof(T)) throw std::logic_error("invalid editor reset"); for(size_t i=0;i<bytes/sizeof(T);++i)p[i]=T{}; }
    else std::memset(p,value,bytes);
}
template<class T> void edZero(EdBuffer<T>& p,int value,size_t) { if(value)throw std::logic_error("invalid editor buffer reset");p.clear(); }
template<class T> void edCopy(T* to,const T* from,size_t bytes) {
    if constexpr (!std::is_trivially_copyable_v<T>) { for(size_t i=0;i<bytes/sizeof(T);++i)to[i]=from[i]; }
    else std::memcpy(to,from,bytes);
}
inline void edCopy(void* to,const void* from,size_t bytes) { std::memcpy(to,from,bytes); }
template<class T> void edCopy(EdBuffer<T>& to,const EdBuffer<T>& from,size_t) {to=from;}
template<class T> void edCopy(EdBuffer<T>& to,const T* from,size_t bytes) {to.clear();for(size_t i=0;i<bytes/sizeof(T);++i)to[i]=from[i];}
template<class T> void edCopy(T* to,const EdBuffer<T>& from,size_t bytes) {for(size_t i=0;i<bytes/sizeof(T);++i)to[i]=from[i];}
template<class T> class EdScratch {
    std::vector<T> values;
public:
    T& operator[](size_t i) {if(i>=values.size())values.resize(i+1);return values[i];}
    T& operator[](int i) {if(i<0)throw std::out_of_range("Negative scratch index");return (*this)[(size_t)i];}
    operator T*() {return values.data();}
};
inline int edCompare(const void* a,const void* b,size_t n) {return std::memcmp(a,b,n);}
template<class T> int edCompare(const EdBuffer<T>& a,const EdBuffer<T>& b,size_t bytes) {size_t n=bytes==sizeof(EdBuffer<T>)?(std::max)(a.size(),b.size()):bytes/sizeof(T);for(size_t i=0;i<n;++i)if(edCompare(&a[i],&b[i],sizeof(T)))return 1;return 0;}
#endif


