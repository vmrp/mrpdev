#include "mrc_base.h"
#include "mrc_bmp.h"
#include "debug.h"



uint16 *OldScreen;//真实屏幕缓冲地址
uint16 *NewScreen;//后台屏幕缓冲地址
int32 Scr_w,Scr_h;//屏幕信息

/********   提供一块与真实屏幕宽高逆转的后台缓存 ********/
/*           void RotateScreen(void)                    */
/*           配合 RotateRefresh  使用                   */
/********   提供一块与真实屏幕宽高逆转的后台缓存 ********/
void RotateScreen(void)
{
    mrc_getScreenSize(&Scr_w,&Scr_h);//获取屏幕缓冲信息
    NewScreen=mrc_malloc(Scr_w*Scr_h*2);//申请一块后台缓冲,如果绘制操作频繁,建议只申请1次
    mrc_memset(NewScreen,0,Scr_w*Scr_h*2);
    w_setScreenBuffer(NewScreen);//将后台缓冲暂时设置为屏幕缓冲地址
    mrc_setScreenSize(Scr_h,Scr_w);//此处实现宽高掉转
}
/********   将后台缓存数据旋转90度拷贝到真实屏幕并刷新   ********/
/*           void RotateRefresh(void)                           */
/*           配合 RotateScreen  使用                            */
/********   将后台缓存数据旋转90度拷贝到真实屏幕并刷新   ********/
void RotateRefresh(void)
{
     w_setScreenBuffer(OldScreen);  //此处还原真实屏幕缓冲地址
     mrc_setScreenSize(Scr_w,Scr_h);//此处还原真实屏幕宽高
     mrc_bitmapShowFlip(NewScreen,0,0,(int16)Scr_h,(int16)Scr_h,(int16)Scr_w,BM_COPY|TRANS_ROT90,0,0,0);//此处将后台缓冲旋转90度后,绘制到初始屏幕缓冲
     mrc_refreshScreen(0,0,(int16)Scr_w,(int16)Scr_h);//毫无疑问将屏幕缓冲内容更新到屏幕

     mrc_free(&NewScreen);//释放后台缓冲,如果绘制操作频繁,建议到程序退出时再释放
     NewScreen=NULL;
}

void HorizontalScreenTest()
{
    int32 f;

	OldScreen = w_getScreenBuffer();//获取真实屏幕缓冲地址
	
	RotateScreen();//配合RotateRefresh使用
    
    mrc_bitmapNew(0, 41, 26);
    mrc_bitmapLoad(0, "plane.bmp", 0, 0, 41, 26,41);
    mrc_bitmapShow(0, 240,50,BM_COPY,0, 0, 41, 26);
    mrc_drawText("原始图",0,0,255,0,0,0,1);
    mrc_drawLine(0,0,320,0,255,0,0);

    mrc_refreshScreen(0,0,240,320);
    f=mrc_open("data.bmp",12);
    mrc_write(f,NewScreen,Scr_w*Scr_h*2);
    mrc_close(f);
//     return;
	RotateRefresh();//配合RotateScreen使用
}

int32 MRC_EXT_INIT(void)
{   
    HorizontalScreenTest();

    return MR_SUCCESS;
}

int32 MRC_EXT_EXIT(void)
{	
return MR_SUCCESS;
}

int32 mrc_appEvent(int32 code, int32 param0, int32 param1)
{
return MR_SUCCESS;
}

int32 mrc_appPause()
{
return MR_SUCCESS;	
}

int32 mrc_appResume()
{
return MR_SUCCESS;
}
