/*
 ============================================================================
 Name        : testmmap.c
 Author      : hfp
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DEVNAME "/dev/mmap"
#define BUFFER_SIZE    (4*1024*1024)
#define SIZE	100
int main(void)
{
	int fd,i,ret;
	void * buff;
	unsigned char cbuf[SIZE];
	//读写方式打开文件
	fd=open(DEVNAME,O_RDWR);
	if(fd<0)
	{
		printf("open %s failed: %s\n",DEVNAME,strerror(errno));
		return -1;
	}
	//映射内核缓存
	buff=mmap(NULL,BUFFER_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	if(buff==MAP_FAILED)
	{
		printf("mmap failed: %s\n",strerror(errno));
		close(fd);
		return -1;
	}
	printf("buff %p \n",buff);
	//通过write写内核缓存
	for(i=0;i<SIZE;i++) cbuf[i] = i;
	ret = write(fd,cbuf,SIZE);
	printf("write:%d\n",ret);
	//通过映射访问内核缓存区
	for(i=0;i<4;i++)
	{
		//通过直接访问映射区的方法验证之前write写内核缓存成功
		printf("read offset %d:0x%x \n",i,*((unsigned int *)(buff+(i<<2))));
		//直接写映射区
		*((unsigned int *)(buff+(i<<2)))=0x55aaaa55;
		//直接读映射区
		printf("read offset %d:0x%x \n",i,*((unsigned int *)(buff+(i<<2))));
	}
	//通过read判断之前通过映射的方式确实将数据写入内核缓存
	memset(cbuf,0,SIZE);
	ret = read(fd,cbuf,SIZE);
	printf("read:%d\n",ret);
	for(i=0;i<SIZE;i++) printf("offset %d:0x%x\n",i,cbuf[i]);
	munmap(buff,BUFFER_SIZE);
	close(fd);
	return 0;
}
