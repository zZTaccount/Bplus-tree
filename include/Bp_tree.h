/**
 * @file Bp_tree.h
 * @brief
 *
 * @author zzt
 * @emial 429834658@qq.com
 **/
#ifndef __BP_TREE_H__
#define __BP_TREE_H__
#include "stdlib.h"
#include "stdio.h"
#include "math.h"
namespace NS_Bptree {

typedef int keyType;
typedef int dataType;
const off_t OFFSET_META = 0;

// const size_t SIZE_NO_CHILDREN =
//     sizeof(Node) - ORDER * sizeof(index); // TODO: whats it is?

enum NodeType
{
    interNode = 1,
    leafNode = 2,
    unUsedNode = 3
};

#pragma pack(push) // 一字节对齐
#pragma pack(1)
struct Index
{
    keyType key;  // 索引值
    int blockNum; // int型存储文件中的块号
};
#pragma pack(pop)

struct Node
{
    int checksum; // 校验和
    // int parent;   // int型存储block块号
    // int minptr; // 指向多出
    int layer;  // 当前节点层数
    int myself; // 存储自身的块号
    int next;   // 尾指针，不同的NodeType的指示意义不一样
    int prev;   // 前一个节点（兄弟节点）
    int keyNum; // 当前key个数
    NodeType type = leafNode; // 标识节点类型,默认为叶子节点
};

// B+树的元数据
typedef struct Meta
{
    size_t order;        // 树的度
    size_t blocksize;    // 树中存储的索引数量
    size_t interNodeNum; // 树的内部节点数
    size_t leafNodeNum;  // 树的叶子节点数
    size_t height;       // 树的高度
    int root;            // 根节点的偏移量
    int firstLeaf;       // 第一个叶子节点的偏移量
    int slot;            // 表示现在文件插入最新节点的偏移量
    int firstUnUsed;     // 表示回收节点链表的首节点
} Meta;
const off_t OFFSET_BLOCK = OFFSET_META + sizeof(struct Meta);

// B+树类
class Bptree
{
  private:
    char path_[256];       // 文件存储路径
    size_t blocksize_ = 0; // 块大小
    char *buf_[4];         // 缓冲区指针数组
    Meta meta_;            // B+树的元数据
    FILE *fp_;             // 文件描述符
    int fpLevel_;          // TODO:

    int ORDER; // 树的度为节点最大子树数目
    size_t MINNUM_KEY;
    size_t MAXNUM_KEY;
    size_t MINNUM_CHILD;
    size_t MAXNUM_CHILD;

  public:
    Bptree(const char *path);
    Bptree(const char *path, int blocksize);
    // 查询
    int search(keyType key, int *data);
    // 范围查询，将查询得到的index数组存入给定buffur
    // num参数传入时代表buf容量，传出时代表查询到的索引个数
    int rangeSearch(
        const keyType left,
        const keyType right,
        void *resultBuffer,
        int &num);
    // 删除
    int remove(keyType key);
    // 插入
    int insert(keyType key, int data);
    // 修改
    int modify(keyType key, int data);
    // 获取元数据
    Meta getMeta()
    {
        take_meta(&meta_);
        return meta_;
    }

    // #ifndef TEST
    //   private:
    // #else
  public:
    // #endif
    // 初始化
    void init();
    // 给定层数，寻找索引
    int searchIndex(keyType key, int layer);
    // 寻找叶子节点
    int searchLeaf(keyType key);
    // 移除内部节点
    void removeInterNode(off_t offset, Node &node, keyType key);
    // 在叶子节点添加数据
    int insertNoSplit(Node *leaf, keyType key, dataType data);
    // 节点分裂
    void insertWithSplit(Node *node, keyType key, dataType data);
    // 向父节点插入新增的索引
    void insertParent(int parentnum, keyType key, int data);
    // 更新父节点的索引
    void updateParent(int parentnum, keyType oldkey, keyType newkey);
    // 直接删除节点中的key
    int removeDirect(Node *leaf, keyType key);
    // 寻找兄弟节点
    // Node *findBrother(Node *node);
    // 向兄弟节点借一个key
    int borrowBroKey(Node *node);
    // 合并两个节点
    void mergeBrother(Node *node);
    // 获取空闲buf
    char *getBuf();
    // 打印所有叶子节点
    void printfLeaf();

  private: // 文件操作
    void
    openFile(const char *mode = "rb+") //"rb+"可读可写方式打开一个二进制文件
    {
        if (fpLevel_ == 0) fp_ = fopen(path_, mode);
        ++fpLevel_; // TODO:
    }
    void closeFile()
    {
        if (fpLevel_ == 1) fclose(fp_);
        --fpLevel_;
    }
    inline int allocInterNode(Node *node);
    inline int allocLeafNode(Node *node);

    void unallocLeafNode(Node *leaf) { --meta_.leafNodeNum; }
    void unallocInterNode(Node *node) { --meta_.interNodeNum; }

    // 从文件中读出一个块
    // template <typename T>
    int take(void *block, int blocknum)
    {
        printf("in the take function\n");
        off_t offset = blocknum * blocksize_;
        openFile();
        fseek(fp_, offset + OFFSET_BLOCK, SEEK_SET); // 设置文件位置偏移量
        size_t rd = fread(
            block,
            blocksize_,
            1,
            fp_); // 返回的是读取的元素个数，而不是读取字节数
        closeFile();
        return rd - 1; // 返回0表示读取成功，返回-1表示读取失败
    }

    // 存入一个块到文件内
    int save(void *block, int blocknum)
    {
        printf("in the save function\n");
        off_t offset = blocknum * blocksize_;
        openFile();
        fseek(fp_, offset + OFFSET_BLOCK, SEEK_SET);
        size_t wd = fwrite(block, blocksize_, 1, fp_);
        closeFile();
        return wd - 1; // 返回0表示存储成功，返回-1表示存储失败
    }

    // 存储meta元数据
    int save_meta(Meta *meta)
    {
        printf("in the save_meta function\n");
        openFile();
        fseek(fp_, OFFSET_META, SEEK_SET);
        size_t wd = fwrite(meta, sizeof(Meta), 1, fp_);
        closeFile();
        return wd - 1;
    }

    // 取出meta元数据
    int take_meta(Meta *meta)
    {
        printf("in the take_meta function\n");
        openFile();
        fseek(fp_, OFFSET_META, SEEK_SET);
        size_t rd = fread(meta, sizeof(Meta), 1, fp_);
        closeFile();
        return rd - 1; // 返回0表示读取成功，返回-1表示读取失败
    }

    // 定位key值对应的区间
    int locate(Index index[], int childNum, int key);

    // 叶子节点的索引进行二分查找
    int binarySearch(Index index[], int childNum, keyType key, int *i = NULL);
    // 打印该点
    void printfNode(Node *node);
};
} // namespace NS_Bptree
#endif // __BP_TREE_H__