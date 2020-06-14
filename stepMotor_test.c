#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "stepMotor.h"

static stepMotor_workmode *mode;

const char numTable[10] = {'0','1','2','3','4','5','6','7','8','9'};

static unsigned char chrcmp(char a,char b)
{
    if (a == b) return 0;
    else return 1;
}

static unsigned char str2num(char *str)
{
    int num , len ,i,j;
    num = 0;
    len = strlen(str);
    for(i=0;i<len;i++)
    {
        for(j = 0; j <10 ; j++)
        {
            if( !chrcmp( str[i], numTable[j]) ) num = num*10 + j;
        }
    }
    return num;
}

int main(int argc, char **argv)
{
	int fd;
    unsigned long ret;
    mode = NULL;
	/* 1. 判断参数 */
	if (argc != 3) 
	{
		printf("Usage: %s <direction> <stepInterval>\n", argv[0]);
		return -1;
	}

	/* 2. 打开文件 */
	fd = open("/dev/myMotor", O_WRONLY);
	if (fd == -1)
	{
		printf("can not open file /dev/myMotor\n");
		return -1;
	}
    mode = malloc(sizeof(stepMotor_workmode));
    if(mode == NULL) 
    {
        printf("Mem alloc failed!\n");
        close(fd);
        return -1;
    }
    ret = str2num(argv[1]);
    if(ret == 0 | ret == 1)
    {
        mode->direction = (char)ret;
        printf("direction = %d",mode->direction);
    }    
    else 
    {
        printf("direction must be 0 or 1!\n");
        free(mode);
        close(fd);
        return -1;
    }
    ret = str2num(argv[2]);
    if(ret < 2)
    {
        printf("stepInterval must be larger than 16!\n");
        free(mode);
        close(fd);
        return -1;
    }
    else
    {
        mode->stepInterval = ret;
        printf("stepInterval = %d\n",mode->stepInterval);
    } 
    write(fd,mode,sizeof(stepMotor_workmode));
    while(1)
    {

    }
    return 0;
}
