/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <windows.h>
#include <tchar.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "os_calls.h"
#include "arch.h"

static HINSTANCE g_instance = 0;
static HWND g_wnd = 0;
static HWND g_lb = 0;
static HWND g_exit_button = 0;
static HWND g_go_button = 0;
static HWND g_font_list = 0;
static char g_font_name[512] = "";
static int g_font_size = 10;
static HFONT g_font = 0;
static int g_running = 0;

#define FONT_DATASIZE(_w, _h) (((_h * ((_w + 7) / 8)) + 3) & ~3)

/*****************************************************************************/
int
check_messages(void)
{
    MSG msg;

    while (PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
    {
        GetMessage(&msg, NULL, 0, 0);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

/*****************************************************************************/
static int
msg(char *msg1, ...)
{
    va_list ap;
    char text1[512];

    va_start(ap, msg1);
    vsnprintf(text1, 511, msg1, ap);
    SendMessageA(g_lb, LB_ADDSTRING, 0, (LPARAM)text1);
    va_end(ap);
    return 0;
}

/*****************************************************************************/
static int
show_last_error(void)
{
    LPVOID lpMsgBuf;

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   NULL, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPSTR)&lpMsgBuf, 0, NULL);
    msg("GetLastError - %s", lpMsgBuf);
    LocalFree(lpMsgBuf);
    return 0;
}

/*****************************************************************************/
static int
font_dump(void)
{
    HDC dc;
    HDC dc1;
    RECT rect;
    HBRUSH brush;
    HGDIOBJ saved;
    HBITMAP bitmap;
    BITMAPINFO bi;
    char *bits;
    ABC abc;
    SIZE sz;
    char filename[256];
    TCHAR text[256];
    char zero1;
    char *bmtext;
    int bmtextindex;
    int fd;
    int x1;
    int strlen1;
    int index1;
    int index2;
    int len;
    int pixel;
    int red;
    int green;
    int blue;
    int width;
    int height;
    int roller;
    int outlen;
    tui8 b1;
    short x2;

    if (g_running)
    {
        return 0;
    }

    g_running = 1;
    msg("starting");
    g_font_name[0] = 0;
    SendMessageA(g_font_list, WM_GETTEXT, 255, (LPARAM)g_font_name);

    if (g_strlen(g_font_name) == 0)
    {
        msg("error font not set");
        g_running = 0;
        return 1;
    }

    dc = GetDC(g_wnd);
    height = -MulDiv(g_font_size, GetDeviceCaps(dc, LOGPIXELSY), 72);
    g_font = CreateFontA(height, 0, 0, 0, FW_DONTCARE, 0, 0, 0, 0, 0, 0,
                         0, 0, g_font_name);
    ReleaseDC(g_wnd, dc);

    if (g_font == 0)
    {
        msg("error - Font creation failed");
    }

    zero1 = 0;
    g_snprintf(filename, 255, "%s-%d.fv1", g_font_name, g_font_size);
    msg("creating file %s", filename);
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
    g_file_write(fd, (char *)&x2, 2);
    x2 = 1; /* style */
    g_file_write(fd, (char *)&x2, 2);
    /* pad */
    index1 = 0;

    while (index1 < 8)
    {
        g_file_write(fd, &zero1, 1);
        index1++;
    }

    for (x1 = 32; x1 < 0x4e00; x1++)
    {
        check_messages();
        dc = GetWindowDC(g_wnd);
        saved = SelectObject(dc, g_font);

        if (!GetCharABCWidths(dc, x1, x1, &abc))
        {
            show_last_error();
        }

        text[0] = (TCHAR)x1;
        text[1] = 0;

        if (!GetTextExtentPoint32(dc, text, 1, &sz))
        {
            show_last_error();
        }

        SelectObject(dc, saved);
        ReleaseDC(g_wnd, dc);

        if ((sz.cx > 0) && (sz.cy > 0))
        {
            dc = GetWindowDC(g_wnd);
            saved = SelectObject(dc, g_font);
            SetBkColor(dc, RGB(255, 255, 255));

            if (!ExtTextOut(dc, 50, 50, ETO_OPAQUE, 0, text, 1, 0))
            {
                show_last_error();
            }

            SelectObject(dc, saved);
            ReleaseDC(g_wnd, dc);
            Sleep(10);
            /* width */
            x2 = abc.abcB;
            g_file_write(fd, (char *)&x2, 2);
            /* height */
            x2 = sz.cy;
            g_file_write(fd, (char *)&x2, 2);
            /* baseline */
            x2 = -sz.cy;
            g_file_write(fd, (char *)&x2, 2);
            /* offset */
            x2 = abc.abcA;
            g_file_write(fd, (char *)&x2, 2);
            /* incby */
            x2 = sz.cx;
            g_file_write(fd, (char *)&x2, 2);
            /* pad */
            index1 = 0;

            while (index1 < 6)
            {
                g_file_write(fd, &zero1, 1);
                index1++;
            }

            dc = GetWindowDC(g_wnd);
            rect.left = 50 + abc.abcA;
            rect.top = 50;
            rect.right = rect.left + abc.abcB;
            rect.bottom = rect.top + sz.cy;
            memset(&bi, 0, sizeof(bi));
            width = (abc.abcB + 7) & (~7);
            height = sz.cy;
            bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
            bi.bmiHeader.biWidth = width;
            bi.bmiHeader.biHeight = height;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bitmap = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void *)&bits, 0, 0);

            if (bitmap == 0)
            {
                msg("error - CreateDIBSection failed");
            }
            else
            {
                memset(bits, 0, width * height * 4);
                dc1 = CreateCompatibleDC(dc);
                SelectObject(dc1, bitmap);

                if (!BitBlt(dc1, 0, 0, width, height, dc, rect.left, rect.top, SRCCOPY))
                {
                    show_last_error();
                }

                bmtext = (char *)g_malloc(width * height + 16, 1);
                bmtextindex = 0;

                for (index1 = (height - 1); index1 >= 0; index1--)
                {
                    for (index2 = 0; index2 < width; index2++)
                    {
                        pixel = ((int *)bits)[index1 * width + index2];
                        red = (pixel >> 16) & 0xff;
                        green = (pixel >> 8) & 0xff;
                        blue = (pixel >> 0) & 0xff;

                        if (red == 0 && green == 0 && blue == 0)
                        {
                            bmtext[bmtextindex] = '1';
                            bmtextindex++;
                        }
                        else
                        {
                            bmtext[bmtextindex] = '0';
                            bmtextindex++;
                        }
                    }
                }

                outlen = 0;
                b1 = 0;
                roller = 0;
                len = g_strlen(bmtext);

                for (index2 = 0; index2 < len; index2++)
                {
                    if (bmtext[index2] == '1')
                    {
                        switch (roller)
                        {
                            case 0:
                                b1 = b1 | 0x80;
                                break;
                            case 1:
                                b1 = b1 | 0x40;
                                break;
                            case 2:
                                b1 = b1 | 0x20;
                                break;
                            case 3:
                                b1 = b1 | 0x10;
                                break;
                            case 4:
                                b1 = b1 | 0x08;
                                break;
                            case 5:
                                b1 = b1 | 0x04;
                                break;
                            case 6:
                                b1 = b1 | 0x02;
                                break;
                            case 7:
                                b1 = b1 | 0x01;
                                break;
                        }
                    }

                    roller++;

                    if (roller == 8)
                    {
                        roller = 0;
                        g_file_write(fd, &b1, 1);
                        outlen++;
                        b1 = 0;
                    }
                }

                while ((outlen % 4) != 0)
                {
                    g_file_write(fd, &zero1, 1);
                    outlen++;
                }

                free(bmtext);
                DeleteDC(dc1);
                DeleteObject(bitmap);
            }

            if (sz.cx != (long)(abc.abcA + abc.abcB + abc.abcC))
            {
                msg("error - width not right 1");
            }

            brush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
            ReleaseDC(g_wnd, dc);
        }
        else
        {
            /* write out a blank glyph here */
            /* width */
            x2 = 1;
            g_file_write(fd, (char *)&x2, 2);
            /* height */
            x2 = 1;
            g_file_write(fd, (char *)&x2, 2);
            /* baseline */
            x2 = 0;
            g_file_write(fd, (char *)&x2, 2);
            /* offset */
            x2 = 0;
            g_file_write(fd, (char *)&x2, 2);
            /* incby */
            x2 = 1;
            g_file_write(fd, (char *)&x2, 2);
            /* pad */
            index1 = 0;

            while (index1 < 6)
            {
                g_file_write(fd, &zero1, 1);
                index1++;
            }

            /* blank bitmap */
            index1 = 0;

            while (index1 < 4)
            {
                g_file_write(fd, &zero1, 1);
                index1++;
            }
        }
    }

    g_file_close(fd);
    msg("done");
    g_running = 0;
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
            brush = CreateSolidBrush(RGB(255, 255, 255));
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
        case WM_COMMAND:

            if ((HWND)lParam == g_exit_button)
            {
                PostMessage(g_wnd, WM_CLOSE, 0, 0);
            }
            else if ((HWND)lParam == g_go_button)
            {
                while (SendMessage(g_lb, LB_GETCOUNT, 0, 0) > 0)
                {
                    SendMessage(g_lb, LB_DELETESTRING, 0, 0);
                }

                SetTimer(g_wnd, 1, 1000, 0);
            }

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
    int left;
    int top;

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
    left = GetSystemMetrics(SM_CXSCREEN) / 2 - 640 / 2;
    top = GetSystemMetrics(SM_CYSCREEN) / 2 - 480 / 2;
    g_wnd = CreateWindow(wc.lpszClassName, _T("fontdump"),
                         style, left, top, 640, 480,
                         (HWND) NULL, (HMENU) NULL, g_instance,
                         (LPVOID) NULL);
    style = WS_CHILD | WS_VISIBLE | WS_BORDER;
    g_lb = CreateWindow(_T("LISTBOX"), _T("LISTBOX1"), style,
                        200, 10, 400, 400, g_wnd, 0, g_instance, 0);
    style = WS_CHILD | WS_VISIBLE;
    g_exit_button = CreateWindow(_T("BUTTON"), _T("Exit"), style,
                                 540, 410, 75, 25, g_wnd, 0, g_instance, 0);
    g_go_button = CreateWindow(_T("BUTTON"), _T("Go"), style,
                               440, 410, 75, 25, g_wnd, 0, g_instance, 0);
    style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWN;
    g_font_list = CreateWindow(_T("COMBOBOX"), _T("COMBOBOX1"), style,
                               50, 250, 125, 125, g_wnd, 0, g_instance, 0);
    ShowWindow(g_wnd, SW_SHOWNORMAL);
    PostMessage(g_wnd, WM_SETFONT, (WPARAM)g_font, 0);
    SendMessageA(g_font_list, CB_ADDSTRING, 0, (LPARAM)"Tahoma");
    SendMessageA(g_font_list, CB_ADDSTRING, 0, (LPARAM)"DejaVu Serif");
    SendMessageA(g_font_list, CB_ADDSTRING, 0, (LPARAM)"DejaVu Sans");
    SendMessageA(g_font_list, CB_ADDSTRING, 0, (LPARAM)"Arial");
    SendMessageA(g_font_list, CB_ADDSTRING, 0, (LPARAM)"Comic Sans MS");
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
