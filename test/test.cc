////
// @file test.cc
// @brief
// 采用catch作为单元测试方案，需要一个main函数，这里定义。
//
// @author niexw
// @email xiaowen.nie.cn@gmail.com
//
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../include/Bp_tree.h"
#include <string>
#include <iostream>
using namespace NS_Bptree;
const int NODENUM = 100;
void printMeta(Meta &mymeta)
{
    printf(
        "meta_.blocksize=%ld\nmeta_.height = "
        "%ld\nmeta_.interNodeNUm=%ld\nmeta_.leafNodeNUm=%ld\nmeta_.root = "
        "%d\nmeta_.firstleaf = %d\n",
        mymeta.blocksize,
        mymeta.height,
        mymeta.interNodeNum,
        mymeta.leafNodeNum,
        mymeta.root,
        mymeta.firstLeaf);
}

TEST_CASE("BPLUSTREEtest")
{
    printf("welcome to use BPluss Tree\nplease input the path and "
           "blocksize:\n");
    std::string str;
    int size;
    scanf("%d", &size);
    // std::cin >> str >> size;
    // std::cout << str << std::endl << size << std::endl;
    // Bptree myBT(str.c_str(), size);

    // 新建一棵树
    Bptree BT("test.db", size);

    // 插入NODENUM以内的奇数
    for (int i = 1; i < NODENUM; i++) {
        REQUIRE(BT.insert(i, i * 10) == 1);
        i++;
    }

    // 从已有文件中读取一棵树
    Bptree myBT("test.db");

    // 插入NODENUM以内的偶数
    for (int i = 0; i < NODENUM; i++) {
        REQUIRE(myBT.insert(i, i * 10) == 1);
        i++;
    }

    printf("\n\n\n");
    printf("*******************************\n");
    printf("begin to search\n");
    printf("*******************************\n");
    printf("\n\n\n");
    int data = 0;
    for (int i = 0; i < NODENUM; i++) {
        i++;
        REQUIRE(myBT.search(i, &data) == 1);
        REQUIRE(data == i * 10);
        printf("data = %d\n", data);
    }
    REQUIRE(myBT.search(5000, &data) == -1);
    REQUIRE(data == -1);

    myBT.printfLeaf();
    char *buf = (char *) malloc(sizeof(Index) * 10);
    int num = 10;
    REQUIRE(myBT.rangeSearch(1, 20, buf, num) == 1);
    Index *index = reinterpret_cast<Index *>(buf);
    for (int i = 0; i < num; i++) {
        printf("%d   %d\n", index->key, index->blockNum);
        index++;
    }
    // REQUIRE(myBT.remove(1) == 1);
    // REQUIRE(myBT.search(1, &data) == -1);
    // 打印meta元数据
    // Meta mymeta;
    // mymeta = myBT.getMeta();
    // printMeta(mymeta);
}