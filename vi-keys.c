/* キーバインドを変更します。
*
* 無変換 => 左クリック
* 変換   => 右クリック
* Ctrl + Shift + H  => マウスカーソル左移動 ←
* Ctrl + Shift + J  => マウスカーソル下移動 ↓
* Ctrl + Shift + K  => マウスカーソル上移動 ↑
* Ctrl + Shift + L  => マウスカーソル右移動 →
*
* Ctrl + H  => VK_LEFT   (左カーソル) ←
* Ctrl + J  => VK_DOWN   (下カーソル) ↓
* Ctrl + K  => VK_UP     (上カーソル) ↑
* Ctrl + L  => VK_RIGHT  (右カーソル) →
* Ctrl + D  => VK_DELETE (削除)
* Ctrl + Q  => VK_ESCAPE (Esc)
* Ctrl + Shift + SPACE  => VK_BACK (バックスペース)
*
* Shift + 無変換  =>  VK_LWIN (左Windowsキー)
* Shift + 変換    =>  VK_LWIN + VK_TAB (タスクビューの表示)
*
* ## ビルド・コマンド ##
*
* cl.exe /O2 /GS- /source-charset:utf-8 vi-keys.c kernel32.lib user32.lib /link /NODEFAULTLIB /STACK:4096
*
* ## スタートアップへの登録 ##
*
* スタートアップの場所
* C:\Users\[ユーザー名]\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup
* 
* 通常は[ファイル名を指定して実行]で以下の内容を実行すれば
* スタートアップ・フォルダーがエクスプローラーで開きます。
* 
* "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup"
*
*/

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#include <Windows.h>

#define MOUSE_MOVE_DX      8
#define MOUSE_MOVE_DY      8
#define MOUSE_MOVE_ACCEL   1
#define MOUSE_MOVE_MAX    16
#define MKEY_NONE       0x00
#define MKEY_LSHIFT     0x01
#define MKEY_RSHIFT     0x02
#define MKEY_LCONTROL   0x04
#define MKEY_RCONTROL   0x08
#define MKEY_RELEASE    0x80
#define MKEY_IGNORE     0x40
#define EXTRA_TEMPORARY 0x80008001
#define WM_APP_RESTORE  (WM_APP + 1)

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
WORD  update_modifiers();
BOOL  keydown(DWORD vkCode, DWORD dwFlags, DWORD time, DWORD dwExtraInfo);
BOOL  keyup(DWORD vkCode, DWORD dwFlags, DWORD time, DWORD dwExtraInfo);
void  send_key_input(WORD vkCode, WORD modifiers, DWORD dwFlags, DWORD time, DWORD dwExtraInfo);
void  set_key_input(INPUT* input, WORD vkCode, DWORD dwFlags, DWORD time, ULONG_PTR dwExtraInfo);
void  restore_temporary_keys();
void  send_mouse_input(DWORD dwFlags, DWORD time, LONG dx, LONG dy, DWORD mouseData);

HWND  hwnd = NULL;
HHOOK hLowLevelKeyboardProc = NULL;

volatile BOOL  IS_LSHIFT_DOWN = FALSE;
volatile BOOL  IS_RSHIFT_DOWN = FALSE;
volatile BOOL  IS_LCONTROL_DOWN = FALSE;
volatile BOOL  IS_RCONTROL_DOWN = FALSE;
volatile BOOL  IS_LSHIFT_TEMPORARY_UP = FALSE;
volatile BOOL  IS_RSHIFT_TEMPORARY_UP = FALSE;
volatile BOOL  IS_LCONTROL_TEMPORARY_UP = FALSE;
volatile BOOL  IS_RCONTROL_TEMPORARY_UP = FALSE;
WORD  REPLACED_KEY[256];
WORD  RESTORE_TEMPRARY_KEY[256];
LONG  mouse_move_accel = 0;

#ifdef _DEBUG
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
void WinMainCRTStartup()
#endif
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	HANDLE mutex;
	WNDCLASSEX wc;
	BOOL ret;
	MSG  msg;
	UINT i;

	mutex = CreateMutex(NULL, FALSE, "VI_KEYS");
	if (mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		ExitProcess(1);
	}

	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

	for (i = 0; i < 256; i++)
	{
		REPLACED_KEY[i] = 0;
		RESTORE_TEMPRARY_KEY[i] = 0;
	}

	hLowLevelKeyboardProc = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
	if (hLowLevelKeyboardProc == NULL)
	{
		//ERROR
		ExitProcess(1);
	}

	SecureZeroMemory(&wc, sizeof(WNDCLASSEX));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = (WNDPROC)WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "VI_KEYS";

	if (RegisterClassEx(&wc))
	{
		hwnd = CreateWindowEx(0, wc.lpszClassName, NULL, 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
	}
	if (hwnd == NULL)
	{
		//ERROR
		ExitProcess(1);
	}

	while (ret = GetMessage(&msg, NULL, 0, 0))
	{
		if (ret == -1)
		{
			msg.wParam = 0;
			break;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (hLowLevelKeyboardProc != NULL)
	{
		ret = UnhookWindowsHookEx(hLowLevelKeyboardProc);
		if (ret == 0)
		{
			//ERROR
		}
	}

	ExitProcess(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_APP_RESTORE:
		Sleep(5);
		restore_temporary_keys();
		break;

	case WM_QUERYENDSESSION:
		return TRUE;

	case WM_ENDSESSION:
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		LPKBDLLHOOKSTRUCT pkbd = (LPKBDLLHOOKSTRUCT)lParam;
		BOOL is_temporary = pkbd->dwExtraInfo & EXTRA_TEMPORARY;
		switch (wParam)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (pkbd->vkCode == VK_LSHIFT)
			{
				IS_LSHIFT_TEMPORARY_UP = FALSE;
				if (IS_LSHIFT_DOWN)
				{
					return TRUE;
				}
				IS_LSHIFT_DOWN = TRUE;
			}
			else if (pkbd->vkCode == VK_RSHIFT)
			{
				IS_RSHIFT_TEMPORARY_UP = FALSE;
				if (IS_RSHIFT_DOWN)
				{
					return TRUE;
				}
			}
			else if (pkbd->vkCode == VK_LCONTROL)
			{
				IS_LCONTROL_TEMPORARY_UP = FALSE;
				if (IS_LCONTROL_DOWN)
				{
					return TRUE;
				}
				IS_LCONTROL_DOWN = TRUE;
			}
			else if (pkbd->vkCode == VK_RCONTROL)
			{
				IS_RCONTROL_TEMPORARY_UP = FALSE;
				if (IS_RCONTROL_DOWN)
				{
					return TRUE;
				}
				IS_RCONTROL_DOWN = TRUE;
			}
			else if (keydown(pkbd->vkCode, (pkbd->flags & ~KEYEVENTF_EXTENDEDKEY), pkbd->time, pkbd->dwExtraInfo))
			{
				return TRUE;
			}
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (pkbd->vkCode == VK_LSHIFT)
			{
				IS_LSHIFT_TEMPORARY_UP = is_temporary;
				IS_LSHIFT_DOWN = FALSE;
			}
			else if (pkbd->vkCode == VK_RSHIFT)
			{
				IS_RSHIFT_TEMPORARY_UP = is_temporary;
				IS_RSHIFT_DOWN = FALSE;
			}
			else if (pkbd->vkCode == VK_LCONTROL)
			{
				IS_LCONTROL_TEMPORARY_UP = is_temporary;
				IS_LCONTROL_DOWN = FALSE;
			}
			else if (pkbd->vkCode == VK_RCONTROL)
			{
				IS_RCONTROL_TEMPORARY_UP = is_temporary;
				IS_RCONTROL_DOWN = FALSE;
			}
			else if (keyup(pkbd->vkCode, ((pkbd->flags | KEYEVENTF_KEYUP) & ~KEYEVENTF_EXTENDEDKEY), pkbd->time, pkbd->dwExtraInfo))
			{
				return TRUE;
			}
			break;
		}
	}
	return CallNextHookEx(hLowLevelKeyboardProc, nCode, wParam, lParam);
}

WORD update_modifiers()
{
	IS_LSHIFT_DOWN = GetAsyncKeyState(VK_LSHIFT) & 0x8000;
	IS_RSHIFT_DOWN = GetAsyncKeyState(VK_RSHIFT) & 0x8000;
	IS_LCONTROL_DOWN = GetAsyncKeyState(VK_LCONTROL) & 0x8000;
	IS_RCONTROL_DOWN = GetAsyncKeyState(VK_RCONTROL) & 0x8000;

	return (IS_LSHIFT_DOWN ? MKEY_LSHIFT : 0)
		 | (IS_RSHIFT_DOWN ? MKEY_RSHIFT : 0)
		 | (IS_LCONTROL_DOWN ? MKEY_LCONTROL : 0)
		 | (IS_RCONTROL_DOWN ? MKEY_RCONTROL : 0);
}

BOOL keydown(DWORD vkCode, DWORD dwFlags, DWORD time, DWORD dwExtraInfo)
{
	WORD modifiers = update_modifiers();

	if (modifiers == MKEY_NONE)
	{
		if (vkCode == VK_NONCONVERT)
		{
			if (!REPLACED_KEY[VK_NONCONVERT])
			{
				REPLACED_KEY[VK_NONCONVERT] = VK_LBUTTON;
				send_mouse_input(MOUSEEVENTF_LEFTDOWN, time, 0, 0, 0);
			}
		}
		else if (vkCode == VK_CONVERT)
		{
			if (!REPLACED_KEY[VK_CONVERT])
			{
				REPLACED_KEY[VK_CONVERT] = VK_RBUTTON;
				send_mouse_input(MOUSEEVENTF_RIGHTDOWN, time, 0, 0, 0);
			}
		}
		else
		{
			return FALSE;
		}
	}
	else if (modifiers == (MKEY_LCONTROL | MKEY_LSHIFT))
	{
		if (vkCode == 'H')
		{
			send_mouse_input(MOUSEEVENTF_MOVE, time, -(MOUSE_MOVE_DX + mouse_move_accel), 0, 0);
			mouse_move_accel += (mouse_move_accel < MOUSE_MOVE_MAX) ? MOUSE_MOVE_ACCEL : 0;
		}
		else if (vkCode == 'J')
		{
			send_mouse_input(MOUSEEVENTF_MOVE, time, 0, +(MOUSE_MOVE_DY + mouse_move_accel), 0);
			mouse_move_accel += (mouse_move_accel < MOUSE_MOVE_MAX) ? MOUSE_MOVE_ACCEL : 0;
		}
		else if (vkCode == 'K')
		{
			send_mouse_input(MOUSEEVENTF_MOVE, time, 0, -(MOUSE_MOVE_DY + mouse_move_accel), 0);
			mouse_move_accel += (mouse_move_accel < MOUSE_MOVE_MAX) ? MOUSE_MOVE_ACCEL : 0;
		}
		else if (vkCode == 'L')
		{
			send_mouse_input(MOUSEEVENTF_MOVE, time, +(MOUSE_MOVE_DX + mouse_move_accel), 0, 0);
			mouse_move_accel += (mouse_move_accel < MOUSE_MOVE_MAX) ? MOUSE_MOVE_ACCEL : 0;
		}
		else if (vkCode == VK_SPACE)
		{
			REPLACED_KEY[VK_SPACE] = VK_BACK;
			send_key_input(VK_BACK, MKEY_NONE, dwFlags, time, dwExtraInfo);
		}
		else
		{
			return FALSE;
		}
	}
	else if (modifiers == MKEY_LCONTROL)
	{
		if (vkCode == 'H')
		{
			REPLACED_KEY['H'] = VK_LEFT;
			send_key_input(VK_LEFT, MKEY_NONE, dwFlags, time, dwExtraInfo);
		}
		else if (vkCode == 'J')
		{
			REPLACED_KEY['J'] = VK_DOWN;
			send_key_input(VK_DOWN, MKEY_NONE, dwFlags, time, dwExtraInfo);
		}
		else if (vkCode == 'K')
		{
			REPLACED_KEY['K'] = VK_UP;
			send_key_input(VK_UP, MKEY_NONE, dwFlags, time, dwExtraInfo);
		}
		else if (vkCode == 'L')
		{
			REPLACED_KEY['L'] = VK_RIGHT;
			send_key_input(VK_RIGHT, MKEY_NONE, dwFlags, time, dwExtraInfo);
		}
		else if (vkCode == 'D')
		{
			REPLACED_KEY['D'] = VK_DELETE;
			send_key_input(VK_DELETE, MKEY_NONE, dwFlags, time, dwExtraInfo);
		}
		else if (vkCode == 'Q')
		{
			REPLACED_KEY['Q'] = VK_ESCAPE;
			send_key_input(VK_ESCAPE, MKEY_NONE, dwFlags, time, dwExtraInfo);
		}
		else
		{
			return FALSE;
		}
	}
	else if (modifiers == MKEY_LSHIFT)
	{
		if (vkCode == VK_NONCONVERT)
		{
			if (!REPLACED_KEY[VK_NONCONVERT])
			{
				REPLACED_KEY[VK_NONCONVERT] = VK_LWIN;
				RESTORE_TEMPRARY_KEY[VK_LWIN] = TRUE;
				send_key_input(VK_LWIN, MKEY_RELEASE, dwFlags, time, dwExtraInfo);
			}
		}
		else if (vkCode == VK_CONVERT)
		{
			if (!REPLACED_KEY[VK_CONVERT])
			{
				REPLACED_KEY[VK_CONVERT] = VK_TAB;
				{
					UINT i = 0;
					INPUT input[5];
					set_key_input(&input[i++], VK_LSHIFT, KEYEVENTF_KEYUP, 0, 0);
					set_key_input(&input[i++], VK_LWIN, 0, 0, 0);
					set_key_input(&input[i++], VK_TAB, 0, 0, 0);
					set_key_input(&input[i++], VK_LWIN, KEYEVENTF_KEYUP, 0, 0);
					set_key_input(&input[i++], VK_LSHIFT, 0, 0, 0);
					SendInput(i, input, sizeof(INPUT));
				}
			}
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

BOOL keyup(DWORD vkCode, DWORD dwFlags, DWORD time, DWORD dwExtraInfo)
{
	if (vkCode == 'H' || vkCode == 'J' || vkCode == 'K' || vkCode == 'L')
	{
		mouse_move_accel = 0;
	}

	if (REPLACED_KEY[vkCode])
	{
		if (REPLACED_KEY[vkCode] == VK_LBUTTON)
		{
			send_mouse_input(MOUSEEVENTF_LEFTUP, time, 0, 0, 0);
		}
		else if (REPLACED_KEY[vkCode] == VK_RBUTTON)
		{
			send_mouse_input(MOUSEEVENTF_RIGHTUP, time, 0, 0, 0);
		}
		else
		{
			send_key_input(REPLACED_KEY[vkCode], 0, dwFlags, time, dwExtraInfo);
		}
		REPLACED_KEY[vkCode] = 0;
		return TRUE;
	}
	if (RESTORE_TEMPRARY_KEY[vkCode])
	{
		RESTORE_TEMPRARY_KEY[vkCode] = 0;
		PostMessage(hwnd, WM_APP_RESTORE, 0, 0);
	}

	return FALSE;
}

void send_key_input(WORD vkCode, WORD modifiers, DWORD dwFlags, DWORD time, DWORD dwExtraInfo)
{
	UINT i = 0;
	INPUT input[9];

	if ((modifiers & MKEY_IGNORE) == 0)
	{
		if (IS_LSHIFT_DOWN && vkCode != VK_LSHIFT && (modifiers & MKEY_LSHIFT) == 0)
		{
			set_key_input(&input[i++], VK_LSHIFT, KEYEVENTF_KEYUP, time, EXTRA_TEMPORARY);
		}
		if (IS_RSHIFT_DOWN && vkCode != VK_RSHIFT && (modifiers & MKEY_RSHIFT) == 0)
		{
			set_key_input(&input[i++], VK_RSHIFT, KEYEVENTF_KEYUP, time, EXTRA_TEMPORARY);
		}
		if (IS_LCONTROL_DOWN && vkCode != VK_LCONTROL && (modifiers & MKEY_LCONTROL) == 0)
		{
			set_key_input(&input[i++], VK_LCONTROL, KEYEVENTF_KEYUP, time, EXTRA_TEMPORARY);
		}
		if (IS_RCONTROL_DOWN && vkCode != VK_RCONTROL && (modifiers & MKEY_RCONTROL) == 0)
		{
			set_key_input(&input[i++], VK_RCONTROL, KEYEVENTF_KEYUP, time, EXTRA_TEMPORARY);
		}
	}

	set_key_input(&input[i++], vkCode, dwFlags, time, 0);

	if ((modifiers & MKEY_IGNORE) == 0 && (modifiers & MKEY_RELEASE) == 0)
	{
		if (IS_RCONTROL_DOWN && vkCode != VK_RCONTROL && (modifiers & MKEY_RCONTROL) == 0)
		{
			set_key_input(&input[i++], VK_RCONTROL, 0, time, EXTRA_TEMPORARY);
		}
		if (IS_LCONTROL_DOWN && vkCode != VK_LCONTROL && (modifiers & MKEY_LCONTROL) == 0)
		{
			set_key_input(&input[i++], VK_LCONTROL, 0, time, EXTRA_TEMPORARY);
		}
		if (IS_RSHIFT_DOWN && vkCode != VK_RSHIFT && (modifiers & MKEY_RSHIFT) == 0)
		{
			set_key_input(&input[i++], VK_RSHIFT, 0, time, EXTRA_TEMPORARY);
		}
		if (IS_LSHIFT_DOWN && vkCode != VK_LSHIFT && (modifiers & MKEY_LSHIFT) == 0)
		{
			set_key_input(&input[i++], VK_LSHIFT, 0, time, EXTRA_TEMPORARY);
		}
	}

	SendInput(i, input, sizeof(INPUT));
}

void set_key_input(INPUT* input, WORD vkCode, DWORD dwFlags, DWORD time, ULONG_PTR dwExtraInfo)
{
	switch (vkCode)
	{
	case VK_LWIN:
	case VK_CANCEL:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_END:
	case VK_HOME:
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN:
	case VK_SNAPSHOT:
	case VK_INSERT:
	case VK_DELETE:
	case VK_DIVIDE:
	case VK_NUMLOCK:
	case VK_RSHIFT:
	case VK_RCONTROL:
	case VK_RMENU:
		dwFlags |= KEYEVENTF_EXTENDEDKEY;
	}

	input->type = INPUT_KEYBOARD;
	input->ki.dwFlags = dwFlags;
	input->ki.wVk = vkCode;
	input->ki.wScan = (WORD)MapVirtualKey(vkCode, 0);
	input->ki.time = time;
	input->ki.dwExtraInfo = dwExtraInfo;
}

void restore_temporary_keys()
{
	UINT i = 0;
	INPUT input[4];

	if (IS_LSHIFT_TEMPORARY_UP)
	{
		set_key_input(&input[i++], VK_LSHIFT, 0, 0, 0);
	}
	if (IS_RSHIFT_TEMPORARY_UP)
	{
		set_key_input(&input[i++], VK_RSHIFT, 0, 0, 0);
	}
	if (IS_LCONTROL_TEMPORARY_UP)
	{
		set_key_input(&input[i++], VK_LCONTROL, 0, 0, 0);
	}
	if (IS_RCONTROL_TEMPORARY_UP)
	{
		set_key_input(&input[i++], VK_RCONTROL, 0, 0, 0);
	}
	if (i > 0)
	{
		SendInput(i, input, sizeof(INPUT));
	}
}

void send_mouse_input(DWORD dwFlags, DWORD time, LONG dx, LONG dy, DWORD mouseData)
{
	INPUT input[1];

	input[0].type = INPUT_MOUSE;
	input[0].mi.dx = dx;
	input[0].mi.dy = dy;
	input[0].mi.mouseData = mouseData;
	input[0].mi.dwFlags = dwFlags;
	input[0].mi.time = time;
	input[0].mi.dwExtraInfo = 0;

	SendInput(1, input, sizeof(INPUT));
}
