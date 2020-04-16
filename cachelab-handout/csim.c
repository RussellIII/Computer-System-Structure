#include "cachelab.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
struct Row {
	int valid;
	long long tag;
	struct Row* next;
};
void DisplayTrace(char operation,long long address,int bytenum,int hit,int miss,int evit) {
	printf("%c %llx,%d",operation,address,bytenum);
	if (miss == 1) {
		printf(" miss");
	}
	if (evit == 1) {
		printf(" eviction");
	}
	if (operation == 'M' && hit ==1 && miss == 0 && evit == 0) {
		printf(" hit hit");
	} else if (hit == 1) {
		printf(" hit");
	}
	printf("\n");
}
void DisplayHelpInfo(void) {
	printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\nOptions:");
	printf("\t-h\tPrint this help message.\n");
	printf("\t-v\tOptional verbose flag.\n");
	printf("\t-s <num>\tNumber of set index bits.\n");
	printf("\t-E <num>\tNumber of lines per set.\n");
	printf("\t-b <num>\tNumber of block offset bits.\n");
	printf("\t-t <file>\tTrace file.\n");
	printf("Examples:\n");
	printf("  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n");
	printf("  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}
int main(int argc, char** argv)
{
	int DisplayTraceInfo = 0; //需要显示轨迹信息
	int IndexDigits = 0; // 组索引的位数,共有2^n组
	int relevance = 0; //关联度，每组包含的缓存行数
	int offsetbits = 0; //offset
	char *FileName;
	int hit_count = 0;
	int miss_count = 0;
	int eviction_count = 0;
	FILE *fp = NULL;
	char buffer[100];

	// 读取命令行传入参数
	if (argv[1][1] == 'h') {
		DisplayHelpInfo();
		return 0;
	』
	if (argc == 10) {
		if(argv[1][1] == 'v'){
			DisplayTraceInfo = 1;
		}
		IndexDigits = atoi(argv[3]);
		relevance = atoi(argv[5]);
		offsetbits = atoi(argv[7]);
		FileName = argv[9];
	} else {
		IndexDigits = atoi(argv[2]);
		relevance = atoi(argv[4]);
		offsetbits = atoi(argv[6]);
		FileName = argv[8];
	}

	int groupnum = 1 << IndexDigits; //一共有几组
	struct Row *group[groupnum];
	
	//把每一组形成一个串
	for (int i = 0; i < groupnum; i ++) {
		if (relevance == 1) {
			group[i] = (struct Row*)malloc(sizeof(struct Row));
			group[i]->next = NULL;
			group[i]->valid = 0;
			group[i]->tag = 0;
		} else {
			group[i] = (struct Row*)malloc(sizeof(struct Row));
			struct Row* rowp = group[i];
			for (int j = 0; j < relevance - 1; j ++) {
				rowp->next = (struct Row*)malloc(sizeof(struct Row));
				rowp->next->valid = 0;
				rowp->next->tag = 0;
				rowp = rowp->next; 
			}
			rowp->next = NULL; 
		}
	}

	fp = fopen(FileName, "r");
	char operation = 'I';
	long long address;
	int bytenum;
	if (fp == NULL) {
		printf("ERROR: Failed to open file.\n");
		return 0;
	}
	fgets(buffer, sizeof(buffer), fp);
	while (!feof(fp)) {
		// printf("%s", buffer);//需要被注释
		//do nothing if the operation is I(load)
		if (buffer[0] == 'I') {
			fgets(buffer, sizeof(buffer), fp);
			continue;
		} else {
			operation = buffer[1];//read the operation
		}
		// printf("operation:%c ", operation);//需要被注释

		//read the address
		int i = 3;
		address = 0;
		while (buffer[i] != ',') {
			if (buffer[i] >= '0' && buffer[i] <= '9') {
				address = address * 16 + buffer[i] - '0';
			}
			if (buffer[i] >= 'a' && buffer[i] <= 'f') {
				address = address * 16 + buffer[i] - 'a' + 10;
			}
			i ++;
		}
		// printf("address:ox%llx\n",address);//需要被注释

		//read the bytenum
		i ++;
		bytenum = 0;
		while (buffer[i] != '\n') {
			bytenum = bytenum * 10 + buffer[i] - '0';
			i ++;
		}
		// printf("字节数:%d\n",bytenum);//需要被注释
		//当前状态：创建好了cache，每一组是一串链表
		//把操作、地址 和 字节数读入operation, address, bytenum

		//Divide address into three part: 
		//'Intag' is used as a tag to find the right row
		//'Inoffset' is used as an offset to find the right data in a certain row
		//'Inindex' is used to find the right group
		long long Intag = (address & (0xFFFFFFFFFFFFFFFF << (IndexDigits + offsetbits))) >> (IndexDigits + offsetbits);
		int Inindex = (address & (((0xFFFFFFFFFFFFFFFF >> offsetbits) << (64 - IndexDigits)) >> (64 - IndexDigits - offsetbits))) >> offsetbits;
		// printf("Tag = %llx, Index = %x\n", Intag, Inindex);//需要被注释
		int Lhit = 0;
		int Lmiss = 0;
		int Levit = 0;
		int mtimes = 1;
		struct Row* firstemptyrow = NULL;
		struct Row* beforeFER = group[Inindex];
		struct Row* lastrowingroup = NULL;
		struct Row* beforeLRIG = group[Inindex];
		struct Row* prerowp = NULL;
Magain:	if (operation == 'L' || operation == 'S' || operation == 'M') {
			//先遍历链表，查找valid = 1 且 tag一致 的行，并记录尾节点和第一个空白的位置
			//若找到，则hit数+1
			//若没找到，则查看第一个空白的行，若有空白的行，则写入，并加入列表最前
			//若没有空白的行，则把最后一个
			firstemptyrow = NULL;
			beforeFER = group[Inindex];
			lastrowingroup = NULL;
			beforeLRIG = group[Inindex];
			prerowp = NULL;
			struct Row* rowp = group[Inindex];
			prerowp = group[Inindex];
			while (1) {
				if (rowp->valid == 0 && firstemptyrow == NULL) {
					firstemptyrow = rowp;
					beforeFER = prerowp; 
				}
				if (rowp->valid == 1 && rowp->tag == Intag) {
					Lhit = 1;
					break;
				}
				if (rowp->next == NULL) {
					beforeLRIG = prerowp;
					lastrowingroup = rowp;
					Lmiss = 1;
					miss_count ++;
					break;
				}
				prerowp = rowp;
				rowp = rowp->next;
			}

			// printf("hit=%d,miss=%d,evict=%d\n",Lhit,Lmiss,Levit);//need to be commented
			//当空row是第一个row时，beforeFER = NULL
			if (Lhit) {
				hit_count ++;
				if (prerowp != rowp) {
					prerowp->next = rowp->next;
					rowp->next = group[Inindex];
					group[Inindex] = rowp;
				}
				if (DisplayTraceInfo && operation != 'M') {
					DisplayTrace(operation,address,bytenum,Lhit,Lmiss,Levit);
				}
				if (DisplayTraceInfo && operation == 'M' && mtimes == 2) {
					DisplayTrace(operation,address,bytenum,Lhit,Lmiss,Levit);
				}
				if (operation == 'M' && mtimes == 1) {
					mtimes ++;
					goto Magain;
				}
				fgets(buffer, sizeof(buffer), fp);
				continue;
			}
			// printf("%s",buffer);
			if (firstemptyrow == NULL) {
				//when all rows in this group are occupied
				Levit = 1;
				eviction_count ++;
				lastrowingroup->tag = Intag;
				lastrowingroup->next = group[Inindex];
				beforeLRIG->next = NULL;
				group[Inindex] = lastrowingroup;
			} else {
				//write info into this empty row and put it into the first place
				firstemptyrow->valid = 1;
				firstemptyrow->tag = Intag;
				if (beforeFER != firstemptyrow) {
					beforeFER->next = firstemptyrow->next;
					firstemptyrow->next = group[Inindex];
					group[Inindex] = firstemptyrow;
				}
			}
			
			if (operation == 'M' && mtimes == 1){
				mtimes ++;
				goto Magain;
			}
			if (DisplayTraceInfo == 1 && operation != 'M') {
				DisplayTrace(operation,address,bytenum,Lhit,Lmiss,Levit);
			}
			// struct Row* checkp = group[Inindex];
			// printf("\tIn group %d:", Inindex);
			// while(checkp != NULL) {
			// 	printf("\tv=%d tag=%llx",checkp->valid, checkp->tag);
			// 	checkp = checkp->next;
			// }
			// printf("\n");
		}
		fgets(buffer, sizeof(buffer), fp);
	}

	// printf("%d %d %d %d %d\n",DisplayHelpInfo,DisplayTraceInfo,IndexDigits,relevance,offsetbits);
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
