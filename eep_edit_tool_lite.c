/*******************************************************************************
*   COPYRIGHT (C) 2018 MACROSAN, INC. ALL RIGHTS RESERVED.
* --------------------------------------------------------------------------
*  This software embodies materials and concepts which are proprietary and
*  confidential to MACROSAN, Inc.
*  MACROSAN distributes this software to its customers pursuant to the terms
*  and conditions of the Software License Agreement contained in the text
*  file software. This software can only be utilized if all terms and conditions
*  of the Software License Agreement are accepted. If there are any questions,
*  concerns, or if the Software License Agreement text file is missing please
*  contact MACROSAN for assistance.
* --------------------------------------------------------------------------
*   Date: 2018-12-12
*   Version: 1.5
*   Author: ZhangPeng
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#define u32 unsigned int

u32 eep_image_size = 0;
int fd;

void eepwrite32(int fd,u32 offset, u32 val);
u32 eepread32(int fd,u32 offset);
int is_version_divided_two_parts(int fd);
u32 get_eep_version_offset(int fd);
void modify_version(int fd,u32 data);

int main (int argc, char **argv)
{
    char *file_name;
	int i = 0, j = 0;
    struct stat fstatus;

    u32 val,fsize;
    u32 sn,ver,data;
	
    if(argc != 4)
    {
       printf("Usage: %s [eep_file] [sn] [version]\n",argv[0]);
       return 0;
    }
    
	if((0 != strncmp("0x",argv[2],2) && 0 != strncmp("0X",argv[2],2)) ||
	   (0 != strncmp("0x",argv[3],2) && 0 != strncmp("0X",argv[3],2)) )
	{
		printf("ERR: please enter the hexadecimal number(prefix '0x' 0r '0X')\n");
		return 0;
	}
	
	for(i=2; i<=3; i++)
    {
        if(2 >= strlen(argv[i]))
	    {
		    printf("%s is invalid\n",argv[i]);
		    return 0;
	    }

	    for (j=2; j<strlen(argv[i]); j++)
	    {
		    if(!isxdigit(argv[i][j]))
		    {
			    printf("ERR: %s is not a hexadecimal number\n",argv[i]);
			    return 0;
            }		
	    } 
    }

    file_name = argv[1];
    fd = open(file_name,O_RDWR);
    if(fd < 0)
    {
        printf("ERR: open file fail\n");
        return -1;
    }

    /*获取文件的长度*/
	if(stat(file_name, &fstatus) < 0)
	{
		printf("ERR: get file size failed\n");
		goto err;
	}
	else
	{
		fsize = fstatus.st_size;
	}

    val = eepread32(fd,0);
    eep_image_size = (val >> 16) + 12;

    if( fsize != eep_image_size &&
	    fsize != eep_image_size - 4 &&
	    fsize != eep_image_size - 8 )
    {
        printf("ERR: This is not a valid eep file!\n");
		goto err;
    }
	
	sn = strtoul(argv[2],NULL,16);//一个字节有效
    ver = strtoul(argv[3],NULL,16);//一个字节有效
	
	if( sn < 0 || sn > 255 )
	{
		printf("ERR: %s should at [0x0,0xff]\n",argv[2]);
		goto err;
	}
	if( ver < 0 || ver > 255 )
	{
		printf("ERR: %s should at [0x0,0xff]\n",argv[3]);
		goto err;
	}
	
    data = (sn << 8) | ver;     //低两个字节有效
    modify_version(fd,data);
	
    close(fd);
    return 0;
err:
	close(fd);
	return -1;
}

void eepwrite32(int fd,u32 offset, u32 val)
{
	unsigned char ch = 0;
    int i, j;
    int ret;

    for(i = offset * 4 + 3,j = 3; i >= offset * 4; i--,j--)
    {
        if(i < eep_image_size)
        {
            ret = lseek(fd,i,SEEK_SET);
            if(-1 == ret)
            {
                break;
            }
            ch = (val >> (8 * j)) & 0xff;
            write(fd,&ch,1);
        }         
    } 
}

u32 eepread32(int fd,u32 offset)
{
	unsigned char ch = 0;
    u32 val = 0;
    int i = 0, size = 0;
    int ret;
	
    for(i = offset*4+3; i >= offset*4; i--)
    {

        ret=lseek(fd,i,SEEK_SET);
        if(-1 == ret)
        {
            break;
        }
        size = read(fd,&ch,1);
        if(1 != size)
        {
            ch = 0x0; 
        }
        val = (val << 8 ) | ch;       
    }
    return val;
}

/*判断最后四字节的版本信息是否需要进行两次读/写*/
int is_version_divided_two_parts(int fd)
{
    int residue = 0;
    u32 val,eep_image_size; 

    val = eepread32(fd,0);
    eep_image_size = (val >> 16) + 12;//前面四个字节 四字节的校验码，四字节的版本信息，共十二字节
    residue = eep_image_size%4;
    
    return residue;
}

/*计算eerpom文件版本信息的偏移,在此之前必须确定最后四字节的版本信息是否需要进行两次读、写，
 *从而确定需要偏移一次或两次，该函数总是返回第一次的偏移*/
u32 get_eep_version_offset(int fd)
{
    int quotient = 0;
    u32 val,eep_image_size,offset; 
    
    val = eepread32(fd,0);
    eep_image_size = (val >> 16) + 12;
    quotient = eep_image_size / 4;
    offset = quotient - 1;

    return offset;
}

/*把传入的dword字节版本信息写入eeprom文件*/
void modify_version(int fd,u32 data)
{
    u32 offset,val,magic;
    magic = (0x5a << 24) | (data & 0xffff); //把带有序列号和版本信息的dword加上0x5a到最高一个字节使得版本信息生效
    offset = get_eep_version_offset(fd);
    if(is_version_divided_two_parts(fd))
    {
        val = eepread32(fd,offset); //读取需要覆盖的数据
        val &= 0xffff;       
        val |= (magic & 0xffff) << 16; //确保不会覆盖校验位高2byte        
        eepwrite32(fd,offset,val); //第一部分写入2 byte低字节
        offset++;     
        val = eepread32(fd,offset);
        val &= 0xffff0000;
        val |= magic >> 16;
        eepwrite32(fd,offset,val); //第二部分写入2 byte高字节
    }
    else
    {
        eepwrite32(fd,offset,magic);
    }
}

