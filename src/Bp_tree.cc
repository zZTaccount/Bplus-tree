/**
 * @file Bp_tree.cc
 * @brief
 *
 * @author zzt
 * @emial 429834658@qq.com
 **/
#include "Bp_tree.h"
#include <stdlib.h>
#include <string.h>
namespace NS_Bptree {

/**********************************
 *public接口，B+树对外接口
 **********************************/
// 构造函数：从文件中读取一个已存在的树
Bptree::Bptree(const char *path)
    : fp_(NULL)
    , fpLevel_(0)
{
    // memset(path_, 0, sizeof(path)); //TODO:sizeof(path)应该是4字节吧
    strcpy(path_, path);

    if (take_meta(&meta_) != 0)
        printf("can't read the meta info!\n"); // 读取元数据失败，无法构建树
    blocksize_ = meta_.blocksize;
    ORDER = meta_.order;
    MINNUM_KEY = ceil(ORDER / 2.0) - 1;
    MAXNUM_KEY = ORDER - 1;
    MINNUM_CHILD = MINNUM_KEY + 1;
    MAXNUM_CHILD = MAXNUM_KEY + 1;
}

// 构造函数：新建一棵树
Bptree::Bptree(const char *path, int blocksize)
    : blocksize_(blocksize)
    , fp_(NULL)
    , fpLevel_(0)
{
    // memset(path_, 0, sizeof(path));
    strcpy(path_, path);

    openFile("w+"); // 该模式下文件存在则清0，若不存在则建立该文件。
    init();
    closeFile();
}

// 查询,成功返回1，将查询结果存入data中;查询不到返回-1
int Bptree::search(keyType key, int *data)
{
    printf("in the search function\n");
    int leaf;
    leaf = searchLeaf(key);
    // printf("the leafNum = %d\n", leaf);
    Node *leafnode;
    char *buf = getBuf();
    take(buf, leaf);
    leafnode = reinterpret_cast<Node *>(buf);
    // printfNode(leafnode);
    *data =
        binarySearch((Index *) (leafnode + 0x1), leafnode->keyNum, key, NULL);
    if ((*data) != -1)
        return 1;
    else
        return -1;
}

// 范围查询
int Bptree::rangeSearch(
    const keyType left,
    const keyType right,
    void *resultBuffer,
    int &num)
{
    Index *result = reinterpret_cast<Index *>(resultBuffer);
    int max = num;
    num = 0;
    int nextNode = searchLeaf(left);
    char *buf;
    Node *node;
    Index *p;
    for (int i = meta_.leafNodeNum; i > 0; i--) {
        buf = getBuf();
        take(buf, nextNode);
        node = reinterpret_cast<Node *>(buf);
        p = (Index *) (node + 0x1);
        if (node->keyNum <= (int) MAXNUM_KEY) {
            for (int j = node->keyNum; j > 0; j--) {
                if (p->key < left) {
                    p++;
                } else {
                    if (p->key <= right) {
                        result->key = p->key;
                        result->blockNum = p->blockNum;
                        num++;
                        if (num == max) return 1; // 已达到给定的buf容量最大值
                        result++;
                        p++;
                    } else
                        return 1;
                }
            }
            nextNode = node->next;
        } else
            return -1;
    }
    return -1;
}

// 插入节点,成功返回1;失败返回-1
int Bptree::insert(keyType key, int data)
{
    printf("in the insert function\n");
    int leafNum;
    int result;
    leafNum = searchLeaf(key);
    if (leafNum >= 0) {
        Node *leaf;
        char *buf = getBuf();
        take(buf, leafNum);
        leaf = reinterpret_cast<Node *>(buf);
        printf("in the insert fun,print the found leafNode\n");
        printfNode(leaf);
        result = binarySearch((Index *) (leaf + 0x1), leaf->keyNum, key, NULL);
        if (result != -1) {
            {
                printf("key already exist!\n");
                return -1;
            }
        } else {
            if (leaf->keyNum + 1 <= (int) MAXNUM_KEY) // 合法插入
            {
                insertNoSplit(leaf, key, data);
                return 1;
            } else // 需要分裂叶子节点
            {
                insertWithSplit(leaf, key, data);
                return 1;
            }
        }
    } else
        return -1;
}

// 删除节点，成功返回1，失败返回-1
int Bptree::remove(keyType key)
{
    int leafnum = searchLeaf(key);
    char *buf = getBuf();
    take(buf, leafnum);
    Node *leaf = reinterpret_cast<Node *>(buf);
    if (binarySearch((Index *) (leaf + 0x1), leaf->keyNum, key) == -1)
        return -1;
    else {
        removeDirect(leaf, key);
        if (leaf->keyNum < (int) MINNUM_KEY) {
            if (borrowBroKey(leaf) == -1) {
                mergeBrother(leaf);
                return 1;
            } else
                return 1;
        }
        return 1;
    }
}

/**********************************
 *private接口，B+树内部函数
 **********************************/
// 初始化
void Bptree::init()
{
    // 初始化元数据
    memset(&meta_, 0, sizeof(Meta));
    meta_.order = ORDER =
        (size_t) floor((blocksize_ - sizeof(Node)) / sizeof(Index));
    meta_.blocksize = blocksize_;
    MINNUM_KEY = ceil(ORDER / 2.0) - 1;
    MAXNUM_KEY = ORDER - 1;
    MINNUM_CHILD = MINNUM_KEY + 1;
    MAXNUM_CHILD = MAXNUM_KEY + 1;
    printf(
        "sizeof Node is %ld , sizeof Index is %ld\n",
        sizeof(Node),
        sizeof(Index));
    printf("order = %ld,minnum_key = %ld\n", meta_.order, MINNUM_KEY);
    meta_.height = 1;
    meta_.slot = 0;

    // 初始化根节点和第一个叶子节点
    Node root;
    root.next = -1;
    root.prev = -1;
    root.layer = 1;
    root.myself = allocLeafNode(&root);
    meta_.root = root.myself;
    meta_.firstLeaf = root.myself;

    // 写入磁盘
    save_meta(&meta_);
    save(&root, meta_.root);
}

// 寻找键值key对应的索引所在的内部节点的偏移量
int Bptree::searchIndex(keyType key, int layer)
{
    printf("in the searchIndex function\n");

    int blocknum = meta_.root;
    int result = 0;
    int h = meta_.height - layer;
    Node *node = NULL;
    char *buf = getBuf();
    while (h >= 1) {
        take(buf, blocknum);
        node = reinterpret_cast<Node *>(buf);
        // printfNode(node);
        result = locate((Index *) (node + 0x1), node->keyNum, key);
        if (result != -1) {
            blocknum = result;
            --h;
        } else // locate返回-1，表示key对应最后一棵子树
        {
            blocknum = node->next;
            if (blocknum == -1)
                printf("\nERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\nnext=-"
                       "1\n");
            --h;
        }
    }
    printf("in the searchIndex fun,return blocknum =%d\n", blocknum);
    return blocknum;
}

// 返回key对应的叶子节点的偏移量
int Bptree::searchLeaf(keyType key) { return searchIndex(key, 1); }

int Bptree::insertNoSplit(Node *node, keyType key, dataType data)
{
    printf("*******************************\n");
    printf("in the insertNoSplit function\n");
    printf("*******************************\n");
    Index *p = (Index *) (node + 0x1);

    int i = node->keyNum;
    bool updateFlag = false;
    keyType oldkey;
    if (p[0].key > key) {
        updateFlag = true;
        oldkey = p[0].key;
    }
    if (i != 0) {
        Index *q = p + i - 1;
        Index *t = q + 1;
        while (p->key < key && i-- > 0) {
            p++;
        }
        while (p <= q) {
            printf("p<=q\n");
            t->key = q->key;
            t->blockNum = q->blockNum;
            t = q;
            q--;
        }
    }
    p->key = key;
    p->blockNum = data;
    node->keyNum++;
    save(node, node->myself);
    if (updateFlag == true) {
        if (node->layer < (int) meta_.height) {
            int parentnum = searchIndex(oldkey, node->layer + 1);
            updateParent(
                parentnum,
                oldkey,
                key); //最小值更新了应该要更新父节点的索引
        }
    }
    // printf("\n\nafter insertNoSplit\n");
    // p = (Index *) (node + 0x1);
    // for (int i = node->keyNum; i > 0; i--) {
    //     printf("the p->key =%d\n", p->key);
    //     p++;
    // }
    // printf("\n\n");
    return 1;
}

void Bptree::insertWithSplit(Node *node, keyType key, dataType data)
{
    printf("*******************************\n");
    printf("in the insertWithSplit function\n");
    printf("*******************************\n");
    // 先插入再分裂
    insertNoSplit(node, key, data);
    int nodeNum = node->myself;

    // 新建并初始化一个新节点
    char *buf = getBuf();
    Node *newNode = reinterpret_cast<Node *>(buf);
    int newNodeNum;
    if (node->type == leafNode)
        newNodeNum = allocLeafNode(newNode);
    else
        newNodeNum = allocInterNode(newNode);
    newNode->layer = node->layer;
    newNode->myself = newNodeNum;
    newNode->prev = node->myself;
    newNode->next = node->next;
    newNode->keyNum = ceil(node->keyNum / 2);
    if (node->type == leafNode) node->next = newNodeNum;
    node->keyNum -= ceil(node->keyNum / 2);

    // 保存将提升到root的key
    Index *i = (Index *) (node + 0x1);
    i += node->keyNum;
    keyType rootKey0 = i->key;
    int key0num = i->blockNum;
    if (node->type == interNode) {
        // 将原节点的一半索引减去第一个复制到新节点
        newNode->keyNum--;
        memcpy(
            (newNode + 0x1),
            ((Index *) (node + 0x1) + node->keyNum + 1),
            sizeof(Index) * newNode->keyNum);
        newNode->next = key0num; // 新节点的next指针指向key0的data
    } else {
        // 将原节点的一半索引复制到新节点
        memcpy(
            (newNode + 0x1),
            ((Index *) (node + 0x1) + node->keyNum),
            sizeof(Index) * newNode->keyNum);
    }
    save(node, node->myself);
    save(newNode, newNodeNum);

    if (node->layer == (int) meta_.height) {
        // 新生成一个root节点
        char *buf2 = getBuf();
        Node *root = reinterpret_cast<Node *>(buf2);
        int rootNum = allocInterNode(root);
        meta_.root = rootNum;
        meta_.height++;

        root->layer = meta_.height;
        root->myself = rootNum;
        root->prev = -1;
        root->next = nodeNum; // 指向最小的节点

        i = (Index *) (root + 0x1);
        i->key = rootKey0;
        i->blockNum = newNodeNum; // 指向新节点
        printfNode(root);
        save(root, rootNum);
    } else {
        // 更新父节点
        int parentNum = searchIndex(rootKey0, newNode->layer + 1);
        insertParent(parentNum, rootKey0, newNodeNum);
    }
    save_meta(&meta_);
}

// 向父节点插入新增的索引
void Bptree::insertParent(int parentnum, keyType key, int blocknum)
{
    printf("*******************************\n");
    printf("in the insertParent function\n");
    printf("*******************************\n");
    char *buf = getBuf();
    take(buf, parentnum);
    Node *parent = reinterpret_cast<Node *>(buf);
    if (parent->keyNum + 1 <= (int) MAXNUM_KEY) // 合法插入
    {
        insertNoSplit(parent, key, blocknum);
    } else // 需要分裂
    {
        insertWithSplit(parent, key, blocknum);
    }
}

// 更新父节点的索引
void Bptree::updateParent(int parentnum, keyType oldkey, keyType newkey)
{
    printf("in the updateParent function\n");
    char *buf = getBuf();
    Node *parent = reinterpret_cast<Node *>(buf);
    take(buf, parentnum);
    Index *index = reinterpret_cast<Index *>(parent + 0x1);
    int i = 0;
    if (binarySearch(index, parent->keyNum, oldkey, &i) != -1) {
        index[i].key = newkey;
        save(buf, parentnum);
    }
}

int Bptree::removeDirect(Node *leaf, keyType key)
{
    printf("*******************************\n");
    printf("in the removeDirect function\n");
    printf("*******************************\n");
    Index *p = (Index *) (leaf + 0x1);
    int keynum = leaf->keyNum;
    int locate = keynum;
    int result = binarySearch(p, keynum, key, &locate);
    if (result != -1) {
        p += locate; // p定位到该删除的点的位置
        Index *t = (p + 1);
        for (int i = keynum - locate - 1; i > 0; i--) {
            p->key = t->key;
            p->blockNum = t->blockNum;
            p++;
            t++;
        }
        leaf->keyNum--;
        if (locate == 0) {
            if (leaf->layer < (int) meta_.height) {
                p = (Index *) (leaf + 0x1);
                int parentnum = searchIndex(p[0].key, leaf->layer + 1);
                updateParent(
                    parentnum,
                    p->key,
                    key); //最小值更新了应该要更新父节点的索引
            }
        }
    } else
        return -1;
    return 1;
}

// 寻找兄弟节点
// Node *Bptree::findBrother(Node *node)
// 向兄弟节点借一个key
int Bptree::borrowBroKey(Node *node)
{
    Index *p = (Index *) (node + 0x1);
    char *buf = getBuf();
    int parentnum =
        searchIndex(p->key, node->layer + 1); // TODO:这里有getbuf函数
    take(buf, parentnum);
    Node *parent = reinterpret_cast<Node *>(buf);
    int position = parent->keyNum;
    binarySearch(p, parent->keyNum, p->key, &position);
    if (position != 0) {
        int broNum = p[position].blockNum;
        char *buf2 = getBuf();
        take(buf2, broNum);
        Node *bro = reinterpret_cast<Node *>(buf2);
        if (bro->keyNum == (int) MINNUM_KEY) {
            return -1;
        } else {
            p += (bro->keyNum - 1); // 指向要向兄弟节点借的key索引
            insertNoSplit(
                node,
                p->key,
                p->blockNum); // 该函数会对node节点和父节点更新并保存
            removeDirect(bro, p->key);
            save(bro, bro->myself);
            return 1;
        }
    } else {
        return -1;
    }
}

// 合并两个节点
void Bptree::mergeBrother(Node *node)
{
    // char *buf = getBuf();
    // Node *newNode = reinterpret_cast<Node *>(buf);
    // Index *locate = (Index *) (newNode + 0x1);
    // Index *p = (Index *) (bro + 0x1);
    // newNode->keyNum = node->keyNum + bro->keyNum;
    // for (int i = bro->keyNum; i > 0; i--) {
    //     locate->key = p->key;
    //     locate->blockNum = p->blockNum;
    //     locate++;
    //     p++;
    // }
    // p = (Index *) (node + 0x1);
    // for (int i = node->keyNum; i > 0; i--) {
    //     locate->key = p->key;
    //     locate->blockNum = p->blockNum;
    //     locate++;
    //     p++;
    // }
    // save(newNode, newNode->myself);
}

int Bptree::allocInterNode(Node *node)
{
    node->keyNum = 1;
    node->type = interNode;
    meta_.interNodeNum++;
    int slot = meta_.slot;
    meta_.slot++;
    printf("in the allocInterNode function\n");
    return slot;
}

int Bptree::allocLeafNode(Node *node)
{
    node->keyNum = 0;
    node->type = leafNode;
    meta_.leafNodeNum++;
    int slot = meta_.slot;
    meta_.slot++;
    printf("in the allocLeafNode function\n");
    return slot; // 返回该节点自身的偏移量
}
/****************************
 * 函数locate：定位key值对应的区间
 * <key值给出key对应的blocknum
 * 例如: key1=10,key2=20
 *      x<10的查找结果为-1
 *      10<=x<20的查找结果是10对应的blocknum
 *      20<=x的查找结果是20对应的blocknum
 ****************************/
int Bptree::locate(Index index[], int keyNum, int key)
{
    printf("in the locate function\n");
    if (keyNum == 0 || keyNum > (int) MAXNUM_KEY) {
        printf("keynum==%d 超界\n", keyNum);
        return -1;
    }
    int low = 0;
    int high = keyNum - 1;
    int mid;
    if (index[0].key > key) {
        printf("locata返回-1\n");
        return -1;
    }
    if (index[high].key <= key) {
        printf(
            "in the locate fun,index[high] blocknum =%d\n",
            index[high].blockNum);
        return index[high].blockNum;
    }
    while (low < high) {
        mid = (low + high) / 2;
        if (index[mid].key < key)
            low = mid + 1;
        else if (index[mid].key > key)
            high = mid;
        else
            return index[mid].blockNum;
    }
    if (low == high)
        return index[low - 1].blockNum;
    else {
        printf("locata出错\n"); // 这句话应该永远不会打出来
        return -1;
    }
}

// 二分查找，失败返回-1，成功返回key对应的blockNum
int Bptree::binarySearch(Index index[], int keyNum, keyType key, int *i)
{
    printf("in the binarySearch function\n");
    printf("the keyNum is = %d\n", keyNum);
    if (keyNum == 0) return -1;
    int low = 0;
    int high = keyNum - 1;
    int mid;
    if (index[low].key > key || index[high].key < key) // 越界
    {
        printf("二分查找越界,找不到该key值\n");
        return -1;
    }
    while (low <= high) {
        mid = (low + high) / 2;
        if (index[mid].key < key)
            low = mid + 1;
        else if (index[mid].key > key)
            high = mid - 1;
        else {
            if (i != NULL) *i = mid;
            return index[mid].blockNum;
        }
    }
    return -1; // 查找失败，没有该key值
}

// 打印该点
void Bptree::printfNode(Node *node)
{
    printf("\n\n");
    printf("the node->layer =%d\n", node->layer);
    printf("the node->myself =%d\n", node->myself);
    printf("the node->prev =%d\n", node->prev);
    printf("the node->next =%d\n", node->next);
    printf("the node->keyNum =%d\n", node->keyNum);
    Index *p = (Index *) (node + 0x1);
    printf("\n");
    if (node->keyNum <= (int) MAXNUM_KEY) {
        for (int i = node->keyNum; i > 0; i--) {
            printf("the p->key =%d,  data =%d\n", p->key, p->blockNum);
            // printf("the p->data =%d\n", p->blockNum);
            p++;
        }
    }
    printf("\n\n");
}

char *Bptree::getBuf()
{
    static int i = 0;
    buf_[0] = (char *) malloc(blocksize_ + sizeof(Index)); // TODO: 内存释放
    buf_[1] = (char *) malloc(blocksize_ + sizeof(Index));
    buf_[2] = (char *) malloc(blocksize_ + sizeof(Index));
    buf_[3] = (char *) malloc(blocksize_ + sizeof(Index));

    i++;
    if (i == 4) i = 0;
    return buf_[i];
}

void Bptree::printfLeaf()
{
    int firstleaf = meta_.firstLeaf;
    int leafnum = meta_.leafNodeNum;
    int nextNode = firstleaf;

    for (int i = leafnum; i > 0; i--) {
        char *buf = getBuf();
        take(buf, nextNode);
        Node *node = reinterpret_cast<Node *>(buf);
        printfNode(node);
        nextNode = node->next;
    }
}
} // namespace NS_Bptree