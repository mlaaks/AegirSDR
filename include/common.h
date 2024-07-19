/*
AegirSDR

MIT License

Copyright (c) 2024 Mikko Laakso

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <condition_variable>
#include <mutex>
#include "cdsp.h"

#include <vector>
#include <iterator>
#include <cstring> //std::memset
#include <queue>

//#define USELIBRTLSDRBUFS //Move this to CMake, D_USE_LIBRTLBUFS
/*
    TODO: Edit the code to common.cc files from header files
*/

const float sync_threshold=0.01; //EDITED for Kraken. Fix this to read the value from .conf file

#define TWO_POW(n)		((double)(1ULL<<(n)))

//signum template:
template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

//a circular pointer array:
class cbuffer{
public:
    int8_t      **ptr;
    uint32_t    *readcnt;
    uint64_t    *timestamp;
    uint32_t    L;
    uint32_t	N;

    uint32_t    wp;
    uint32_t    rp;

    cbuffer(uint32_t n, uint32_t l){
    	ptr 	  = new int8_t* [n];
    	readcnt   = new uint32_t [n];
    	timestamp = new uint64_t [n];

        wp = 0;
        rp = 0;
    	N=n;
    	L=l;

    	uint32_t alignment = volk_get_alignment();
    	for (uint32_t i=0;i<N;++i){
            readcnt[i]=0;
            timestamp[i]=0;

#ifndef USELIBRTLSDRBUFS
			ptr[i] = (int8_t *) volk_malloc(sizeof(int8_t)*L,alignment);
			std::memset(ptr[i],0,sizeof(int8_t)*L);
#else
			ptr[i] = NULL;
#endif		
		}
    };

    ~cbuffer(){
#ifndef USELIBRTLSDRBUFS
    	for (int i=0;i<N;++i)
			volk_free(ptr[i]);
#endif
    	delete[] ptr;
        delete[] readcnt;
        delete[] timestamp;
    };


void setbufferptr(uint8_t *buffer,uint32_t rcnt)
{
    timestamp[((wp) & (N-1))] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    readcnt[((wp) & (N-1))] = rcnt;
#ifdef USELIBRTLSDRBUFS
    ptr[((wp) & (N-1))] = (int8_t *) buffer;
#endif
    cdsp::convtosigned(buffer, (uint8_t *) ptr[wp++ & (N-1)],L);
};

int8_t *getbufferptr(){
#ifndef USELIBRTLSDRBUFS        
        return ptr[rp & (N-1)];
#else
        //we might return a null pointer during the first run through the circular pointer array. Hack to avoid that...
        return ptr[rp & (N-1)] != NULL ? ptr[rp & (N-1)] : ptr[0]; 
#endif
    }

uint32_t get_rcnt(){
    return readcnt[rp & (N-1)];
}

void consume(){
    rp++;
}


int8_t *getbufferptr(uint32_t rcnt){
#ifndef USELIBRTLSDRBUFS        
        return ptr[rcnt % N];
#else
        return ptr[rcnt % N] != NULL ? ptr[rcnt % N] : ptr[0];
#endif
}
};

class barrier
{
private:
    std::mutex mtx;
    std::condition_variable cv;
    std::size_t cnt;
public:
    explicit barrier(std::size_t cnt) : cnt{cnt} { }
    void wait()
    {
        std::unique_lock<std::mutex> lock{mtx};
        if (--cnt == 0) {
            cv.notify_all();
        } else {
            cv.wait(lock, [this] { return cnt == 0; });
        }
    }
};

using namespace std;

//thread-safe(?) simple vector template class
template <class T>
class lvector{
private:
    std::vector<T> q;

public:
	mutable std::mutex m;
    explicit lvector(): q(){
    }

    lvector(T obj): q(obj){
    }

    void push_back(const T& v){
    	std::lock_guard<std::mutex> lock(m);
        q.push_back(v);
    };
    void pop_back(){
    	std::lock_guard<std::mutex> lock(m);
        q.pop_back();
    };

    using iterator = typename std::vector<T>::iterator;

    void lock(){
    	m.lock();
    };

    void unlock(){
        m.unlock();
    };

    void erase(iterator it){
      	std::lock_guard<std::mutex> lock(m);
        q.erase(it);
    }

    const iterator begin(){
        return q.begin();
    }

    const iterator end(){
        return q.end();
    }
    T& back(){
        return q.back();
    }
    
    size_t size() const{
        return q.size();
    };

          T& operator[](std::size_t idx)       { /*std::unique_lock<std::mutex> lock(m); */ return q[idx]; }
    const T& operator[](std::size_t idx) const { /*std::unique_lock<std::mutex> lock(m); */ return q[idx]; }
    bool operator!=(const T& a){ return !(a == q);}
};


//thread-safe(?) simple queue template class
template <class T>
class lqueue{
private:
    std::queue<T> q;
    std::condition_variable cv;
    std::mutex m;
public:
    lqueue(): q(){
    }
    lqueue(T obj): q(obj){
    }

    void push(const T & v){
        std::lock_guard<std::mutex> lock(m);
        q.push(v);
        cv.notify_one();
    };

    T& front(){
       // std::unique_lock<std::mutex> lock(m);
       // cv.wait(lock, [this]{return (q.size() > 0);});
        return q.front();
    };
    T pop(){
        {
            std::unique_lock<std::mutex> lock(m);
            cv.wait(lock, [this]{return (q.size()>0);});
        }
        T v =q.front(); // was T& v, reference got bad after certain number of chars when T=std::string

        q.pop();
        return v;
    };

    size_t size() const{
        return q.size();
    };
};

bool is_zeros(uint32_t *p,int n);