#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <asm/io.h>

#define PRINT	1
#define DEVICE_NAME  "mmap"
#define DRIVER_AUTHOR "hfp<hefuping_20080824@126.com>"
#define DRIVER_DESC   "mmap driver test"
#define DEVICE_NODE	"mmap"
#define BUFFER_SIZE (4*1024*1024)
 struct my_dev{
    dev_t devno ;
    struct cdev cdev;
    dev_t dev_num;
    struct device *dev;
    struct class * my_class;
    void __iomem *    buff_base;/*内核态缓存区虚拟地址*/
};
typedef struct my_dev * my_dev_t;
my_dev_t my_dev;

static int major=0;
static int minor=0;

MODULE_LICENSE ("GPL");
MODULE_AUTHOR (DRIVER_AUTHOR);
MODULE_DESCRIPTION (DRIVER_DESC);

static int32_t char_open(struct inode *inode, struct file *filp)
{
	struct page *page;
	printk("line:%d\tfunc:%s\n",__LINE__,__func__);
	my_dev_t mydevp = container_of(inode->i_cdev, struct my_dev, cdev);
	filp->private_data = mydevp;
	printk("line:%d\tfunc:%s\n",__LINE__,__func__);
	/*申请用于映射的缓冲区的内存*/
	mydevp->buff_base = kzalloc(BUFFER_SIZE,GFP_KERNEL);
    if(IS_ERR(mydevp->buff_base))
    {
    	printk(KERN_ERR"kzalloc error\n");
    	return -1;
    }
    printk("line:%d\tfunc:%s\n",__LINE__,__func__);
    /*设置为保留页,使页面不被交换*/
	page=virt_to_page(mydevp->buff_base);
	printk("line:%d\tfunc:%s\n",__LINE__,__func__);
	for(;page<virt_to_page(mydevp->buff_base+BUFFER_SIZE);page++)
	{
		SetPageReserved(page);
	}
	printk("line:%d\tfunc:%s\n",__LINE__,__func__);
	return 0;
}
ssize_t char_read(struct file *filp, char __user *pbuf, size_t count, loff_t *f_pos)
{
	my_dev_t mydevp;
	ssize_t len,result = 0;
	mydevp = filp->private_data;
	len = count<BUFFER_SIZE?count:BUFFER_SIZE;
	if (copy_to_user (pbuf,mydevp->buff_base,len))
		result = -EFAULT;
	else
		printk ("wrote %d bytes\n", count);
	return len;
}
ssize_t char_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *f_pos)
{
	my_dev_t mydevp;
	ssize_t len,result = 0;
	mydevp = filp->private_data;
	len = count<BUFFER_SIZE?count:BUFFER_SIZE;
	if (copy_from_user (mydevp->buff_base, pbuf, len))
		result = -EFAULT;
	return len;
}
static int32_t char_release(struct inode *inode, struct file *filp)
{
	struct page *page;
	my_dev_t mydevp;
	mydevp = filp->private_data;
    page=virt_to_page(mydevp->buff_base);
	for(;page<virt_to_page(mydevp->buff_base+BUFFER_SIZE);page++)
	{
		ClearPageReserved(page);
	}
	kfree(mydevp->buff_base);
	return 0;
}

int char_mmap(struct file *filp, struct vm_area_struct *vma)
{
	my_dev_t mydevp;
	mydevp = filp->private_data;
	unsigned long size = vma->vm_end - vma->vm_start;
//	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	printk("1vma start 0x%lx ,end 0x%lx ,vm_pgoff 0x%lx,size 0x%x\n",vma->vm_start,vma->vm_end,vma->vm_pgoff,size);
	if(size>BUFFER_SIZE)
	{
		printk("mmap size is 0x%x,large than 0x%x bytes\n",size,BUFFER_SIZE);
		return -ENOMEM;
	}
	vma->vm_flags|=VM_DONTEXPAND | VM_DONTDUMP;
	//将一个内核虚地址mydevp->buff_base转化成页的描述结构 struct page *，然后根据需要的大小size进行映射
	if(remap_pfn_range(vma,vma->vm_start,page_to_pfn(virt_to_page(mydevp->buff_base)),size,vma->vm_page_prot))
	return -EAGAIN;
	printk("2vma start 0x%lx ,end 0x%lx ,vm_pgoff 0x%lx\n",vma->vm_start,vma->vm_end,vma->vm_pgoff);
	return 0;
}

struct file_operations char_fops = {
  .owner = THIS_MODULE,
  .open = char_open,
  .release = char_release,
  .read = char_read,
  .write = char_write,
  .mmap = char_mmap,
};

static int __init char_init_module (void)
{
	int result , error;;
	my_dev = kzalloc(sizeof(struct my_dev),GFP_KERNEL);
    if(IS_ERR(my_dev))
    {
    	printk(KERN_ERR"kzalloc error\n");
    	return -1;
    }
	result=alloc_chrdev_region(&my_dev->devno,0,1,DEVICE_NAME);
	if (result!=0)
	{
		printk (KERN_WARNING "char: can't get major number %d\n", major);
		return result;
	}
	major = MAJOR(my_dev->devno);
	minor = MINOR(my_dev->devno);
	printk (KERN_WARNING "major:%d\tminor:%d\n",major,minor);

	//以my_dev的成员的形式初始化cdev，保证将来可以通过inode结构的i_cdev成员访问cdev，从而访问my_dev
	cdev_init (&my_dev->cdev, &char_fops);
	my_dev->cdev.owner = THIS_MODULE;
	error = cdev_add (&my_dev->cdev, my_dev->devno , 1);

	my_dev->my_class =class_create(THIS_MODULE, "mmap_class");
	if(IS_ERR(my_dev->my_class))
	{
		printk("Err: failed in creating class.\n");
		return -1;
	}
	device_create(my_dev->my_class,NULL,my_dev->devno,NULL, DEVICE_NODE,minor);
	printk ("char device registered\n");
	return 0;
}
module_init (char_init_module);
static void __exit char_cleanup_module (void)
{
	int minor;
	cdev_del (&my_dev->cdev);
	unregister_chrdev_region (my_dev->devno, 1);
	device_destroy(my_dev->my_class,my_dev->devno);
	class_destroy(my_dev->my_class);
	printk ("char_cleanup_module\n");
}
module_exit (char_cleanup_module);
