#include "LockManager.h"

LockManager::LockManager() {
    stop = false;
    check_interval = 2;
    deadlock_checker = new thread([this]{ this->detectDeadlock(); });
}

LockManager::~LockManager() {
    stopDeadlockDetector();
}

LockManager &LockManager::getInstance() {
    static LockManager inst;
    return inst;
}

shared_ptr<Lock> LockManager::getLock(string node_name, string param_name, int & ret) {
    bool deadlock_possible;
    ret = 0; // ok
    // 已经拥有锁
    if(findLock(node_name, param_name) != nullptr) {
        ret = 1; // duplicate lock
        return nullptr;
    }

    mtx.lock();
    auto p = getLockInternal(node_name, param_name, deadlock_possible);
    mtx.unlock();
    return p;
}

shared_ptr<Lock> LockManager::getLockInternal(string node_name, string param_name, bool & deadlock_possible) {
    deadlock_possible = false; // no deadlock
    if(!param_to_locklist.count(param_name)) param_to_locklist[param_name] = list<shared_ptr<Lock>>{};
    if(!node_to_params.count(node_name)) node_to_params[node_name] = make_pair(0, list<shared_ptr<Lock>>{});
    node_set.insert(node_name); // insert seen node_name
    
    shared_ptr<Lock> newlock;
    if(param_to_locklist[param_name].size() == 0) {
        // 没有node获取这个param
        newlock = make_shared<Lock>(node_name, param_name, 0);
        param_to_locklist[param_name].push_back(newlock);
        node_to_params[node_name].first++;
        node_to_params[node_name].second.push_back(newlock);
    } else {
        // 0 locked 1 waiting
        newlock = make_shared<Lock>(node_name, param_name, 1);
        param_to_locklist[param_name].push_back(newlock);
        node_to_params[node_name].second.push_back(newlock);
        
        // 占有并等待
        if(node_to_params[node_name].first > 0) {
            // 申请者->占有者的一条边
            const auto& first_lock = param_to_locklist[param_name].front();
            string p0id = first_lock->node_name;
            if(!lock_graph.count(node_name)) lock_graph[node_name] = list<string>{};
            lock_graph[node_name].push_back(p0id);
            deadlock_possible = true;
        }
    }
    return newlock;
}

shared_ptr<Lock> LockManager::findLock(string node_name, string param_name) {
    shared_ptr<Lock> result = nullptr;
    mtx.lock();
    for(const auto& p: node_to_params[node_name].second) {
        // Lock->name == param_name 正在申请或已拥有
        if(p->param_name == param_name) result = p;
    }
    mtx.unlock();
    return result;
}

int LockManager::releaseLock(shared_ptr<Lock> lock) {
    mtx.lock();
    releaseLockInternal(lock);
    mtx.unlock();
    return 0;
}

int LockManager::releaseLockInternal(shared_ptr<Lock> lock) {
    string node_name = lock->node_name;
    string param_name = lock->param_name;
    
    auto& locklist = param_to_locklist[param_name];
    locklist.remove(lock);
    node_to_params[node_name].second.remove(lock);
    
    printf("release lock(node_name=%s, param_name=%s, state=%d)\n", node_name.c_str(), param_name.c_str(), lock->state);

    // 拥有者释放 且还有获取的node
    if(lock->state == 0 && locklist.size() > 0) {
        node_to_params[node_name].first--;
        string p0id = locklist.front()->node_name; // new process that get lock
        locklist.front()->state = 0; // lock is granted
        node_to_params[p0id].first++;
        
        for(auto it = locklist.begin(); it != locklist.end(); it++) {
            string p1id = (*it)->node_name;
            // 释放指向拥有者的边
            lock_graph[p1id].remove(node_name); // remove edge to removed node_name
            printf("remove edge(%s->%s)\n", p1id.c_str(), node_name.c_str());
            if(p1id != p0id) {
                // 向新的拥有者添加一条边
                lock_graph[p1id].push_back(p0id.c_str()); // add new edge
                printf("add edge(%s->%s)\n", p1id.c_str(), p0id.c_str());
            }
        }
    }
    
    return 0;
}

// 找需要释放的node
bool LockManager::isDeadLock(string& tokill) {
    // 节点对应的 SCC
    map<string, vector<string>> node_to_SCC;
    calSCC(node_to_SCC);
    for(const auto& cyc : node_to_SCC) {
        const auto& vec = cyc.second;
        printf("detected deadlock: ");
        int minlocks = 1e8;
        string releaseNode = "";
        for(auto it = vec.begin(); it != vec.end(); it++){
            printf("%s->", (*it).c_str());
            // 找victim(锁最少的节点)
            int nlock = node_to_params[*it].first;
            if (nlock < minlocks) {
                minlocks = nlock;
                releaseNode = *it;
            }
        }
        printf("%s\n", vec.front().c_str());
        if(releaseNode != "") {
            printf("***Can release node_name=%s(%d) to break deadlock.***\n", releaseNode.c_str(), minlocks);
            tokill = releaseNode;
        }
    }
    return node_to_SCC.size() > 0;
}

void LockManager::releaseProcess(string node_name) {
    if(node_to_params.count(node_name)) {
        list<shared_ptr<Lock>> tmplist = node_to_params[node_name].second;
        for(const auto& p_lock: tmplist) {
            releaseLockInternal(p_lock);
        }
        node_to_params.erase(node_name);
    };

    if(lock_graph.count(node_name)) lock_graph.erase(node_name);
    node_set.erase(node_name);
    printf("erase node_name %s\n", node_name.c_str());
}

// ============== deadlock detector ==============

void LockManager::startDetection(int interval) {
    if(deadlock_checker != nullptr) {
        check_interval = interval;
        deadlock_checker = new thread([this]{
            this->detectDeadlock();
        });
    }
}

void LockManager::stopDeadlockDetector() {
    stop = true;
    if(deadlock_checker && deadlock_checker->joinable()) {
        printf("deadlock detector is stoped\n");
        deadlock_checker->join();
        deadlock_checker = nullptr;
    }
}

void LockManager::detectDeadlock() {
    using std::chrono::system_clock;
    while (!stop) {
        string tokill;
        mtx.lock();

        // v0, 只死锁检查
        // isDeadLock(tokill);

        // v1, 释放锁，有可能引发新的死锁，循环检测
        while(isDeadLock(tokill)) {
            releaseProcess(tokill);
        }
        mtx.unlock();
        
        // 控制检测频率
        std::time_t tt = system_clock::to_time_t (system_clock::now());
        struct std::tm *ptm = std::localtime(&tt);
        ptm->tm_sec += check_interval;
        std::this_thread::sleep_until(system_clock::from_time_t(mktime(ptm)));
    }
}

// ===========计算有向图中的强连通分量=============
void reverseGraph(map<string, list<string>>& origin, map<string, list<string>>& dest) {
    for(const auto& p : origin) {
        string e = p.first;
        const auto& vec = p.second;
        for(auto v : vec) {
            if (!dest.count(v)) dest[v] = list<string>{};
            dest[v].push_back(e);
        }
    }
}

void dfs(map<string, list<string>>& graph, string cur, set<string>& visited, vector<string>& order) {
    if (visited.count(cur)) 
        return;
    visited.insert(cur);
    for (auto x: graph[cur]) {
        dfs(graph, x, visited, order);
    }
    order.push_back(cur);
}

void topSort(map<string, list<string>>& graph, set<string>& node_set, vector<string>& order) {
    set<string> visited;
    for(auto x: node_set) {
        dfs(graph, x, visited, order);
    }
}

void printVec(vector<string>& vec) {
    printf("vector[%s", vec.front().c_str());
    for(auto it = vec.begin()+1; it != vec.end(); it++) {
        printf(",%s", (*it).c_str());
    }
    printf("]\n");
}

void LockManager::calSCC(map<string, vector<string>>& node_to_SCC) {
    map<string, list<string>> reverse_graph;
    reverseGraph(lock_graph, reverse_graph);
    vector<string> top_order;
    topSort(reverse_graph, node_set, top_order);
    
    // DEBUG print
    // printf("top order:");
    // printVec(top_order);
    
    set<string> visited;
    for(int i = top_order.size()-1; i >= 0; i--) {
        auto vec = vector<string>{};
        dfs(lock_graph, top_order[i], visited, vec);
        // 找到强连通分量
        if (vec.size() > 1) 
            node_to_SCC[top_order[i]] = vec;
    }
}

void LockManager::print() {
    cout << "node_name to locks:[\n";
    for(const auto& p: node_to_params) {
        printf("(%s, %d, [", p.first.c_str(), p.second.first);
        for(const auto& q: p.second.second) {
            printf("(node_name=%s, param_name=%s, state=%d),", q->node_name.c_str(), q->param_name.c_str(), q->state);
        }
        printf("])\n");
    }
    cout << ']' << endl << "res to locks:[\n";
    for(const auto& p: param_to_locklist) {
        printf("(%s, [", p.first.c_str());
        for(const auto& q: p.second) {
            printf("(node_name=%s, param_name=%s, state=%d),", q->node_name.c_str(), q->param_name.c_str(), q->state);
        }
        printf("])\n");
    }
    cout << ']' << endl << "graph:[\n";
    for(const auto& p: lock_graph) {
        string e = p.first;
        printf("%s:[", e.c_str());
        for(const auto& q: p.second) {
            printf("%s,", q.c_str());
        }
        printf("]\n");
    }
    cout << ']' << endl << "node_set:[";
    for(auto x: node_set) printf("%s, ", x.c_str());
    cout << ']' << endl;
}


