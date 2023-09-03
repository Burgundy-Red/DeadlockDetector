#ifndef LOCK_MANAGER_H_
#define LOCK_MANAGER_H_

#include <condition_variable>
#include <stdio.h>
#include <iostream>
#include <memory>
#include <map>
#include <list>
#include <vector>
#include <set>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>

#include "Lock.h"

using namespace std;

typedef pair<int, list<shared_ptr<Lock> > > pivec;

class LockManager{
public:
    static LockManager& getInstance();
    ~LockManager();
    
    // ret: 0=ok, 1=duplicate lock
    shared_ptr<Lock> getLock(string node_name, string param_name, int & ret);
    
    // find existing lock
    shared_ptr<Lock> findLock(string node_name, string param_name);
    
    int releaseLock(shared_ptr<Lock> lock);
    
    bool isDeadLock(string& node_name);
    
    void print();
    
    void startDetection(int interval);

    void stopDeadlockDetector();

private:
    LockManager();
    void calSCC(map<string, vector<string>>& pid_to_SCCid); // only put cycle into map
    void releaseProcess(string pid); // release all the locks related to process
    shared_ptr<Lock> getLockInternal(string node_name, string param_name, bool& possible);
    int releaseLockInternal(shared_ptr<Lock> lock);
    
    //deadlock detection thread func
    void detectDeadlock();
    
    map<string, list<shared_ptr<Lock>>> param_to_locklist; // param->key -> locklist
    map<string, pivec> node_to_params;                   // nodename -> {param-key, locklist}
    map<string, list<string>> lock_graph; // graph
    set<string> node_set;
    // thread param
    thread* deadlock_checker;
    bool stop;
    int check_interval;
    mutex mtx;
};

#endif /* lock_manager_hpp */
