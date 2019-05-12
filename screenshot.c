//作者 space - 连
//本源码公开，任何个人可以用于正当用途，源码请标明作者
//作者网盘http://ourmrp.ys168.com
//简单16位截屏函数，适用于MTK手机和SPR手机截屏
//				space 于 2011-9-9

#include "mrc_base.h"

#define BMP_HEAD "\x42\x4D\x42\xC8\x00\x00\x00\x00\x00\x00\x46\x00\x00\x00\x38\x00\x00\x00\xA0\x00\x00\x00\xA0\x00\x00\x00\x01\x00\x10\x00\x03\x00\x00\x00\x00\xC8\x00\x00\xA0\x0F\x00\x00\xA0\x0F\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"  //54字节
#define BMP_HEAD_MTK "\x00\xF8\x00\x00\xE0\x07\x00\x00\x1F\x00\x00\x00\x00\x00\x00\x00"   //16字节
#define BMP_HEAD_SPR "\xF8\x00\x00\x00\x07\xE0\x00\x00\x00\x1F\x00\x00\x00\x00\x00\x00"

int32 picShot(char *pBmp,char *SaveDir,int16 x,int16 y,int16 w,int16 h,int16 max_w,int8 isSPR)
{
	mr_datetime datetime;
	char saveShotPath[24]={0};
	int32 dirlen=mrc_strlen(SaveDir);
	int8 elseNum=w%2;
	char *pTempBmp=NULL;
	int32 fileH=0,i=0;

	mrc_getDatetime(&datetime);
	mrc_sprintf(saveShotPath,"%s%02d%02d_%02d%02d_%02d.bmp",SaveDir,datetime.month ,datetime.day ,datetime.hour ,datetime.minute ,i);	
	for (i=1;i<100;i++)
	{
		if(MR_IS_FILE==mrc_fileState(saveShotPath))
		{
			mrc_sprintf(saveShotPath+dirlen+10,"%02d.bmp",i);
		}else 
		{
			break;
		}
	}
	fileH=mrc_open(saveShotPath,MR_FILE_RDWR|MR_FILE_CREATE);
	if(fileH)
	{
		int32 fileLen=(w+elseNum)*h*2+54+16+2;
		int32 addPos=0;
		pTempBmp=mrc_malloc(fileLen);
		mrc_memset(pTempBmp,0,fileLen);
		mrc_memcpy(pTempBmp,BMP_HEAD,54);
		if (isSPR==TRUE)
		{
			for (i=0;i<4;i++)
			{
				mrc_memcpy(pTempBmp+2+i,(char*)&fileLen+3-i,1);
			}
			for (i=0;i<4;i++)
			{
				mrc_memcpy(pTempBmp+18+i,(char*)&w+3-i,1);
			}
			for (i=0;i<4;i++)
			{
				mrc_memcpy(pTempBmp+22+i,(char*)&h+3-i,1);
			}
			mrc_memcpy(pTempBmp+54,BMP_HEAD_SPR,16);
			//mrc_write(fileH,BMP_HEAD_SPR,16);
		}else
		{
			mrc_memcpy(pTempBmp+2,&fileLen,4);
			mrc_memcpy(pTempBmp+18,&w,4);
			mrc_memcpy(pTempBmp+22,&h,4);
			mrc_memcpy(pTempBmp+54,BMP_HEAD_MTK,16);
			//mrc_write(fileH,BMP_HEAD_MTK,16);
		}
		addPos=54+16;
		for (i=h+y -1;i>=y;i--)
		{
			mrc_memcpy(pTempBmp+addPos,pBmp+(x+i*max_w )*2,w*2);
			addPos+=(w+elseNum)*2;
			//mrc_write(fileH,(uint8 *)pBmp+x*2 +i*max_w *2,w*2 );
			//mrc_write(fileH,"\00\00",2*elseNum);
		}
		mrc_seek(fileH,0,MR_SEEK_SET);
		mrc_write(fileH,pTempBmp,fileLen);
		mrc_close(fileH);
		fileH=0;
		mrc_free(pTempBmp),pTempBmp=NULL;
		return MR_SUCCESS;
	}else
	{
		return MR_FAILED;
	}
}