#ifndef _SMP_STYLEBOX_H
#define _SMP_STYLEBOX_H

#include "window.h"


#define  SMP_BOXI_HILICHANGED	0x0001	//高亮改变

#define  SMP_BOXI_CLICKED		0x0002	//鼠标点击

#define  SMP_BOXI_SELECTED		0x0003	//选中

#define  SMP_BOXI_MOUSEMOVE		0x0004	//鼠标移动

/*获得高亮序号*/
int SMP_Box_GetHilightId(HWND hWnd);

/*设置信息*/
VOID SMP_Stylewnd_SetItem(HWND hWnd, const DWORD* bmps, int size);


LRESULT SMP_Box_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

#endif