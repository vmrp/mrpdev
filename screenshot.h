#ifndef _SCREENSHOT_H_
#define _SCREENSHOT_H_

//mrper使用的截16位bmp图函数

/*注意 只适合于16bit屏幕

参数说明

pBmp	截图地址，注意这里是char * 指针

SaveDir	保存文件夹(截图命名规则 SaveDir+4位日期+4位时间+2位区分数.bmp)

x,y,w,h	是截图位置

max_h	是屏幕宽度

isSPR	表征是否是SPR展讯平台,取值0/1 对应 MTK/SPR

返回值：
MR_SUCCESS	截屏成功
MR_FAILED	截屏失败（创建文件失败）

示例截MTK大屏全屏
picShot( (char *)w_getScreenBuffer,0,0,240,320,240,0);
*/
int32 picShot(char *pBmp,char *SaveDir,int16 x,int16 y,int16 w,int16 h,int16 max_w,int8 isSPR);

#endif