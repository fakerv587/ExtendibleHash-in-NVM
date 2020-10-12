#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

#include "data_page.h"
#include "pm_ehash.h"

using namespace std;

const string DIR_PREFIX = "./workloads/";
const string LOAD_SUFFIX = "-load.txt";
const string RUN_SUFFIX = "-run.txt";
const vector<string> FILENAME_LIST = { "1w-rw-50-50", "10w-rw-0-100", "10w-rw-100-0", "10w-rw-25-75", "10w-rw-75-25",
    "10w-rw-50-50", "220w-rw-50-50" };

void load_bench(PmEHash& PmE, string filename)
{
    filename = DIR_PREFIX + filename + LOAD_SUFFIX;
    printf("filename: %s\n", filename.c_str());
    freopen(filename.c_str(), "r", stdin);
    char op[10];
    uint64_t num;
    int count = 0;
    auto start = chrono::steady_clock::now();
    while (scanf("%s%llu", op, &num) != EOF) {
        kv newdata;
        newdata.key = num;
        newdata.value = num;
        int result = PmE.insert(newdata);
        count++;
    }
    auto end = chrono::steady_clock::now();
    chrono::duration<double> elapsed_seconds = end - start;
    printf("load time: %lfs\n", elapsed_seconds.count());
    printf("load OPS: %.2lf/s\n", count / elapsed_seconds.count());
    printf("\n");
    fclose(stdin);
}

void run_bench(PmEHash& PmE, string filename)
{
    filename = DIR_PREFIX + filename + RUN_SUFFIX;
    printf("filename: %s\n", filename.c_str());
    freopen(filename.c_str(), "r", stdin);
    char op[10];
    uint64_t num;
    int count = 0;
    auto start = chrono::steady_clock::now();
    while (scanf("%s%llu", op, &num) != EOF) {
        kv newdata;
        newdata.key = num;
        newdata.value = num;
        if (op[0] == 'U') {
            PmE.update(newdata);
            count++;
        } else if (op[0] == 'I') {
            PmE.insert(newdata);
            count++;
        } else if (op[0] == 'R') {
            PmE.search(newdata.key, newdata.value);
            count++;
        }
    }
    auto end = chrono::steady_clock::now();
    chrono::duration<double> elapsed_seconds = end - start;
    printf("run time: %lfs\n", elapsed_seconds.count());
    printf("run OPS: %.2lf/s\n", count / elapsed_seconds.count());
    printf("\n");
    fclose(stdin);
}

int main()
{
    for (auto& filename : FILENAME_LIST) {
        PmEHash PmE(true);
        load_bench(PmE, filename);
        run_bench(PmE, filename);
        PmE.selfDestory();
    }
    return 0;
}
