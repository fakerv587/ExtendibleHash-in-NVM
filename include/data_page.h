#ifndef DATA_PAGE
#define DATA_PAGE

#include "global_def.h"

void WritePageToFile(data_page* page);

data_page* ReadPageFromFile(uint32_t fid);

void CreateNewPage(uint32_t fid);

#endif