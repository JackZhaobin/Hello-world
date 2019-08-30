#ifndef _HZHY_SGI_H_
#define _HZHY_SGI_H_

#include <linux/skbuff.h>
#include <linux/spinlock.h>

#define		CONFIG_HZHY_SGI

#define		HZHY_SGI_INT_NUM			15		//双核交互中断号

#define		HZHY_GOOSE_TYPE_POS			16		//GOOSE特征字位置
#define		HZHY_GOOSE_TYPE_VALUE		0x88B8	//GOOSE特征字

//双核共享内存具体定义
#define		SM_DUAL_EACH_BLOCK_SIZE		4096
#define		SM_DUAL_TOTAL_BLOCK_NUM		1024

//GOOSE缓冲内存具体定义
#define		SM_GOOSE_EACH_BLOCK_SIZE	2048
#define		SM_GOOSE_TOTAL_BLOCK_NUM	64//16

//SNTP缓存宏
#define		SM_SNTP_EACH_BLOCK_SIZE		1024
#define		SM_SNTP_TOTAL_BLOCK_NUM		2


//GOOSE是否发生标志宏
//#define		NO_GOOSE_OCCUR				0x00
//#define		GOOSE_OCCUR					0x01

extern struct sk_buff *Goose_skb[SM_GOOSE_TOTAL_BLOCK_NUM],Sntp_skb;
extern struct sk_buff *test_skb;


//共享内存结构体
struct HZHY_SHARED_MEM {
	char *base;
	//char flag;						//特殊场合可能用到
	//spinlock_t w_lock;
	unsigned int w_ptr;				//写指针，指向具体写哪块共享内存
	unsigned int r_ptr;
	unsigned int unread_num;		//当前未处理的块数
	unsigned int each_block_size;	//每块共享内存大小，单位：字节
	unsigned int total_block_num;	//共享内存块数
};

extern struct HZHY_SHARED_MEM dual_core_mem;
extern struct HZHY_SHARED_MEM goose_mem;
extern struct HZHY_SHARED_MEM sntp_mem;

extern void handle_hzhy_SGI(int ipinr, void *regs);
extern int refresh_w_ptr(struct HZHY_SHARED_MEM *sm);
extern int hzhy_xemacps_send_data(struct sk_buff *skb);
#endif
