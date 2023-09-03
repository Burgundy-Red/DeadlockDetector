#include <iostream>
#include <string>

#include "LockManager.h"

int doGetLock(string node_name, string param_name) {
    static auto& lock_manager = LockManager::getInstance();
    int ret;
    printf("set lock(node_name=%s, res_id=%s)\n", node_name.c_str(), param_name.c_str());
    lock_manager.getLock(node_name, param_name, ret);
    if(ret == 1) {
        printf("lock(node_name=%s, res_id=%s) is duplicated, lock failed\n", node_name.c_str(), param_name.c_str());
    }
    return ret;
}

int doReleaseLock(string node_name, string param_name) {
    static auto& lock_manager = LockManager::getInstance();
    auto lock = lock_manager.findLock(node_name, param_name);
//    printf("release lock(node_name=%d, res_id=%d)\n", node_name, param_name);
    if(lock != nullptr) lock_manager.releaseLock(lock);
    else printf("no such lock(node_name=%s, res_id=%s)\n", node_name.c_str(), param_name.c_str());
    return 0;
}

int main(int argc, const char * argv[]) {
    char tmp[40];
    char node_name[20], param_name[20];
    while(scanf("%s %s %s", tmp, node_name, param_name) != EOF) {
        if(string(tmp) == "lock") {
            doGetLock(node_name, param_name);
        } else {
            doReleaseLock(node_name, param_name);
        }
    }

    std::this_thread::sleep_for (std::chrono::seconds(1));

    LockManager::getInstance().stopDeadlockDetector();
    
    return 0;
}
