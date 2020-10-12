#include "pm_ehash.h"

vector<data_page*> PageList;

/**
 * @description: construct a new instance of PmEHash in a default directory
 * @param NULL
 * @return: new instance of PmEHash
 */
PmEHash::PmEHash()
    : clearAfterDeconstruct(false)
{

    recoverMetadata();
    mapAllPage();
    recoverCatalog();
}

/**
 * @description: construct a new instance of PmEHash in a default directory
 * @param flag: 指示是否需要在析构后清除所有数据
 * @return: new instance of PmEHash
 */
PmEHash::PmEHash(bool flag)
    : clearAfterDeconstruct(flag)
{
    recoverMetadata();
    mapAllPage();
    recoverCatalog();
}

/**
 * @description: persist and munmap all data in NVM
 * @param NULL
 * @return: NULL
 */
PmEHash::~PmEHash()
{
    // 保存metadata ：
    pmem_persist(metadata, sizeof(metadata));
    pmem_unmap(metadata, sizeof(metadata));
    // 保存catalog ：
    size_t map_len;
    int is_pmem;
    ehash_catalog* cat = (ehash_catalog*)pmem_map_file(
        "/mnt/mem/pm_ehash_catalog", sizeof(ehash_catalog), PMEM_FILE_CREATE, 0777, &map_len, &is_pmem);
    *cat = this->catalog;
    pmem_persist(cat, sizeof(cat));
    pmem_unmap(cat, sizeof(cat));
    // 保存page ：
    for (auto& p : PageList) {
        WritePageToFile(p);
        pmem_unmap(p, sizeof(p));
    }
    if (clearAfterDeconstruct)
        selfDestory();
}

/**
 * @description: 插入新的键值对，并将相应位置上的位图置1
 * @param kv: 插入的键值对
 * @return: 0 = insert successfully, -1 = fail to insert(target data with same key exist)
 */
int PmEHash::insert(kv new_kv_pair)
{
    uint64_t return_val = 0;
    if (search(new_kv_pair.key, return_val) == 0) {
        return -1;
    }
    pm_bucket* bucket = getFreeBucket(new_kv_pair.key);
    kv* freePlace = getFreeKvSlot(bucket, 1);
    *freePlace = new_kv_pair;
    return 0;
}

/**
 * @description: 删除具有目标键的键值对数据，不直接将数据置0，而是将相应位图置0即可
 * @param uint64_t: 要删除的目标键值对的键
 * @return: 0 = removing successfully, -1 = fail to remove(target data doesn't exist)
 */
int PmEHash::remove(uint64_t key)
{
    uint64_t h = hashFunc(key);
    if (h >= this->metadata->catalog_size)
        return -1;
    kv* tslot = this->catalog.buckets_virtual_address[h]->slot;
    bool* tbitmap = this->catalog.buckets_virtual_address[h]->bitmap;
    for (int i = 0; i < BUCKET_SLOT_NUM; i++) {
        if (tbitmap[i] == 1 && tslot[i].key == key) {
            this->catalog.buckets_virtual_address[h]->bitmap[i] = 0;
            mergeBucket(h);
            return 0;
        }
    }
    return -1;
}

/**
 * @description: 更新现存的键值对的值
 * @param kv: 更新的键值对，有原键和新值
 * @return: 0 = update successfully, -1 = fail to update(target data doesn't exist)
 */
int PmEHash::update(kv kv_pair)
{
    uint64_t h = hashFunc(kv_pair.key);
    if (h >= this->metadata->catalog_size)
        return -1;
    kv* tslot = this->catalog.buckets_virtual_address[h]->slot;
    bool* tbitmap = this->catalog.buckets_virtual_address[h]->bitmap;
    for (int i = 0; i < BUCKET_SLOT_NUM; i++) {
        if (tbitmap[i] == 1 && tslot[i].key == kv_pair.key) {
            tslot[i].value = kv_pair.value;
            return 0;
        }
    }
    return -1;
}

/**
 * @description: 查找目标键值对数据，将返回值放在参数里的引用类型进行返回
 * @param uint64_t: 查询的目标键
 * @param uint64_t&: 查询成功后返回的目标值
 * @return: 0 = search successfully, -1 = fail to search(target data doesn't exist)
 */
int PmEHash::search(uint64_t key, uint64_t& return_val)
{
    uint64_t h = hashFunc(key);
    if (h >= this->metadata->catalog_size)
        return -1;
    kv* tslot = this->catalog.buckets_virtual_address[h]->slot;
    bool* tbitmap = this->catalog.buckets_virtual_address[h]->bitmap;
    for (int i = 0; i < BUCKET_SLOT_NUM; i++) {
        if (tbitmap[i] == 1 && tslot[i].key == key) {
            return_val = tslot[i].value;
            return 0;
        }
    }
    return -1;
}

/**
 * @description: 用于对输入的键产生哈希值，然后取模求桶号(自己挑选合适的哈希函数处理)
 * @param uint64_t: 输入的键
 * @return: 返回键所属的桶号
 */
uint64_t PmEHash::hashFunc(uint64_t key)
{
    uint64_t gd = this->metadata->global_depth;
    uint64_t h = key & ((1ull << gd) - 1);
    return h;
}

/**
 * @description: 获得供插入的空闲的桶，无空闲桶则先分裂桶然后再返回空闲的桶
 * @param uint64_t: 带插入的键
 * @return: 空闲桶的虚拟地址
 */
pm_bucket* PmEHash::getFreeBucket(uint64_t key)
{
    uint64_t h = hashFunc(key);
    pm_bucket* curbucket = this->catalog.buckets_virtual_address[h];
    while (getFreeKvSlot(curbucket, 0) == NULL) {
        splitBucket(h);
        h = hashFunc(key);
        curbucket = this->catalog.buckets_virtual_address[h];
    }
    return curbucket;
}

/**
 * @description: 获得空闲桶内第一个空闲的位置供键值对插入,
 * 传入的flag若为0，则只是简单查询是否有空闲slot，若为1，则是真正返回slot，需要将标志位置1
 * @param pm_bucket* bucket， int flag
 * @return: 空闲键值对位置的虚拟地址
 */
kv* PmEHash::getFreeKvSlot(pm_bucket* bucket, int flag)
{
    bool* tbitmap = bucket->bitmap;
    for (int i = 0; i < BUCKET_SLOT_NUM; i++) {
        if (tbitmap[i] == 0) {
            if (flag == 1) {
                (bucket->bitmap)[i] = 1;
            }
            return &(bucket->slot[i]);
        }
    }
    return NULL;
}

/**
 * @description: 桶满后进行分裂操作，可能触发目录的倍增
 * @param uint64_t: 目标桶在目录中的序号
 * @return: NULL
 */
void PmEHash::splitBucket(uint64_t bucket_id)
{
    pm_bucket* curbucket = this->catalog.buckets_virtual_address[bucket_id];
    if (curbucket->local_depth == this->metadata->global_depth) {
        extendCatalog();
    }
    vector<kv> new_kv;
    new_kv.reserve(BUCKET_SLOT_NUM);
    bool* tbitmap = curbucket->bitmap;
    for (int i = 0; i < BUCKET_SLOT_NUM; ++i) {
        if (tbitmap[i] != 0) {
            new_kv.push_back(curbucket->slot[i]);
            (curbucket->bitmap)[i] = 0;
        }
    }
    uint64_t newDepth = curbucket->local_depth + 1;
    pm_bucket newbucket;
    curbucket->local_depth = newDepth;
    newbucket.local_depth = newDepth;
    this->catalog.buckets_virtual_address[bucket_id] = curbucket;
    *(this->catalog.buckets_virtual_address[bucket_id + (1 << (this->metadata->global_depth - 1))]) = newbucket;
    for (size_t i = 0; i < new_kv.size(); ++i) {
        this->insert(new_kv[i]);
    }
}

/**
 * @description: 桶空后，回收桶的空间，并设置相应目录项指针
 * @param uint64_t: 桶号
 * @return: NULL
 */
void PmEHash::mergeBucket(uint64_t bucket_id)
{
    if (bucket_id < (1 << (this->metadata->global_depth - 1))) {
        *(this->catalog.buckets_virtual_address[bucket_id])
            = *(this->catalog.buckets_virtual_address[bucket_id + (1 << (this->metadata->global_depth - 1))]);
        this->catalog.buckets_virtual_address[bucket_id]->local_depth--;
        *(this->catalog.buckets_virtual_address[bucket_id + (1 << (this->metadata->global_depth - 1))]) = pm_bucket();
    } else {
        this->catalog.buckets_virtual_address[bucket_id - (1 << (this->metadata->global_depth - 1))]->local_depth--;
        *(this->catalog.buckets_virtual_address[bucket_id]) = pm_bucket();
    }
}

/**
 * @description: 对目录进行倍增，需要重新生成新的目录文件并复制旧值，然后删除旧的目录文件
 * @param NULL
 * @return: NULL
 */
void PmEHash::extendCatalog()
{
    this->metadata->global_depth += 1;
    this->metadata->catalog_size = (1 << (this->metadata->global_depth));
    ehash_catalog newcatalog(this->metadata->catalog_size);
    if (this->metadata->global_depth == 1) {
        for (int i = 0; i < 2; i++) {
            newcatalog.buckets_virtual_address[i] = getFreeSlot(newcatalog.buckets_pm_address[i]);
        }
    } else {
        for (int i = 0; i < (1 << (this->metadata->global_depth - 1)); ++i) {
            newcatalog.buckets_virtual_address[i] = this->catalog.buckets_virtual_address[i];
            newcatalog.buckets_pm_address[i] = this->catalog.buckets_pm_address[i];
        }
        for (int i = (1 << (this->metadata->global_depth - 1)); i < (1 << this->metadata->global_depth); ++i) {
            newcatalog.buckets_virtual_address[i] = getFreeSlot(newcatalog.buckets_pm_address[i]);
        }
    }
    this->catalog = newcatalog;
}

/**
 * @description: 获得一个可用的数据页的新槽位供哈希桶使用，如果没有则先申请新的数据页
 * @param pm_address&: 新槽位的持久化文件地址，作为引用参数返回
 * @return: 新槽位的虚拟地址
 */
pm_bucket* PmEHash::getFreeSlot(pm_address& new_address)
{
    if (free_list.empty())
        allocNewPage();
    pm_bucket* ret = free_list.front();
    free_list.pop();
    new_address = vAddr2pmAddr[ret];
    return ret;
}

/**
 * @description: 申请新的数据页文件，并把所有新产生的空闲槽的地址放入free_list等数据结构中
 * @param NULL
 * @return: NULL
 */
void PmEHash::allocNewPage()
{
    CreateNewPage(metadata->max_file_id);
    for (int i = 0; i < DATA_PAGE_SLOT_NUM; i++) {
        free_list.push(&(PageList.back()->slot)[i]);
        pm_address paddr;
        paddr.fileId = metadata->max_file_id;
        paddr.offset = i;
        vAddr2pmAddr[&(PageList.back()->slot)[i]] = paddr;
        pmAddr2vAddr[paddr] = &(PageList.back()->slot)[i];
    }
    metadata->max_file_id++;
}

/**
 * @description: 恢复metadata文件数据
 * @param NULL
 * @return: NULL
 */
void PmEHash::recoverMetadata()
{
    size_t map_len;
    int is_pmem;
    this->metadata = (ehash_metadata*)pmem_map_file(
        "/mnt/mem/pm_ehash_metadata", sizeof(ehash_metadata), PMEM_FILE_CREATE, 0777, &map_len, &is_pmem);
}

/**
 * @description: 恢复catalog文件数据
 * @param NULL
 * @return: NULL
 */
void PmEHash::recoverCatalog()
{
    size_t map_len;
    int is_pmem;
    ehash_catalog* cat = (ehash_catalog*)pmem_map_file(
        "/mnt/mem/pm_ehash_catalog", sizeof(ehash_catalog), PMEM_FILE_CREATE, 0777, &map_len, &is_pmem);
    this->catalog = *cat;
    if (this->metadata->global_depth == 0) {
        extendCatalog();
    }
}

/**
 * @description: 重启时，将所有数据页进行内存映射，设置地址间的映射关系，空闲的和使用的槽位都需要设置
 * @param NULL
 * @return: NULL
 */
void PmEHash::mapAllPage()
{
    // page 恢复
    // 获取当前目录下page+num的文件数，然后调用ReadPageFromFile(fid)获取：
    for (uint64_t i = 0; i < metadata->max_file_id; i++) {
        data_page* cur = ReadPageFromFile(i);
        // 根据map过来的datapage指针的信息设置下述：
        // 设置地址间的映射关系 （vAddr2pmAddr pmAddr2vAddr）
        // 空闲的和使用的槽位都需要设置 （free_list）
        for (size_t j = 0; j < DATA_PAGE_SLOT_NUM; j++) {
            pm_address curaddress;
            curaddress.fileId = (uint32_t)i;
            curaddress.offset = (uint32_t)j;
            vAddr2pmAddr[&(cur->slot)[j]] = curaddress;
            pmAddr2vAddr[curaddress] = &(cur->slot)[j];
            if ((cur->bitmap)[i] == 0) {
                free_list.push(&(cur->slot)[j]);
            }
        }
    }
}

/**
 * @description:
 * 删除PmEHash对象所有数据页，目录和元数据文件，主要供gtest使用。即清空所有可扩展哈希的文件数据，不止是内存上的
 * @param NULL
 * @return: NULL
 */
void PmEHash::selfDestory()
{
    char t[100];
    sprintf(t, "sudo find %s -mindepth 1 -delete", PM_EHASH_DIRECTORY);
    system(t);
    PageList.clear();
}