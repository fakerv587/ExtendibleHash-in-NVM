#include "data_page.h"

// 数据页表的相关操作实现都放在这个源文件下，如PmEHash申请新的数据页和删除数据页的底层实现

// 更新页表数据到file
void WritePageToFile(data_page* page) { pmem_persist(page, sizeof(page)); }

// 读取文件转换为页表
data_page* ReadPageFromFile(uint32_t fid)
{
    size_t map_len;
    int is_pmem;
    string tmp = PM_EHASH_DIRECTORY + to_string((int)fid);
    const char* filename = tmp.c_str();
    data_page* ret = (data_page*)pmem_map_file(filename, sizeof(data_page), PMEM_FILE_CREATE, 0777, &map_len, &is_pmem);
    PageList.push_back(ret);
    return ret;
}

// 创建新的页表
void CreateNewPage(uint32_t fid)
{
    size_t map_len;
    int is_pmem;
    string tmp = PM_EHASH_DIRECTORY + to_string((int)fid);
    const char* filename = tmp.c_str();
    data_page* tpage
        = (data_page*)pmem_map_file(filename, sizeof(data_page), PMEM_FILE_CREATE, 0777, &map_len, &is_pmem);
    for (int i = 0; i < DATA_PAGE_SLOT_NUM; i++) {
        (tpage->slot)[i] = pm_bucket();
    }
    PageList.push_back(tpage);
}
