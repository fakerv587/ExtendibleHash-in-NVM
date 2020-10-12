#ifndef GLOBAL_DEF
#define GLOBAL_DEF

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <libpmem.h>
#include <map>
#include <queue>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

#define DATA_PAGE_SLOT_NUM 32
#define BUCKET_SLOT_NUM 32
#define DEFAULT_CATALOG_SIZE 16
#define META_NAME "pm_ehash_metadata"
#define CATALOG_NAME "pm_ehash_catalog"
#define PM_EHASH_DIRECTORY "/mnt/mem/"

/*
---the physical address of data in NVM---
fileId: 1-N, the data page name
offset: data offset in the file
*/
typedef struct pm_address {
    uint32_t fileId;
    uint32_t offset;
    pm_address()
        : fileId(1)
        , offset(0)
    {
    }
    pm_address(uint32_t t_fileId, uint32_t t_offset)
        : fileId(t_fileId)
        , offset(t_offset)
    {
    }
    bool operator==(const struct pm_address& t1) const { return (fileId == t1.fileId) && (offset == t1.offset); }
    bool operator<(const struct pm_address& t1) const
    {
        if (fileId == t1.fileId)
            return offset < t1.offset;
        return fileId < t1.fileId;
    }
} pm_address;

template <> class std::hash<pm_address> {
public:
    size_t operator()(const pm_address& addr) const
    {
        return (hash<unsigned int>()(addr.fileId) << 4) ^ hash<unsigned int>()(addr.offset);
    }
};

/*
the data entry stored by the ehash
*/
typedef struct kv {
    uint64_t key;
    uint64_t value;
    kv()
        : key(0)
        , value(0)
    {
    }
} kv;

typedef struct pm_bucket {
    uint64_t local_depth;
    bool bitmap[BUCKET_SLOT_NUM]; // one bit for each slot
    kv slot[BUCKET_SLOT_NUM]; // one slot for one kv-pair
    pm_bucket()
        : local_depth(1)
    {
        for (size_t i = 0; i < BUCKET_SLOT_NUM; i++) {
            bitmap[i] = false;
            slot[i] = kv();
        }
    }
} pm_bucket;

// in ehash_catalog, the virtual address of buckets_pm_address[n] is stored in buckets_virtual_address
// buckets_pm_address: open catalog file and store the virtual address of file
// buckets_virtual_address: store virtual address of bucket that each buckets_pm_address points to
typedef struct ehash_catalog {
    pm_address* buckets_pm_address; // pm address array of buckets
    pm_bucket** buckets_virtual_address; // virtual address of buckets that buckets_pm_address point to
    ehash_catalog() {}
    ehash_catalog(uint64_t size)
    {
        buckets_pm_address = new pm_address[size];
        buckets_virtual_address = new pm_bucket*[size];
        for (uint64_t i = 0; i < size; i++) {
            buckets_virtual_address[i] = new pm_bucket;
        }
    }
} ehash_catalog;

typedef struct ehash_metadata {
    uint64_t max_file_id; // next file id that can be allocated 从0开始
    uint64_t catalog_size; // the catalog size of catalog file(amount of data entry)
    uint64_t global_depth; // global depth of PmEHash
} ehash_metadata;

// use pm_address to locate the data in the page
// uncompressed page format design to store the buckets of PmEHash
// one slot stores one bucket of PmEHash
typedef struct data_page {
    // fixed-size record design
    // uncompressed page format
    uint32_t fid; // 文件名 file id
    bool bitmap[DATA_PAGE_SLOT_NUM]; // one bit for each slot
    pm_bucket slot[DATA_PAGE_SLOT_NUM]; // one slot for one bucket
} data_page;

extern vector<data_page*> PageList;

#endif