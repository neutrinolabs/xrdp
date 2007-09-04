
#include <windows.h>
#include <tchar.h>
#include "os_calls.h"
#include "arch.h"

static HINSTANCE g_instance = 0;
static HWND g_wnd = 0;
static char* g_font_name = "Tahoma";
static int g_font_size = 10;
static HFONT g_font = 0;

int
msg(char* msg1, ...)
{
  return 0;
}

/*****************************************************************************/
static int
font_dump(void)
{
  HDC dc;
  RECT rect;
  HBRUSH brush;
  HGDIOBJ saved;
  ABC abc;
  char filename[256];
  TCHAR text[256];
  char zero1;
  int fd;
  int x1;
  int strlen1;
  short x2;

  zero1 = 0;
  g_snprintf(filename, 255, "%s-%d.fv1", g_font_name, g_font_size);
  g_file_delete(filename);
  fd = g_file_open(filename);
  g_file_write(fd, "FNT1", 4);
  strlen1 = g_strlen(g_font_name);
  g_file_write(fd, g_font_name, strlen1);
  x1 = strlen1;
  while (x1 < 32)
  {
    g_file_write(fd, &zero1, 1);
    x1++;
  }
  x2 = g_font_size; /* font size */
  g_file_write(fd, (char*)&x2, 2);
  x2 = 1;
  g_file_write(fd, (char*)&x2, 2);
  for (x1 = 32; x1 < 1024; x1++)
  {
    dc = GetWindowDC(g_wnd);
    GetCharABCWidths(dc, x1, x1, &abc);
    ReleaseDC(g_wnd, dc);
    text[0] = ' ';
    text[1] = (TCHAR)x1;
    text[2] = ' ';
    dc = GetWindowDC(g_wnd);
    saved = SelectObject(dc, g_font);
    TextOut(dc, 50, 50, text, 3);
    SelectObject(dc, saved);
    ReleaseDC(g_wnd, dc);
    Sleep(10);
    dc = GetWindowDC(g_wnd);
    rect.left = 50;
    rect.top = 50;
    rect.right = 100;
    rect.bottom = 100;
    brush = CreateSolidBrush(RGB(0, 0, 255));
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
    ReleaseDC(g_wnd, dc);
  }
  g_file_close(fd);
  return 0;
}

/*****************************************************************************/
static LRESULT CALLBACK
wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  PAINTSTRUCT ps;
  HBRUSH brush;
  RECT rect;

  switch (message)
  {
    case WM_PAINT:
      BeginPaint(hWnd, &ps);
      brush = CreateSolidBrush(RGB(0, 0, 255));
      rect = ps.rcPaint;
      FillRect(ps.hdc, &rect, brush);
      DeleteObject(brush);
      EndPaint(hWnd, &ps);
      break;
    case WM_CLOSE:
      DestroyWindow(g_wnd);
      g_wnd = 0;
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_TIMER:
      KillTimer(g_wnd, 1);
      font_dump();
      break;
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

/*****************************************************************************/
static int
create_window(void)
{
  WNDCLASS wc;
  DWORD style;
  HDC dc;
  int height;

  ZeroMemory(&wc, sizeof(wc));
  wc.lpfnWndProc = wnd_proc; /* points to window procedure */
  /* name of window class */
  wc.lpszClassName = _T("fontdump");
  wc.hCursor = LoadCursor(0, IDC_ARROW);
  /* Register the window class. */
  if (!RegisterClass(&wc))
  {
    return 0; /* Failed to register window class */
  }
  style = WS_OVERLAPPED | WS_CAPTION | WS_POPUP | WS_MINIMIZEBOX |
          WS_SYSMENU | WS_SIZEBOX | WS_MAXIMIZEBOX;
  g_wnd = CreateWindow(wc.lpszClassName, _T("fontdump"),
                       style, 0, 0, 200, 200,
                       (HWND) NULL, (HMENU) NULL, g_instance,
                       (LPVOID) NULL);
  ShowWindow(g_wnd, SW_SHOWNORMAL);
  dc = GetDC(g_wnd);
  height = -MulDiv(g_font_size, GetDeviceCaps(dc, LOGPIXELSY), 72);
  g_font = CreateFontA(height, 0, 0, 0, FW_DONTCARE, 0, 0, 0, 0, 0, 0,
                       0, 0, g_font_name);
  if (g_font == 0)
  {
    MessageBox(g_wnd, _T("Font creation failed"), _T("error"), MB_OK);
  }
  ReleaseDC(g_wnd, dc);
  PostMessage(g_wnd, WM_SETFONT, (WPARAM)g_font, 0);
  SetTimer(g_wnd, 1, 1000, 0);
  return 0;
}

/*****************************************************************************/
static int
main_loop(void)
{
  MSG msg;

  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (int)(msg.wParam);
}

/*****************************************************************************/
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
        LPSTR lpCmdLine, int nCmdShow)
{
  g_instance = hInstance;
  create_window();
  return main_loop();
}
