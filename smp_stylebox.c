#include "smp_stylebox.h"

#include "smp.h"
#include "bmp.h"
#include "mrc_exb.h"
#include "types.h"


#define SMP_BOX_RECOVER_SPEED	3		//滑动恢复的加速度（单位：像素/次）


/*宫格的一项所具有的信息*/
typedef struct SMP_ItemData{
	int index;		//序号
	int width;		//宽度
	int height;		//高度
	HBITMAP hBmp;	//图片句柄
}SMPITEMDATA, *PSMPITEMDATA;


/*窗口结构体*/
typedef struct
{
	int		num;		//总数量
	BOOL	focus;		//是否有焦点
	int		onewidth;	//一个宽高
	int		oneheight;

	int rows;		//行
	int stand;		//多少列

	int		topmagin;	//滑动时  顶部边距
	int		leftmagin;

	int		highlight;	//哪个高亮
}BOXDATA, *PBOXDATA;


#define SMP_STYLEBOX_MAX	10	

static SMPITEMDATA BoxItem[SMP_STYLEBOX_MAX];	//九宫格每一项




/*设置信息*/
VOID SMP_Stylewnd_SetItem(HWND hWnd, const DWORD* bmps, int size)
{
	
	PBOXDATA pBoxData = _GET_WINDATA(hWnd, PBOXDATA);
	int i;
	if(size>SMP_STYLEBOX_MAX || !bmps)
		return;

	for (i=0; i<size; i++)
	{
		BoxItem[i].index = i;
		BoxItem[i].hBmp = SGL_LoadBitmap(bmps[i], &BoxItem[i].width, &BoxItem[i].height);
	}
	pBoxData->num = size;
	pBoxData->highlight = 4;	//默认最中间获得焦点

	pBoxData->onewidth = _WIDTH(hWnd)/3;
	pBoxData->oneheight = _HEIGHT(hWnd)/3;
	pBoxData->topmagin = 0;
	pBoxData->leftmagin = 0;
	
	pBoxData->rows = 3;		//默认3*3  九宫格
	pBoxData->stand = 3;
}


VOID SMP_Update_OneBox(HWND hWnd, int index)
{
	int hx, hy;		//该项源坐标（hx, hy）
	PBOXDATA pBoxData = _GET_WINDATA(hWnd, PBOXDATA);

	hx = _LEFT(hWnd) + pBoxData->onewidth*(index%3);
	hy = _TOP(hWnd) + pBoxData->oneheight*(index/3);

	if (hy + pBoxData->topmagin >= _TOP(hWnd) && hy+pBoxData->topmagin+pBoxData->highlight <= _TOP(hWnd)+_HEIGHT(hWnd))
	{
		if (pBoxData->highlight == index)//高亮
			GAL_FillRoundRrct(hx+(pBoxData->onewidth - BoxItem[index].width)/2 + pBoxData->leftmagin-3, 
			hy+(pBoxData->oneheight - BoxItem[index].height)/2 + pBoxData->topmagin-2,
			BoxItem[index].width+6, BoxItem[index].height+4, COLOR_focus);
		else
			GAL_FillBox(PHYSICALGC, hx, hy, pBoxData->onewidth, pBoxData->oneheight, _BGCOLOR(hWnd));
		mrc_bitmapShowEx(BoxItem[index].hBmp, 
			hx+(pBoxData->onewidth - BoxItem[index].width)/2 + pBoxData->leftmagin, 
			hy+(pBoxData->oneheight - BoxItem[index].height)/2 + pBoxData->topmagin, 
			BoxItem[index].width, BoxItem[index].width, BoxItem[index].height, BM_TRANSPARENT, 0, 0);

		GAL_FlushRegion(PHYSICALGC, hx + pBoxData->leftmagin, hy + pBoxData->topmagin, pBoxData->onewidth, pBoxData->oneheight);
	}
}



/*绘制所有项，主要是平滑移动的时候
*  没有采取将当前屏幕载入缓冲进行重复刷新，而是不断重绘刷新
*/
VOID SMP_DrawStyleBox(HWND hWnd)
{
	int i, j, index = 0;
	int x, y, w, h;	//起始坐标，及宽高
	int hx, hy;		//绘制临时坐标
	
	PBOXDATA pBoxData = _GET_WINDATA(hWnd, PBOXDATA);
	x = _LEFT(hWnd);
	y = _TOP(hWnd);
	w = _WIDTH(hWnd);
	h = _HEIGHT(hWnd);
	hy = y;
	for (i=0; i<pBoxData->rows; i++)//行
	{
		hx = x;
		for (j=0; j<pBoxData->stand; j++)//列
		{
			if (pBoxData->highlight == index)//高亮
				GAL_FillRoundRrct(hx+(pBoxData->onewidth - BoxItem[index].width)/2 + pBoxData->leftmagin-3, 
				hy+(pBoxData->oneheight - BoxItem[index].height)/2 + pBoxData->topmagin-2,
				BoxItem[index].width+6, BoxItem[index].height+4, COLOR_focus);

			mrc_bitmapShowEx(BoxItem[index].hBmp, 
				hx+(pBoxData->onewidth - BoxItem[index].width)/2 + pBoxData->leftmagin, 
				hy+(pBoxData->oneheight - BoxItem[index].height)/2 + pBoxData->topmagin, 
				BoxItem[index].width, BoxItem[index].width, BoxItem[index].height, BM_TRANSPARENT, 0, 0);
			hx+=pBoxData->onewidth;
			index++;
		}
		hy+=pBoxData->oneheight;
	}
}

/*获得高亮序号， 在本窗口产生事件时，上一层可以通过(int)lParam获得高亮序号*/
int SMP_Box_GetHilightId(HWND hWnd)
{
	PBOXDATA pBoxData = _GET_WINDATA(hWnd, PBOXDATA);
	return pBoxData->highlight;
}



/*松开鼠标或者按键，如果不在原位，则需要恢复原位*/
VOID SMP_Box_RecoverLoaction(HWND hWnd)
{
	PBOXDATA pBoxData = _GET_WINDATA(hWnd, PBOXDATA);
	int i;
	//int x_step = 1;
	int y_step = 1;	//充当加速度

	if(pBoxData->leftmagin==0 && pBoxData->topmagin==0)
		return;

	if(pBoxData->topmagin>0)//从上面回来
	{
		for (i=0; i<=pBoxData->topmagin; i++, y_step+=SMP_BOX_RECOVER_SPEED)//加 速度为1
		{
			if(pBoxData->topmagin - y_step<0)
				pBoxData->topmagin = 0;
			else
				pBoxData->topmagin-= y_step;

			SGL_TRACE("%d", pBoxData->topmagin);
			SGL_UpdateWindow(hWnd);
		}
	}else if(pBoxData->topmagin<0)//从下面回来
	{
		for(i=0; i>=pBoxData->topmagin; i--, y_step+=SMP_BOX_RECOVER_SPEED)
		{
			if(pBoxData->topmagin + y_step>0)
				pBoxData->topmagin = 0;
			else
				pBoxData->topmagin+=y_step;

			SGL_TRACE("%d", pBoxData->topmagin);
			SGL_UpdateWindow(hWnd);
		}
	}
}

LRESULT SMP_Box_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	PBOXDATA pBoxData = _GET_WINDATA(hWnd, PBOXDATA);
	//static int start_x = 0;
	static int start_y = 0;
	static int click_count = 0;
	static int key_step = 0;	//键盘模拟滑动速度
	
	switch(Msg)
	{
	case WM_CREATE:
		{
			//request memory and initialize members
			pBoxData = (PBOXDATA)SGL_MALLOC(sizeof(BOXDATA));
			if(!pBoxData){
				SGL_TRACE("%s, %d: memory out\n", __FILE__, __LINE__);
				return 1;
			}
			SGL_MEMSET(pBoxData, 0, sizeof(BOXDATA));
			_SET_WINDATA(hWnd, pBoxData);
			return 0;
		}

	case WM_DESTROY:
		{
			if(pBoxData) SGL_FREE(pBoxData);
			return 0;
		}

	case WM_SHOW:
		{
			return 0;
		}

	case WM_PAINT:
		{
			_BGCOLOR(hWnd) = 0x00ddeeff;
			GAL_FillBox(PHYSICALGC, _LEFT(hWnd), _TOP(hWnd), _WIDTH(hWnd), _HEIGHT(hWnd), _BGCOLOR(hWnd));
			SMP_DrawStyleBox(hWnd);
			return 0;
		}

	case WM_MOUSEDOWN:
		{
			int index_x = 0;	//该序号对应区域所在的起始坐标(index_x,index_y)
			int index_y = 0;
			int oldhl = pBoxData->highlight;	//旧高亮图标
			index_x = ((int)wParam)/pBoxData->onewidth;
			index_y = ((int)lParam)/pBoxData->oneheight;

			pBoxData->highlight = (index_x+3*index_y);

			if	(oldhl == pBoxData->highlight)
			{
				click_count ++;
				SGL_NotifyParent(hWnd, SMP_BOXI_CLICKED, pBoxData->highlight);
			}
			else if (oldhl != pBoxData->highlight)//高亮切换 
			{
				click_count = 1;
				SMP_Update_OneBox(hWnd, oldhl);			//刷新图标
				SMP_Update_OneBox(hWnd, pBoxData->highlight);
				SGL_NotifyParent(hWnd, SMP_BOXI_HILICHANGED, pBoxData->highlight);
			}

			//位置复原
			if (!pBoxData->leftmagin|| !pBoxData->topmagin)
				SMP_Box_RecoverLoaction(hWnd);

			//////////////////////////////////////////////////////////////////////////
			//检测鼠标移动前的Y坐标
			start_y = (int)lParam;

			return 0;
		}

	case WM_MOUSEMOVE:
		{
			//pBoxData->leftmagin = ((int)wParam - start_x);
			pBoxData->topmagin = ((int)lParam - start_y);
			SGL_UpdateWindow(hWnd);
			SGL_NotifyParent(hWnd, SMP_BOXI_MOUSEMOVE, 0);
			return 0;
		}
	case WM_MOUSEUP:
		{
			int index_x = 0;
			int index_y = 0;
			int oldhl = pBoxData->highlight;
			index_x = ((int)wParam)/pBoxData->onewidth;
			index_y = ((int)lParam)/pBoxData->oneheight;
			pBoxData->highlight = (index_x+3*index_y);
			if (oldhl == pBoxData->highlight)
			{
				if (click_count == 2)//第二次点击后并且松开
				{
					click_count = 1;
					SGL_NotifyParent(hWnd, SMP_BOXI_SELECTED, pBoxData->highlight);
				}
			}else if (oldhl != pBoxData->highlight)//高亮切换 
			{
				SMP_Update_OneBox(hWnd, oldhl);			//刷新图标
				SMP_Update_OneBox(hWnd, pBoxData->highlight);
				SGL_NotifyParent(hWnd, SMP_BOXI_HILICHANGED, pBoxData->highlight);
			}

			//位置复原
			if (!pBoxData->leftmagin|| !pBoxData->topmagin)
				SMP_Box_RecoverLoaction(hWnd);
			return 0;
		}

	case WM_KEYDOWN:
	case WM_KEYDOWNREPEAT:
		{
			int oldhl = pBoxData->highlight;
			switch(wParam)
			{
				/*处理键盘移动*/
			case MR_KEY_UP:
				{
					if (pBoxData->highlight>2)
						pBoxData->highlight-=3;
					else
						pBoxData->highlight+=6;
					SMP_Update_OneBox(hWnd, oldhl);			//刷新图标
					SMP_Update_OneBox(hWnd, pBoxData->highlight);
					SGL_NotifyParent(hWnd, SMP_BOXI_HILICHANGED, pBoxData->highlight);
					return 1;
				}
			case MR_KEY_DOWN:
				{
					if (pBoxData->highlight<6)
						pBoxData->highlight+=3;
					else
						pBoxData->highlight-=6;
					SMP_Update_OneBox(hWnd, oldhl);			//刷新图标
					SMP_Update_OneBox(hWnd, pBoxData->highlight);
					SGL_NotifyParent(hWnd, SMP_BOXI_HILICHANGED, pBoxData->highlight);
					return 1;
				}
			case MR_KEY_LEFT:
				{
					if (pBoxData->highlight>0)
						pBoxData->highlight--;
					else
						pBoxData->highlight = 8;
					SMP_Update_OneBox(hWnd, oldhl);			//刷新图标
					SMP_Update_OneBox(hWnd, pBoxData->highlight);
					SGL_NotifyParent(hWnd, SMP_BOXI_HILICHANGED, pBoxData->highlight);
					return 1;
				}
			case MR_KEY_RIGHT:
				{
					if (pBoxData->highlight<8)
						pBoxData->highlight++;
					else
						pBoxData->highlight = 0;
					SMP_Update_OneBox(hWnd, oldhl);			//刷新图标
					SMP_Update_OneBox(hWnd, pBoxData->highlight);
					SGL_NotifyParent(hWnd, SMP_BOXI_HILICHANGED, pBoxData->highlight);
					return 1;
				}
			case MR_KEY_2://键盘模拟，向上滑动
				{
					if(pBoxData->topmagin>(-_HEIGHT(hWnd))+30)
					{
						key_step +=5;
						pBoxData->topmagin -= key_step;
						SGL_UpdateWindow(hWnd);
						SGL_NotifyParent(hWnd, SMP_BOXI_MOUSEMOVE, 0);
					}
					return 1;
				}
			case MR_KEY_8://键盘模拟，向下滑动
				{
					if(pBoxData->topmagin<_HEIGHT(hWnd)-30)
					{
						key_step +=5;
						pBoxData->topmagin += key_step;
						SGL_UpdateWindow(hWnd);
						SGL_NotifyParent(hWnd, SMP_BOXI_MOUSEMOVE, 0);
					}
					return 1;
				}
			}
			return 0;
		}

	case WM_KEYUP:
		{
			/*选中*/
			if(wParam == MR_KEY_SELECT)
			{
				SGL_NotifyParent(hWnd, SMP_BOXI_SELECTED, pBoxData->highlight);
				return 1;
			}

			//位置复原
			if (!pBoxData->leftmagin|| !pBoxData->topmagin)
				SMP_Box_RecoverLoaction(hWnd);

			key_step = 0;
		}

// 	case WM_COMMAND:
// 		{
// 			WORD id = LOWORD(wParam);
// 			WORD code = HIWORD(wParam);
// 			switch(id)
// 			{
// 				case WM_COMMAND:
// 					{
// 						if (code == WM_COMMAND)
// 						{
// 							//
// 						}
// 						break;
// 					}
// 			}
// 			return 0;
// 		}
	case WM_SETFOCUS:
    case WM_KILLFOCUS:
		{
			SGL_UpdateWindow(hWnd);
			break;
		}	
	}

	return 0;
}