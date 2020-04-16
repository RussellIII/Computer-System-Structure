#include "cachelab.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
struct Row {
	int valid;
	long long tag;
	struct Row* next;
};
int hit_count;
int miss_count;
int eviction_count;
int hit;
int miss;
int evit;
void DisplayTrace(char operation,int hit,int miss,int evit) {
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
void CreateCache(struct Row** group, int groupnum, int relevance) {
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
}
void UpdateCache(struct Row** group, int Inindex, long long Intag, char operation, int mtimes, int PrintTrace) {
	struct Row* firstemptyrow = NULL;
	struct Row* beforeFER = group[Inindex];
	struct Row* lastrowingroup = NULL;
	struct Row* beforeLRIG = group[Inindex];
	struct Row* rowp = group[Inindex];
	struct Row* prerowp = group[Inindex];
	while (1) {
		if (rowp->valid == 0 && firstemptyrow == NULL) {
			firstemptyrow = rowp;
			beforeFER = prerowp; 
		}
		if (rowp->valid == 1 && rowp->tag == Intag) {
			hit = 1;
			break;
		}
		if (rowp->next == NULL) {
			beforeLRIG = prerowp;
			lastrowingroup = rowp;
			miss = 1;
			miss_count ++;
			break;
		}
		prerowp = rowp;
		rowp = rowp->next;
	}
	if (hit) {
		hit_count ++;
		if (prerowp != rowp) {
			prerowp->next = rowp->next;
			rowp->next = group[Inindex];
			group[Inindex] = rowp;
		}
		if (PrintTrace && operation != 'M') {
			DisplayTrace(operation,hit,miss,evit);
		}
		if (PrintTrace && operation == 'M' && mtimes == 2) {
			DisplayTrace(operation,hit,miss,evit);
		}
		return;
	}
	// printf("%s",buffer);
	if (firstemptyrow == NULL) {
		//when all rows in this group are occupied
		evit = 1;
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
	if (PrintTrace == 1 && operation != 'M') {
		DisplayTrace(operation,hit,miss,evit);
	}
}
int main(int argc, char** argv)
{
	int PrintTrace = 0; //需要显示轨迹信息
	int indexbits = 0; // 组索引的位数,共有2^n组
	int relevance = 0; //关联度，每组包含的缓存行数
	int offsetbits = 0; //offset
	char *FileName;
	hit_count = 0;
	miss_count = 0;
	eviction_count = 0;

	// 读取命令行传入参数
	if (argv[1][1] == 'h') {
		DisplayHelpInfo();
		return 0;
	}
	if (argc == 10) {
		if(argv[1][1] == 'v'){
			PrintTrace = 1;
		}
		indexbits = atoi(argv[3]);
		relevance = atoi(argv[5]);
		offsetbits = atoi(argv[7]);
		FileName = argv[9];
	} else {
		indexbits = atoi(argv[2]);
		relevance = atoi(argv[4]);
		offsetbits = atoi(argv[6]);
		FileName = argv[8];
	}

	int groupnum = 1 << indexbits; //一共有几组
	struct Row *group[groupnum];
	//根据组数、相连度创建Cache，仿真中不需要设置具体数据的存储
	CreateCache(group, groupnum, relevance);

	FILE *fp = NULL;
	if ((fp = fopen(FileName, "r")) == NULL) {
		printf("ERROR: Failed to open file.\n");
		return 0;
	}
	char operation;
	long long unsigned address;
	int bytenum;
	while (fscanf(fp, "%s %llx,%d", &operation, &address, &bytenum) != EOF) {
		//do nothing if the operation is I(load)
		if (operation == 'I') {
			continue;
		}
		printf("%c %llx,%d",operation,address,bytenum);
		//Divide address into three part: 
		//'Intag' is used as a tag to find the right row
		//'Inoffset' is used as an offset to find the right data in a certain row
		//'Inindex' is used to find the right group
		long long Intag = (address & (0xFFFFFFFFFFFFFFFF << (indexbits + offsetbits)))\
			>> (indexbits + offsetbits);
		int Inindex = (address & (((0xFFFFFFFFFFFFFFFF >> offsetbits) << \
			(64 - indexbits)) >> (64 - indexbits - offsetbits))) >> offsetbits;

		if (operation == 'L' || operation == 'S') {
			hit = 0;
			miss = 0;
			evit = 0;
			UpdateCache(group, Inindex, Intag, operation, 1, PrintTrace);
		} else {
			hit = 0;
			miss = 0;
			evit = 0;
			UpdateCache(group, Inindex, Intag, operation, 1, PrintTrace);
			UpdateCache(group, Inindex, Intag, operation, 2, PrintTrace);
		}
	}
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}