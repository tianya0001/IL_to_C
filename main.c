#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

#define INST_NUM        (13)      // 指令数量
#define INST_MAX_LEN    (64)    // 最大指令长度
#define INST_SPLIT_CHAR (" ")   // 指令分割符
#define CMD_MAX_LEN     (32)    // 最大操作码长度
#define PARAM_MAX_NUM   (16)    // 最大参数数量
#define PARAM_MAX_LEN   (32)    // 参数最大长度
#define CHILD_MAX_NUM   (16)    // 最大叶节点数量
#define SRC_FILE_NAME   ("../IL_Test.txt")   // 指令表源码路径

typedef enum {
    IT_Exec = 0,    // 执行指令，一般位于梯形图行的末尾 OUT
    IT_Logic,       // 执行前的逻辑判断 AND、ANDI、OR、ORI
    IT_Logic_Start, // 逻辑判断的开始。 LD、LDI
                    // 如果有对应的结束符，则组成一个逻辑块；
                    // 如果没有对应的结束符，则单独成一条指令
    IT_Logic_End,   // 逻辑判断块的结束。ORB
    IT_Block_Start, // 表示一个程序块开始 MPS、MRD、MPP
    IT_Block_End    // 一个程序块的结束
} InstType_t;

typedef enum {
    OP_None = 0,
    OP_LD,
    OP_LDI,
    OP_AND,
    OP_AND_NOT,
    OP_OR,
    OP_OR_NOT,
    OP_ORB,
    OP_MPS,
    OP_MRD,
    OP_MPP,
    OP_OUT,
    OP_SET,
    OP_RST,
} Opcode_t;

typedef enum {
    LT_AND = 0,
    LT_OR,
    LT_AND_NOT,
    LT_PR_NOT
} LogicType_t;

typedef struct Instruction {
    char NAME[32];
    Opcode_t opcode;
    int param_num;
    InstType_t type;
} Instruction_t;

typedef struct InstructionNode {
    char name[CMD_MAX_LEN];
    Opcode_t opcode;                            // 指令操作码
    InstType_t inst_type;                       // 指令类型
    int param_num;                              // 参数数量
    char params[PARAM_MAX_NUM][PARAM_MAX_LEN];  // 参数
    int Leaf_num;                               // 子节点数量
    struct InstructionNode* Leaf[CHILD_MAX_NUM];
    struct InstructionNode* Parent;
}InstructionNode_t;

// 记录程序分支时的指令节点
typedef struct BranchNode
{
    InstructionNode_t* pInstNode;   // 分支处的指令节点
    struct BranchNode* next;
    struct BranchNode* prev;
} BranchNode_t;

BranchNode_t MPS_list_head;
BranchNode_t* curr_MPS_node = &MPS_list_head;
BranchNode_t logic_list_head;
BranchNode_t* curr_logic_node = &logic_list_head;

Instruction_t ALL_INSTRUCTIONS[INST_NUM] = {
    {INS_OP_LD,         OP_LD,      1,  IT_Logic_Start},
    {INS_OP_LDI,        OP_LDI,     1,  IT_Logic_Start},
    {INS_OP_AND,        OP_AND,     1,  IT_Logic},
    {INS_OP_AND_NOT,    OP_AND_NOT, 1,  IT_Logic},
    {INS_OP_OR,         OP_OR,      1,  IT_Logic},
    {INS_OP_OR_NOT,     OP_OR_NOT,  1,  IT_Logic},
    {INS_OP_ORB,        OP_ORB,     0,  IT_Logic_End},
    {INS_OP_MPS,        OP_MPS,     0,  IT_Block_Start},
    {INS_OP_MRD,        OP_MRD,     0,  IT_Block_Start},
    {INS_OP_MPP,        OP_MPP,     0,  IT_Block_End},
    {INS_OP_OUT,        OP_OUT,     1,  IT_Exec},
    {INS_OP_SET,        OP_SET,     1,  IT_Exec},
    {INS_OP_RST,        OP_RST,     1,  IT_Exec}
};


void generate_inst_tree(FILE* src_file, InstructionNode_t* tree_head);
void inst_detect(char* inst, InstructionNode_t* node);
void int_adjust(InstructionNode_t** ppParentNode, InstructionNode_t* pLeafNode);

int main(void)
{
    InstructionNode_t host_node = {0};
    FILE* src_file = fopen(SRC_FILE_NAME, "r");
    if (src_file == NULL) {
        return -1;
    }

    generate_inst_tree(src_file, &host_node);

    fclose(src_file);

    return 0;
}

/**
 * @brief 把字符串中的换行符替换位'\0'
 * 
 * @param str 需要处理的字符串
 */
static void inst_delete_last_char(char* str)
{
    for (int i = 0; i < INST_MAX_LEN; i++)
    {
        if(str[i] == '\n')
        {
            str[i] = '\0';
        }

        if(str[i] == '\t')
        {
            str[i] = ' ';
        }
    }
}

/**
 * @brief 生成语指令树
 * 
 */
void generate_inst_tree(FILE* src_file, InstructionNode_t* tree_head)
{
    char inst[INST_MAX_LEN];
    char cmd[CMD_MAX_LEN];
    char inst_param[PARAM_MAX_LEN];
    int param_num = 0;
    char* token = NULL;
    InstructionNode_t* curr_node = tree_head;

    while (NULL != fgets(inst, INST_MAX_LEN, src_file))
    {
        int Leaf_idx = curr_node->Leaf_num;
        inst_delete_last_char(inst);
        // printf("%s\n", inst);

        // 创建指令节点
        InstructionNode_t* leaf_inst_node = malloc(sizeof(InstructionNode_t));
        if(leaf_inst_node == NULL)    return;
        memset(leaf_inst_node, 0, sizeof(InstructionNode_t));
        leaf_inst_node->Leaf_num = 0;

        // 指令分割
        token = strtok(inst, INST_SPLIT_CHAR);
        strcpy(cmd, token);
        
        // 指令识别
        inst_detect(cmd, leaf_inst_node);
        
        // 填充参数
        for(int i = 0; i < leaf_inst_node->param_num; i++)
        {
            token = strtok(NULL, INST_SPLIT_CHAR);
            strcpy(leaf_inst_node->params[i], token);
        }

        int_adjust(&curr_node, leaf_inst_node);


        printf("----------------------------------------------------------------\n");
    }

    printf("-------- OVER --------\n");
}

/**
 * @brief 识别指令，填入指令码、参数数量
 * 
 * @param inst 指令码
 * @param node 指令节点
 */
void inst_detect(char* inst, InstructionNode_t* node)
{
    for(int i = 0; i < INST_NUM; i++)
    {
        if(strcmp(inst, ALL_INSTRUCTIONS[i].NAME) == 0)
        {
            
            strcpy(node->name, inst);
            node->opcode = ALL_INSTRUCTIONS[i].opcode;
            node->param_num = ALL_INSTRUCTIONS[i].param_num;
            node->inst_type = ALL_INSTRUCTIONS[i].type;
        }
    }
    printf("CMD: %s, CODE: %d, PARAM NUM: %d\n", inst, node->opcode, node->param_num);
}

/**
 * @brief 指令关系处理，处理分支、逻辑块、
 * 
 * @param ParentNode 
 * @param LeafNode 
 */
void int_adjust(InstructionNode_t** ppParentNode, InstructionNode_t* pLeafNode)
{
    InstructionNode_t* pParentNode = *ppParentNode;
    int leaf_idx = pParentNode->Leaf_num;
    if(pLeafNode->inst_type == IT_Exec)
    {
        // 连接节点
        pParentNode->Leaf[leaf_idx] = pLeafNode;
        pParentNode->Leaf_num++;
        pLeafNode->Parent = pParentNode;


    }
    else if(pLeafNode->inst_type == IT_Logic)
    {
        pParentNode->Leaf[leaf_idx] = pLeafNode;
        pParentNode->Leaf_num++;
        pLeafNode->Parent = pParentNode;
        *ppParentNode = pLeafNode;
    }
    else if(pLeafNode->inst_type == IT_Logic_Start)
    {
        pParentNode->Leaf[leaf_idx] = pLeafNode;
        pParentNode->Leaf_num++;
        pLeafNode->Parent = pParentNode;
        *ppParentNode = pLeafNode;
    }
    else if(pLeafNode->inst_type == IT_Logic_End)
    {
        pParentNode->Leaf[leaf_idx] = pLeafNode;
        pParentNode->Leaf_num++;
        pLeafNode->Parent = pParentNode;
        *ppParentNode = pLeafNode;
    }
    else if(pLeafNode->inst_type == IT_Block_Start)
    {
        if(pLeafNode->opcode == OP_MPS)
        {
            pParentNode->Leaf[leaf_idx] = pLeafNode;
            pParentNode->Leaf_num++;
            pLeafNode->Parent = pParentNode;
            *ppParentNode = pLeafNode;

            BranchNode_t* next_MPS_node = malloc(sizeof(BranchNode_t));
            memset(next_MPS_node, 0, sizeof(BranchNode_t));

            curr_MPS_node->next = next_MPS_node;
            next_MPS_node->prev = curr_MPS_node;
            next_MPS_node->pInstNode = pLeafNode;
            curr_MPS_node = next_MPS_node;
        }

        if(pLeafNode->opcode == OP_MRD)
        {
            // Parent节点指向分支
            *ppParentNode = curr_MPS_node->pInstNode->Parent;
            pParentNode = *ppParentNode;

            // 在分支处添加子节点
            leaf_idx = pParentNode->Leaf_num;
            pParentNode->Leaf[leaf_idx] = pLeafNode;
            pParentNode->Leaf_num++;
            pLeafNode->Parent = pParentNode;
            *ppParentNode = pLeafNode;
        }
    }
    else if(pLeafNode->inst_type == IT_Block_End)
    {
        // Parent节点指向分支
        *ppParentNode = curr_MPS_node->pInstNode->Parent;
        pParentNode = *ppParentNode;

        // 在分支处添加子节点
        leaf_idx = pParentNode->Leaf_num;
        pParentNode->Leaf[leaf_idx] = pLeafNode;
        pParentNode->Leaf_num++;
        pLeafNode->Parent = pParentNode;
        *ppParentNode = pLeafNode;

        // 释放节点表
        curr_MPS_node = curr_MPS_node->prev;
        free(curr_MPS_node->next);
    }
    else
    {
        printf("Error\n");
    }
}