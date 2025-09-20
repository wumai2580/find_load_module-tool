// Windows GUI with blue theme, marquee animation and copyable output
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <string>
#include <sstream>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

#include "base_func.h"
#include "analyze_kernel.h"
#include "kernel_version_parser.h"

static const wchar_t* kTitle = L"查找内核 load_module 工具";
static const UINT WM_APP_WORK_DONE = WM_APP + 1;

#ifndef PBM_SETSTATE
#define PBM_SETSTATE (WM_USER + 16)
#define PBST_NORMAL 0x0001
#define PBST_ERROR  0x0002
#define PBST_PAUSED 0x0003
#endif

struct WorkResult {
	std::wstring text;
	std::wstring offsetHex;
};

struct AppUI {
    HWND hwnd = nullptr;
    HWND hBtnOpen = nullptr;
    HWND hBtnCopy = nullptr;
    HWND hProgress = nullptr;
    HWND hEdit = nullptr;
    HFONT hFont = nullptr;
};

static AppUI g_ui;
static std::wstring g_lastOffset;
static volatile LONG g_statusState = 0; // 0 idle, 1 working, 2 done

static void AppendText(HWND hEdit, const std::wstring& t) {
	int len = GetWindowTextLengthW(hEdit);
	SendMessageW(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
	SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)t.c_str());
}

static void StartMarquee(HWND hProgress) {
	SendMessageW(hProgress, PBM_SETMARQUEE, TRUE, 20);
}

static void StopMarquee(HWND hProgress) {
	SendMessageW(hProgress, PBM_SETMARQUEE, FALSE, 0);
}

static void SetProgressDoneGreen(HWND hProgress) {
    
    SendMessageW(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(hProgress, PBM_SETPOS, 100, 0);
    SendMessageW(hProgress, PBM_SETSTATE, PBST_NORMAL, 0);
}

static DWORD WINAPI WorkerThread(LPVOID lp) {
	std::wstring* pPath = (std::wstring*)lp;
	std::wstring path = *pPath;
	delete pPath;

	WorkResult* res = new WorkResult();

    std::vector<char> buf = read_file_buf_w(path);
    std::wstringstream ws;
    ws << L"本工具由GRIEFREDD重新编译\r\n";
    ws << L"用于查找aarch64 Linux内核中load_module的位置\r\n";
    ws << L"==============================\r\n";
	if (buf.empty()) {
		ws << L"无法打开文件（路径可能无效或权限不足）\r\n";
		res->text = ws.str();
		PostMessageW(g_ui.hwnd, WM_APP_WORK_DONE, 0, (LPARAM)res);
		return 0;
	}
    ws << L"文件大小: " << (unsigned long long)buf.size() << L" 字节\r\n";
    {
        KernelVersionParser ver(buf);
        std::string verStr = ver.find_kernel_versions();
        if (!verStr.empty()) {
            std::wstring wver(verStr.begin(), verStr.end());
            ws << L"找到当前的Linux内核版本: " << wver << L"\r\n";
            if (verStr.size() && (verStr[0] == '5' || verStr[0] == '6')) {
                ws << L"无签名特征\r\n";
                res->text = ws.str();
                PostMessageW(g_ui.hwnd, WM_APP_WORK_DONE, 0, (LPARAM)res);
                return 0;
            }
        }
    }
    ws << L"开始解析内核，可能需要几秒，请耐心等待...\r\n";

	KernelVersionParser ver(buf);
	std::string verStr = ver.find_kernel_versions();
	if (!verStr.empty()) {
		std::wstring wver(verStr.begin(), verStr.end());
		ws << L"找到当前的Linux内核版本: " << wver << L"\r\n";
	}

	AnalyzeKernel analyzer(buf);
	if (!analyzer.analyze_kernel_symbol()) {
		ws << L"解析内核符号失败\r\n";
		res->text = ws.str();
		PostMessageW(g_ui.hwnd, WM_APP_WORK_DONE, 0, (LPARAM)res);
		return 0;
	}
	KernelSymbolOffset sym = analyzer.get_symbol_offset();
	{
		std::wstringstream hex;
		hex << L"0x" << std::hex << sym.load_module_offset;
		res->offsetHex = hex.str();
		ws << L"加载模块偏移量: " << res->offsetHex << L"\r\n";
	}

	res->text = ws.str();
	PostMessageW(g_ui.hwnd, WM_APP_WORK_DONE, 0, (LPARAM)res);
	return 0;
}

static void BeginProcess(const std::wstring& filePath) {
	EnableWindow(g_ui.hBtnOpen, FALSE);
	EnableWindow(g_ui.hBtnCopy, FALSE);
	StartMarquee(g_ui.hProgress);
    AppendText(g_ui.hEdit, L"开始处理...\r\n\r\n");
	std::wstring* p = new std::wstring(filePath);
	CloseHandle(CreateThread(nullptr, 0, WorkerThread, p, 0, nullptr));
}

static void OnDropFiles(HWND hwnd, HDROP hDrop) {
	wchar_t path[MAX_PATH] = {0};
	if (DragQueryFileW(hDrop, 0, path, MAX_PATH) > 0) {
		BeginProcess(path);
	}
	DragFinish(hDrop);
}

static void OnChooseFile(HWND hwnd) {
	OPENFILENAMEW ofn = {0};
	wchar_t file[MAX_PATH] = {0};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = file;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = L"All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
	if (GetOpenFileNameW(&ofn)) {
		BeginProcess(file);
	}
}

static void CopyTextToClipboard(HWND hwnd, const std::wstring& text) {
    if (text.empty()) return;
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
	if (OpenClipboard(hwnd)) {
		EmptyClipboard();
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (hMem) {
			void* dst = GlobalLock(hMem);
            memcpy(dst, text.c_str(), bytes);
			GlobalUnlock(hMem);
			SetClipboardData(CF_UNICODETEXT, hMem);
		}
		CloseClipboard();
	}
}

static HBRUSH g_headerBrush = nullptr;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
    case WM_CREATE: {
		g_ui.hwnd = hwnd;
		InitCommonControls();
		g_headerBrush = CreateSolidBrush(RGB(33,150,243)); 
		DragAcceptFiles(hwnd, TRUE);
		
		g_ui.hFont = CreateFontW(
			-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");

		g_ui.hBtnOpen = CreateWindowW(L"BUTTON", L"选择文件", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
			10, 58, 90, 28, hwnd, (HMENU)1, GetModuleHandleW(nullptr), nullptr);
		g_ui.hBtnCopy = CreateWindowW(L"BUTTON", L"复制偏移量", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
			320, 58, 90, 28, hwnd, (HMENU)2, GetModuleHandleW(nullptr), nullptr);
		g_ui.hProgress = CreateWindowW(PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_MARQUEE,
			170, 60, 140, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        // 去除“等待文件”标签
		g_ui.hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
			10, 95, 400, 180, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        // 应用字体
		SendMessageW(g_ui.hBtnOpen, WM_SETFONT, (WPARAM)g_ui.hFont, TRUE);
		SendMessageW(g_ui.hBtnCopy, WM_SETFONT, (WPARAM)g_ui.hFont, TRUE);
		SendMessageW(g_ui.hEdit, WM_SETFONT, (WPARAM)g_ui.hFont, TRUE);
        // 尝试开启系统暗角效果（更自然的阴影/圆角）
        BOOL val = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &val, sizeof(val));
        MARGINS m = { -1 };
        DwmExtendFrameIntoClientArea(hwnd, &m);
		return 0;
	}
    case WM_ERASEBKGND: {
		RECT rc; GetClientRect(hwnd, &rc);
		HDC hdc = (HDC)wParam;
		RECT hdr = { rc.left, rc.top, rc.right, rc.top + 50 };
		FillRect(hdc, &hdr, g_headerBrush);
		RECT body = { rc.left, hdr.bottom, rc.right, rc.bottom };
		FillRect(hdc, &body, (HBRUSH)(COLOR_WINDOW + 1));
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, RGB(255,255,255));
        LPCWSTR title = kTitle;
		DrawTextW(hdc, title, -1, &hdr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
		return 1;
	}
   
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->hwndItem == g_ui.hBtnOpen || dis->hwndItem == g_ui.hBtnCopy) {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            BOOL disabled = (dis->itemState & ODS_DISABLED);
            BOOL pressed = (dis->itemState & ODS_SELECTED);
            HBRUSH bg = (HBRUSH)(COLOR_WINDOW + 1);
            FillRect(hdc, &rc, bg);
            RECT rShadow = rc; OffsetRect(&rShadow, 2, 2);
            HBRUSH hShadow = CreateSolidBrush(RGB(220,220,220));
            RoundRect(hdc, rShadow.left, rShadow.top, rShadow.right, rShadow.bottom, 12, 12);
            DeleteObject(hShadow);
            COLORREF btnColor = disabled ? RGB(180, 180, 180) : RGB(33,150,243);
            HBRUSH hBtn = CreateSolidBrush(btnColor);
            HPEN hPen = CreatePen(PS_SOLID, 1, disabled ? RGB(200,200,200) : RGB(20,130,220));
            HGDIOBJ oldB = SelectObject(hdc, hBtn);
            HGDIOBJ oldP = SelectObject(hdc, hPen);
            RECT rBtn = rc; if (pressed) OffsetRect(&rBtn, 1, 1);
            RoundRect(hdc, rBtn.left, rBtn.top, rBtn.right, rBtn.bottom, 12, 12);
            SelectObject(hdc, oldB); DeleteObject(hBtn);
            SelectObject(hdc, oldP); DeleteObject(hPen);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255,255,255));
            wchar_t text[64] = {0};
            GetWindowTextW(dis->hwndItem, text, 64);
            DrawTextW(hdc, text, -1, &rBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }
	case WM_DROPFILES:
		OnDropFiles(hwnd, (HDROP)wParam);
		return 0;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case 1: OnChooseFile(hwnd); break;
		case 2: CopyTextToClipboard(hwnd, g_lastOffset); break;
		}
		return 0;
	case WM_APP_WORK_DONE: {
		StopMarquee(g_ui.hProgress);
		EnableWindow(g_ui.hBtnOpen, TRUE);
		EnableWindow(g_ui.hBtnCopy, TRUE);
        g_statusState = 2;
		SetProgressDoneGreen(g_ui.hProgress);
		WorkResult* res = (WorkResult*)lParam;
		AppendText(g_ui.hEdit, res->text);
		g_lastOffset = res->offsetHex;
		delete res;
		return 0;
	}
	case WM_DESTROY:
		if (g_headerBrush) { DeleteObject(g_headerBrush); g_headerBrush = nullptr; }
		if (g_ui.hFont) { DeleteObject(g_ui.hFont); g_ui.hFont = nullptr; }
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
	INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_PROGRESS_CLASS };
	InitCommonControlsEx(&icc);

	const wchar_t* cls = L"FindLoadModuleGuiClass_v2";
	WNDCLASSW wc = {0};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = cls;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, cls, kTitle, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 420, 320, nullptr, nullptr, hInst, nullptr);
	if (!hwnd) return 0;
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	int wargc = 0;
	LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
	if (wargv && wargc >= 2) {
		BeginProcess(wargv[1]);
		LocalFree(wargv);
	}
	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return (int)msg.wParam;
}
#endif // _WIN32


