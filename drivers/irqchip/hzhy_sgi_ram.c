
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h> //cdev
#include <linux/fs.h> //file_operations
#include <linux/uaccess.h> //copy_* 
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/bcd.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/poll.h>

#include <linux/hzhy_sgi.h>


#define 	DRIVER_NAME 					"hzhy_sharemem"

#define		IOCTL_IS_SGI_OCCUR				0x01		//���жϷ���
/*
#define		IOCTL_DUAL_GET_EACH_BLOCK_SIZE	0x11		
#define		IOCTL_DUAL_GET_UNREAD_NUM		0x12		//��ȡ��ǰδ��������
#define		IOCTL_DUAL_GET_R_OFFSET			0x13		//��ȡ��ƫ��(���Ե�ַ=����ַ+ƫ��)
*/
#define		IOCTL_DUAL_GET_UNREAD_INFO		0x12		//��ȡ��ǰδ��������������+��ƫ�Ƶ�ַ

//#define		IOCTL_IS_GOOSE_OCCUR			0x21
#define		IOCTL_GOOSE_GET_UNREAD_INFO		0x22		//��ȡGOOSE��ǰδ��������

//�����ڴ�ƫ���������Ե�ַ���豸����ָ����
#define		SM_DUAL_BASE					0x00000000
#define		SM_DUAL_EACH_SIZE				SM_DUAL_EACH_BLOCK_SIZE	//ÿƬ��С
#define		SM_DUAL_NUM						SM_DUAL_TOTAL_BLOCK_NUM		//��Ƭ��
#define		SM_DUAL_END						(SM_DUAL_BASE+SM_DUAL_EACH_SIZE*SM_DUAL_NUM)

#define		SM_GOOSE_BASE					SM_DUAL_END
#define		SM_GOOSE_EACH_SIZE				SM_GOOSE_EACH_BLOCK_SIZE	//ÿƬ��С
#define		SM_GOOSE_NUM					SM_GOOSE_TOTAL_BLOCK_NUM			//��Ƭ��
#define		SM_GOOSE_END					(SM_GOOSE_BASE+SM_GOOSE_EACH_SIZE*SM_GOOSE_NUM)

#define		SM_SNTP_BASE					SM_GOOSE_END
#define		SM_SNTP_EACH_SIZE				SM_SNTP_EACH_BLOCK_SIZE
#define		SM_SNTP_NUM						SM_SNTP_TOTAL_BLOCK_NUM
#define		SM_SNTP_END						(SM_SNTP_BASE+SM_SNTP_EACH_SIZE*SM_SNTP_NUM)

static dev_t devno;
static struct class *class;

struct sgi_struct {
	void __iomem *base;
	//char buf[4096];
};

//Ӧ�ò����������轻������Ϣ
struct SHAREDMEM_EXCHANGE {
	unsigned int unread_num;
	unsigned int r_ptr;
};

struct HZHY_SHARED_MEM dual_core_mem =	//˫�˽��������ڴ�
{
	NULL,
	0,
	0,
	0,
	SM_DUAL_EACH_BLOCK_SIZE,
	SM_DUAL_TOTAL_BLOCK_NUM
};

struct HZHY_SHARED_MEM goose_mem =		//GOOSE����
{
	NULL,
	0,
	0,
	0,
	SM_GOOSE_EACH_BLOCK_SIZE,
	SM_GOOSE_TOTAL_BLOCK_NUM
};

struct HZHY_SHARED_MEM sntp_mem =		//SNTP����
{
	NULL,
	0,
	0,
	0,
	SM_SNTP_EACH_BLOCK_SIZE,
	SM_SNTP_TOTAL_BLOCK_NUM
};

struct sk_buff *Goose_skb[SM_GOOSE_TOTAL_BLOCK_NUM],Sntp_skb;
struct sk_buff *test_skb;

struct sgi_struct *my_sgi;

static DECLARE_WAIT_QUEUE_HEAD(hzhy_sgi_waitq);

/* �ж��¼���־, �жϷ����������1����Ӧ����������0 */
static volatile int ev_sgi = 0;

static struct fasync_struct *sgi_async;

//�����ڴ��ʼ��
//�ú���������������ε�����ֹ�ں��ѽ���n������Ϣ���������ʼ�������
static void init_sharedmem(void)
{
	struct HZHY_SHARED_MEM *sm;
	
	sm = &dual_core_mem;
	memset((char *)sm,0,sizeof(struct HZHY_SHARED_MEM));
	sm->base = my_sgi->base + SM_DUAL_BASE;
	sm->each_block_size = SM_DUAL_EACH_BLOCK_SIZE;
	sm->total_block_num = SM_DUAL_TOTAL_BLOCK_NUM;
	
	sm = &goose_mem;
	memset((char *)sm,0,sizeof(struct HZHY_SHARED_MEM));
	sm->base = my_sgi->base + SM_GOOSE_BASE;
	sm->each_block_size = SM_GOOSE_EACH_BLOCK_SIZE;
	sm->total_block_num = SM_GOOSE_TOTAL_BLOCK_NUM;
	
	sm = &sntp_mem;
	memset((char *)sm,0,sizeof(struct HZHY_SHARED_MEM));
	sm->base = my_sgi->base + SM_SNTP_BASE;
	sm->each_block_size = SM_SNTP_EACH_BLOCK_SIZE;
	sm->total_block_num = SM_SNTP_TOTAL_BLOCK_NUM;
	
}

//дָ������һ
//ע��Ӧ�ȵ��øú�������ִ�о���д����
//�״�д��ַΪ1����0��
int refresh_w_ptr(struct HZHY_SHARED_MEM *sm)
{
	static int i=0;
	
	if (sm->total_block_num == 0)
		return -1;
	if (((sm->w_ptr+1)%sm->total_block_num) == sm->r_ptr)	//дָ��׷�϶�ָ�룬����������
	{
		if (i == 0)
		{
			i = 1;
			printk("myKernel Err!WBuff is Full!w%d r%d\n",sm->w_ptr,sm->r_ptr);
		}
		return -2;
	}

	//spin_lock_irq(&sm->w_lock);
	sm->w_ptr++;
	if (sm->w_ptr >= sm->total_block_num)
		sm->w_ptr = 0;
	//spin_unlock_irq(&sm->w_lock);
	
	return 0;
}

//��ָ������һ
//ע��Ӧ�ȵ��øú�������ִ�о��������
//�״ε�ַΪ1����0��
static int refresh_r_ptr(struct HZHY_SHARED_MEM *sm)
{
	if (sm->total_block_num == 0)
	{
		//printk("Kernel r Err!\n");
		return -1;
	}
	if (sm->r_ptr == sm->w_ptr)	//��δ��
	{
		//printk("Kernel r Err %d!\n",sm->r_ptr);
		//return -2;
		return 0;
	}
	sm->r_ptr++;
	if (sm->r_ptr >= sm->total_block_num)
		sm->r_ptr = 0;
	
	return 0;
}

//����Ŀǰδ����Ŀ���
static int refresh_undo_num(struct HZHY_SHARED_MEM *sm)
{
	if (sm->total_block_num == 0)
		return -1;
	
	sm->unread_num = (sm->w_ptr + sm->total_block_num - sm->r_ptr) % sm->total_block_num;
	return 0;
}

//��ȡ��ǰδ����Ŀ���
//�ú���Ӧ��ˢ�¶�ָ��ǰ���ã���ֹ������ǰ��δ������Ϣor�����걾����Ϣ������δ������Ϣ
static int get_undo_num(struct HZHY_SHARED_MEM *sm)
{
	int ret;
	
	ret = refresh_undo_num(sm);
	if (ret < 0)
		return ret;
	return sm->unread_num;
}


static int dispose_dual_mem(void)
{
	struct HZHY_SHARED_MEM *sm;
	
	sm = &dual_core_mem;
	refresh_w_ptr(sm);				//ע�⣺˫�˽������޷���ͨ��Ϣ����д���ڳ�������
/*	
	if (sm->w_ptr < 10)
		printk("dualW%d,R%d\n",sm->w_ptr,sm->r_ptr);
*/	
	return 0;
}

static int dispose_goose(void)
{
	static int cnt=0;
	int ret;
	struct HZHY_SHARED_MEM *sm;
	
	sm = &goose_mem;
	ret = get_undo_num(sm);
	if (ret <= 0)				//�������账��
		return ret;
	ret = refresh_r_ptr(sm);	//��ȡ��ǰ��ָ��ƫ��
	if (ret < 0)
		return ret;
	if (cnt++ < 32)
	ret = 0;//hzhy_xemacps_send_data(Goose_skb[sm->r_ptr]);	//ֱ��ת��
	else if (cnt > 128)
		cnt = 0;
	printk("Kgoose %d!\n",sm->r_ptr);
	
	//dev_kfree_skb(Goose_skb[sm->r_ptr]);
	
	/*
	ret = hzhy_xemacps_send_data(test_skb);	//ֱ��ת��
	//printk("user=%d\n",Goose_skb[sm->r_ptr]->users);
	//dev_kfree_skb(test_skb);
	*/
	return ret;
}

static int dispose_sntp(void)
{
	int ret,len=0;
	struct HZHY_SHARED_MEM *sm;
	
	sm = &sntp_mem;
	ret = get_undo_num(sm);
	if (ret <= 0)				//�������账��
		return ret;
	ret = refresh_r_ptr(sm);	//��ȡ��ǰ��ָ��ƫ��
	if (ret < 0)
		return ret;
	if (sm->base != NULL)
		len = ioread32(sm->base + sm->r_ptr*sm->each_block_size);

	printk("sntp %d!len=%d\n",sm->r_ptr,len);
	return 0;
}
/*
*	Zynq���ж���Ӧ����
*/
void handle_hzhy_SGI(int ipinr, void *regs)
{
	//static int cnt=0;
	
	if (ipinr != HZHY_SGI_INT_NUM)
		return;	

	//if (cnt++ < 10)
	//	printk("sgi%02d\n",cnt);
	dispose_dual_mem();
	dispose_goose();
	dispose_sntp();
	
	ev_sgi = 1;
	wake_up_interruptible(&hzhy_sgi_waitq); // �������ߵĽ���
}



static int hzhy_sgi_open(struct inode *inode, struct file *file)
{
	//printk("hzhy sgi open!\n");
	init_sharedmem();
	return 0;
}
/*
static unsigned int my_read32(unsigned int addr)
{
	unsigned int val;
	
	addr &= ~0x03;	//���ֶ���

	val = ioread32(my_sgi->base + addr);
	return val;
}

static unsigned int my_readN32(unsigned int addr, char *buf, unsigned int len)
{
	unsigned int i=0,val;
	
	//printk("R 0x%02x,len=%d\n",(unsigned int)my_sgi->base+addr,len);
	for (i=0; i<len; i+=4)
	{
		val = my_read32(addr+i);
		buf[i] = val & 0xFF;
		buf[i+1] = (val>>8) & 0xFF;
		buf[i+2] = (val>>16) & 0xFF;
		buf[i+3] = (val>>24) & 0xFF;
	}
	return len;
}
*/
ssize_t hzhy_sgi_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	/*int ret,num,addr;
	
//	printk("hzhy sgi read\n");
	
	addr = 0;
	num = my_readN32(addr,my_sgi->buf,size);
	if (num != size)
	{
		printk("Kernel read Error!\n");
		return -3;
	}
	
	ret = copy_to_user(buf, my_sgi->buf,size);
	if (ret < 0)
		return ret;
	
	return num;*/
	return 0;
}


int hzhy_sgi_close(struct inode *inode, struct file *file)
{
	//printk("hzhy sgi close!\n");
	return 0;
}

static unsigned hzhy_sgi_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	poll_wait(file, &hzhy_sgi_waitq, wait); // ������������

	if (ev_sgi)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int hzhy_sgi_fasync (int fd, struct file *filp, int on)
{
	printk("driver: hzhy_sgi_fasync\n");
	//��ʼ��/�ͷ� fasync_struct �ṹ�� (fasync_struct->fa_file->f_owner->pid)
	return fasync_helper (fd, filp, on, &sgi_async);
}


static long hzhy_sgi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret=0;
	unsigned int tmp;
	struct HZHY_SHARED_MEM *sm;
	struct SHAREDMEM_EXCHANGE exch;
	
	/* ����������Ч�� */
/*    if (_IOC_TYPE(cmd) != HZHY_AXDX_MAGIC) 
        return -EINVAL;
	*/
	switch (cmd)
	{
		case IOCTL_IS_SGI_OCCUR:
			wait_event_interruptible(hzhy_sgi_waitq, ev_sgi);
			ev_sgi = 0;
			tmp = 1;
			ret = __put_user(tmp, (int *)arg);
			break;
		case IOCTL_DUAL_GET_UNREAD_INFO:
			sm = &dual_core_mem;
			exch.unread_num = get_undo_num(sm);
			ret = refresh_r_ptr(sm);		//��ȡ��ǰ��ָ��ƫ��
			if (ret < 0)
			{
				printk("refresh_r_ptr Err!%d\n",ret);
				return ret;
			}
			//buf[1] = sm->r_ptr*sm->each_block_size;
			exch.r_ptr = sm->r_ptr;
			
			ret = copy_to_user((int *)arg, (char *)&exch, sizeof(struct SHAREDMEM_EXCHANGE));
			break;
			/*
		case IOCTL_IS_GOOSE_OCCUR:
			sm = &goose_mem;
			tmp = sm->flag;
			ret = __put_user(tmp, (int *)arg);
			break;
			*/
		case IOCTL_GOOSE_GET_UNREAD_INFO:
			sm = &goose_mem;
			ret = refresh_r_ptr(sm);		//��ȡ��ǰ��ָ��ƫ��
			if (ret < 0)
				return ret;
			exch.r_ptr = sm->r_ptr;
			exch.unread_num = get_undo_num(sm);
			
			ret = copy_to_user((int *)arg, (char *)&exch, sizeof(struct SHAREDMEM_EXCHANGE));
			if (ret < 0)
				return ret;
			break;
		default:
			printk("Kernel invalid ioctl cmd %02X!\n",cmd);
			return -1;
	}

	return ret;
}


static struct file_operations hzhy_sgi_fops = {
	.owner =THIS_MODULE,/* ����һ���꣬�������ģ��ʱ�Զ�������__this_module���� */
	.open=hzhy_sgi_open, 
	.read=hzhy_sgi_read,
	.release =hzhy_sgi_close,
	.poll=hzhy_sgi_poll,
	.fasync=hzhy_sgi_fasync,
	.unlocked_ioctl = hzhy_sgi_ioctl,
};

static int my_char_register(void)
{
	int ret = 0;
	static struct cdev *cdev;
	
	ret = alloc_chrdev_region(&devno, 0, 1, DRIVER_NAME);
	if(ret < 0)	{
		printk(KERN_ALERT"Error: Can not register device number\n");
		return ret;
	}
	
	//printk(KERN_ALERT"Major:%d  Minor:%d\n",MAJOR(devno),MINOR(devno));

	cdev = cdev_alloc(); // �����ַ��豸����
	cdev->ops = &hzhy_sgi_fops;
	cdev->owner = THIS_MODULE;

	ret = cdev_add(cdev, devno, 1); //ע���ַ��豸
	if(ret) {
		printk(KERN_ALERT"Error %d adding chdrv",ret);
		return ret;
	}

	// �Զ������豸�ڵ�
	class = class_create(THIS_MODULE, DRIVER_NAME); //����һ������
	if (IS_ERR(class)) {
		printk("class create error!\n");
		return IS_ERR(class);
	}
	device_create(class, NULL, devno, NULL, DRIVER_NAME);

	return 0;
}

static void my_char_cleanup(void)
{
	device_destroy(class, devno); //delete device node under /dev
    class_destroy(class); //delete class created

	unregister_chrdev_region(devno, 1);
}

static int hzhy_sgi_remove(struct platform_device *pdev)
{
	my_char_cleanup();
	
	return 0;
}

static int hzhy_sgi_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	my_sgi = devm_kzalloc(&pdev->dev, sizeof(*my_sgi), GFP_KERNEL);	//������ܷ�����ڴ��������ж��ʱ�Զ��ͷ�
	if (!my_sgi)
		return -ENOMEM;

#if 0
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	my_sgi->base = devm_ioremap_resource(&pdev->dev, res);
	printk("0x%x ioremap 0x%x\n",res->start,(unsigned int)my_sgi->base);
	if (IS_ERR(my_sgi->base))
		return PTR_ERR(my_sgi->base);
#endif
	ret = my_char_register();	//ע���ַ��豸
	init_sharedmem();
	return ret;
}

static struct of_device_id hzhy_sgi_of_match[] = {
	{ .compatible = "hzhy,shareram-1.0", },
	{ /* end of table */ }
};
MODULE_DEVICE_TABLE(of, hzhy_sgi_of_match);

static struct platform_driver hzhy_sgi_driver = {
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = hzhy_sgi_of_match,
	},
	.probe		= hzhy_sgi_probe,
	.remove		= hzhy_sgi_remove,
};


static int hzhy_sgi_init(void)
{
	return platform_driver_register(&hzhy_sgi_driver);
}

static void hzhy_sgi_exit(void)
{
	platform_driver_unregister(&hzhy_sgi_driver);
}


module_init(hzhy_sgi_init);
module_exit(hzhy_sgi_exit);



MODULE_AUTHOR("HZHY TECH.Xiao Zhixin");
MODULE_DESCRIPTION("hzhy sgi driver");
MODULE_LICENSE("GPL");