#ifndef lock_hpp
#define lock_hpp

#include <stdio.h>
#include <string>

using namespace std;

class Lock{
public:
    string node_name;
    string param_name;
    int state; //0 == locked, 1 == waiting

    Lock(string p, string res, int stat) {
        node_name = p;
        param_name = res;
        state = stat;
    }
};

#endif /* lock_hpp */
