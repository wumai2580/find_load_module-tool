// patch_kernel_root.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "base_func.h"
#include "analyze_kernel.h"
#include "kernel_version_parser.h"
#include <cstring>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <conio.h>
#include <clocale>
#include <cwchar>
#endif

bool check_file_path(const char* file_path) {
	size_t len = strlen(file_path);
	if (len > 4 && strcmp(file_path + len - 4, ".img") == 0) {
		return false;
	}
	return true;
}

#ifdef _WIN32
static void set_console_utf8() {
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	setlocale(LC_ALL, "");
}

static void set_color(WORD attr) {
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (h != INVALID_HANDLE_VALUE) {
		SetConsoleTextAttribute(h, attr);
	}
}


static void print_w(const wchar_t* text) {
#ifdef _WIN32
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (h == INVALID_HANDLE_VALUE) {
		std::wcout << text << std::endl;
		return;
	}
	DWORD written = 0;
	WriteConsoleW(h, text, (DWORD)wcslen(text), &written, nullptr);
	WriteConsoleW(h, L"\r\n", 2, &written, nullptr);
#else
	std::wcout << text << std::endl;
#endif
}

static void pause_after_done() {
	print_w(L"查找已完成");
	_getch();
}
#else
static void pause_after_done() {
	print_w(L"查找已完成");
}
#endif

int main(int argc, char* argv[]) {

#ifdef _WIN32
    set_console_utf8();
    print_w(L"本工具由GRIEFREDD重新编译");
    print_w(L"用于查找aarch64 Linux内核中load_module的位置");
#else
    std::cout << "本工具由GRIEFREDD重新编译" << std::endl;
    std::cout << "用于查找aarch64 Linux内核中load_module的位置" << std::endl;
#endif

    std::string input_path;
    std::wstring wpath;
#ifdef _WIN32
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv && wargc >= 2) {
        wpath = wargv[1];
        LocalFree(wargv);
    }
    if (wpath.empty() && argc >= 2 && argv[1] && argv[1][0] != '\0') {
        int need = MultiByteToWideChar(CP_ACP, 0, argv[1], -1, nullptr, 0);
        if (need > 0) {
            std::wstring tmp(static_cast<size_t>(need), L'\0');
            MultiByteToWideChar(CP_ACP, 0, argv[1], -1, &tmp[0], need);
            if (!tmp.empty() && tmp.back() == L'\0') tmp.pop_back();
            wpath = std::move(tmp);
        }
    }
#endif

    if (
#ifdef _WIN32
        wpath.empty()
#else
        !(argc >= 2)
#endif
    ) {
        print_w(L"请将内核二进制文件拖到本程序上运行");
        pause_after_done();
        return 0;
    }
	
#ifdef _WIN32
    if (!wpath.empty()) {
        if (wpath.size() >= 4 && _wcsicmp(wpath.c_str() + (wpath.size() - 4), L".img") == 0) {
            print_w(L"请输入正确的 Linux 内核二进制文件路径");
            print_w(L"例如 boot.img 需先解包并提取内核文件");
            pause_after_done();
            return 0;
        }
    }
#else
    const char* file_path = (argc >= 2 ? argv[1] : "");
    if (!check_file_path(file_path)) {
        print_w(L"请输入正确的 Linux 内核二进制文件路径");
        print_w(L"例如 boot.img 需先解包并提取内核文件");
		pause_after_done();
		return 0;
	}
#endif
	
    std::vector<char> file_buf =
#ifdef _WIN32
        read_file_buf_w(wpath);
#else
        read_file_buf(file_path);
#endif
    if (!file_buf.size()) {
        print_w(L"无法打开文件（路径可能无效或权限不足）");
		pause_after_done();
		return 0;
	}

#ifdef _WIN32
    {
        std::wostringstream woss;
        woss << L"文件大小: " << (unsigned long long)file_buf.size() << L" 字节";
        print_w(woss.str().c_str());
    }
    {
        KernelVersionParser ver(file_buf);
        std::string verStr = ver.find_kernel_versions();
        if (!verStr.empty()) {
            std::wstring wver(verStr.begin(), verStr.end());
            std::wostringstream vout; vout << L"找到当前的Linux内核版本: " << wver;
            print_w(vout.str().c_str());
            if (verStr.size() && (verStr[0] == '5' || verStr[0] == '6')) {
                print_w(L"无签名特征");
                pause_after_done();
                return 0;
            }
        }
    }
    print_w(L"开始解析内核，可能需要几秒，请耐心等待...");
#else
    std::cout << "文件大小: " << file_buf.size() << " 字节" << std::endl;
    {
        KernelVersionParser ver(file_buf);
        std::string verStr = ver.find_kernel_versions();
        if (!verStr.empty()) {
            std::cout << "找到当前的Linux内核版本: " << verStr << std::endl;
            if (verStr.size() && (verStr[0] == '5' || verStr[0] == '6')) {
                std::cout << "无签名特征" << std::endl;
                return 0;
            }
        }
    }
    std::cout << "开始解析内核，可能需要几秒，请耐心等待..." << std::endl;
#endif

	AnalyzeKernel analyze_kernel(file_buf);
    if (!analyze_kernel.analyze_kernel_symbol()) {
        print_w(L"解析内核符号失败");
		pause_after_done();
		return 0;
	}
	KernelSymbolOffset sym = analyze_kernel.get_symbol_offset();

#ifdef _WIN32
	set_color(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
#endif
    {
        std::wostringstream woss;
        woss << L"加载模块偏移量: 0x" << std::hex << sym.load_module_offset;
        print_w(woss.str().c_str());
    }
#ifdef _WIN32
	set_color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
	pause_after_done();
	return 0;
}
