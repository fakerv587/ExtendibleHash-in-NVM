#ifndef _PM_E_HASH_H
#define _PM_E_HASH_H

#include "data_page.h"

using namespace std;

class PmEHash {
private:
    ehash_metadata* metadata; // virtual address of metadata, mapping the metadata file
    ehash_catalog catalog; // the catalog of hash

    queue<pm_bucket*> free_list; // all free slots in data pages to store buckets
    unordered_map<pm_bucket*, pm_address> vAddr2pmAddr; // map virtual address to pm_address, used to find specific pm_address
    unordered_map<pm_address, pm_bucket*>
        pmAddr2vAddr; // map pm_address to virtual address, used to find specific virtual address

    bool clearAfterDeconstruct;

    uint64_t hashFunc(uint64_t key);

    pm_bucket* getFreeBucket(uint64_t key);
    pm_bucket* getNewBucket();
    void freeEmptyBucket(pm_bucket* bucket);
    kv* getFreeKvSlot(pm_bucket* bucket, int flag);

    void splitBucket(uint64_t bucket_id);
    void mergeBucket(uint64_t bucket_id);

    void extendCatalog();
    pm_bucket* getFreeSlot(pm_address& new_address);
    void allocNewPage();

    void recoverMetadata();
    void recoverCatalog();
    void mapAllPage();

public:
    PmEHash();
    PmEHash(bool flag);
    ~PmEHash();

    int insert(kv new_kv_pair);
    int remove(uint64_t key);
    int update(kv kv_pair);
    int search(uint64_t key, uint64_t& return_val);

    void selfDestory();
};

#endif
