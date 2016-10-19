/*
 * Copyright(c) 2016  Guillaume Knispel <xilun0@gmail.com>, Tim Felgentreff <timfelgentreff@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <Shlwapi.h>
#include <string>
#include <locale>
#include <codecvt>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define XMINGARGS ":0 -br -multiwindow -clipboard -compositewm -wgl -dpi [dpi]"
#define DEFAULT_X_PORT "6000"
WSADATA wsaData;
HWND Stealth;

void Win32_perror(const char* what)
{
	const int errnum = ::GetLastError();
	const bool what_present = (what && *what);

	WCHAR *str;
	DWORD nbWChars = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
									  | FORMAT_MESSAGE_FROM_SYSTEM
									  | FORMAT_MESSAGE_IGNORE_INSERTS
									  | FORMAT_MESSAGE_MAX_WIDTH_MASK,
									  nullptr, (DWORD)errnum, 0, (LPWSTR)&str,
									  0, nullptr);
	if (nbWChars == 0) {
		std::fprintf(stderr, "%s%swin32 error %d\n",
					 what_present ? what : "",
					 what_present ? ": " : "",
					 errnum);
	} else {
		std::fprintf(stderr, "%s%s%S\n",
					 what_present ? what : "",
					 what_present ? ": " : "",
					 str);
		::LocalFree(str);
	}
	::SetLastError(errnum);
}

class CCmdLine
{
public:
    CCmdLine()
      : m_bash_launcher(get_default_bash_launcher()),
        m_escaped_bash_cmd_line_params(),
		m_x_launcher(),
		m_x_args(),
        m_has_bash_exe_tilde(false),
		m_hide_console(false)
    {
        const wchar_t* p = get_cmd_line_params();

        while (1) {
            const wchar_t* new_p;
            std::wstring param = parse_argv_param(p, &new_p);
			if (param == L"--help") {
				std::fprintf(stdout,
							 "xbash, a launcher for WSL bash that starts an X server and propagates its DISPLAY to WSL.\n\n"
							 "The default X server is Xming, it is searched by its association to .xlaunch files. "
							 "If you use another X server or have not associated Xming with .xlaunch files, "
							 "you must pass the path for the X server to launch on display :0 (tcp port 6000).\n\n"
							 "The default arguments to Xming are \"" XMINGARGS "\". "
							 "You can override these, and if you have the \"[dpi]\" placeholder in your args, "
							 "this will be replaced with the dynamically determined dpi.\n\n"
							 "Arguments:\n"
							 "\t--xbash-launcher [path to bash executable]\n"
							 "\t--xbash-xserver [path to X server]\n"
							 "\t--xbash-xserver-args [commandlineargs for X server]\n"
							 "\t--xbash-hide-console\n"
							 "All other arguments are passed to the bash executable.\n\n");
				break;
			} else if (param.find(L"--xbash-") != 0) {
				break;
			} else if (param == L"--xbash-launcher") {
                m_bash_launcher = parse_argv_param(new_p, &new_p);
                if (m_bash_launcher.empty() || m_bash_launcher.find(L'"') != std::wstring::npos) {
                    std::fprintf(stderr, "xbash: invalid --xbash-launcher param %S\n", m_bash_launcher.c_str());
                    std::exit(1);
                }
			} else if (param == L"--xbash-hide-console") {
				m_hide_console = true;
			} else if (param == L"--xbash-xserver") {
				m_x_launcher = parse_argv_param(new_p, &new_p);
				if (m_x_launcher.empty() || m_bash_launcher.find(L'"') != std::wstring::npos) {
					std::fprintf(stderr, "xbash: invalid --xbash-xserver param %S\n", m_x_launcher.c_str());
					std::exit(1);
				}
			} else if (param == L"--xbash-xserver-args") {
				m_x_args = parse_argv_param(new_p, &new_p);
				if (m_x_args.empty()) {
					std::fprintf(stderr, "xbash: invalid --xbash-xserver-args param %S\n", m_x_args.c_str());
					std::exit(1);
				}
            } else {
                std::fprintf(stderr, "xbash: unknown %S param\n", param.c_str());
                std::exit(1);
            }
            p = new_p;
        }

        m_has_bash_exe_tilde = (p[0] == L'~')
            && (is_cmd_line_sep(p[1]) || !p[1]);
        if (m_has_bash_exe_tilde) {
            p++;
            while (is_cmd_line_sep(*p))
                p++;
        }

        m_escaped_bash_cmd_line_params = bash_escape_within_double_quotes(p);
    }

	int start_command(PROCESS_INFORMATION& out_pi)
	{
		const wchar_t* display = start_xserver();
		if (display == NULL) {
			std::exit(EXIT_FAILURE);
		}

		STARTUPINFO si = { 0 };
		std::wstring cmdline = L"\"" + m_bash_launcher +
								L"\" " + (m_has_bash_exe_tilde ? L"~ -c \"" : L"-c \"") +
								L"DISPLAY=" + display +
								L" exec bash " + m_escaped_bash_cmd_line_params + L"\"";

		if (CreateProcessW(NULL,
						   &cmdline[0],
						   NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &out_pi) == 0) {
			Win32_perror("xbash: CreateProcess");
			std::fprintf(stderr, "xbash: CreateProcess failed (%lu) for command: %S\n", ::GetLastError(), cmdline.c_str());
			return 1;
		}

		if (m_hide_console) {
			hide_console();
		}

		return 0;
	}

private:
	static void hide_console() {
		Sleep(3000);
		AllocConsole();
		Stealth = FindWindowA("ConsoleWindowClass", NULL);
		ShowWindow(Stealth, 0);
	}

	static bool is_listening_on_port(const char* port) {
		struct addrinfo *result = NULL, hints;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		// Resolve the local address and port to be used by the server
		int iResult = getaddrinfo(NULL, port, &hints, &result);
		if (iResult != 0) {
			Win32_perror("xbash: getaddrinfo failed");
			WSACleanup();
			std::exit(EXIT_FAILURE);
		}

		SOCKET ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {
			Win32_perror("xbash: error checking for running X server");
			freeaddrinfo(result);
			WSACleanup();
			std::exit(EXIT_FAILURE);
		}

		iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			if (WSAGetLastError() != WSAEADDRINUSE) {
				Win32_perror("xbash: checking if we can bind to the X server port failed");
				freeaddrinfo(result);
				closesocket(ListenSocket);
				WSACleanup();
				std::exit(EXIT_FAILURE);
			} else {
				std::fprintf(stdout, "xbash: Port 6000 already in use, not launching X\n");
				return true;
			}
		}
		closesocket(ListenSocket);
		freeaddrinfo(result);
		return false;
	}

	const wchar_t* start_xserver()
	{
		if (!is_listening_on_port(DEFAULT_X_PORT)) {
			STARTUPINFO si = { 0 };
			PROCESS_INFORMATION pi = { 0 };
			HDC screen;
			std::wstring cmdline;

			screen = GetDC(0);
			int dpi = GetDeviceCaps(screen, LOGPIXELSX); // Assuming square pixels
			ReleaseDC(0, screen);

			if (m_x_launcher.empty()) {
				cmdline = get_default_x_launcher() + L" ";
			} else {
				cmdline = m_x_launcher + L" ";
			}
			if (m_x_args.empty()) {
				cmdline += std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(XMINGARGS);
			} else {
				cmdline += m_x_args;
			}
			std::wstring wdpistr = std::to_wstring(dpi);
			std::wstring dpiplaceholder = L"[dpi]";
			for (size_t pos = 0; ; pos += wdpistr.length()) {
				pos = cmdline.find(dpiplaceholder, pos);
				if (pos == std::wstring::npos) break;
				cmdline.erase(pos, dpiplaceholder.length());
				cmdline.insert(pos, wdpistr);
			}

			si.cb = sizeof(si);
			if (CreateProcessW(NULL,
							   &cmdline[0],
							   NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi) == 0) {
				Win32_perror("xbash: Xming");
				std::fprintf(stderr, "xbash: CreateProcess failed (%d) for command: %S\n", ::GetLastError(), cmdline.c_str());
				return NULL;
			}
		}
		return L"localhost:0.0";
	}

	static bool is_cmd_line_sep(wchar_t c)
	{
		return c == L' ' || c == L'\t';
	}

    static std::wstring bash_escape_within_double_quotes(const wchar_t* p)
    {
        std::wstring result;
        while (*p) {
            if (*p == L'$' || *p == L'`' || *p == L'\\' || *p == L'"')
                result.push_back(L'\\');
            result.push_back(*p);
            p++;
        }
        return result;
    }

    static std::wstring parse_argv_param(const wchar_t* p, const wchar_t** next_p)
    {
        std::wstring result;
        bool quoted = false;
        while (true) {
            int backslashes = 0;
            while (*p == L'\\') {
                p++;
                backslashes++;
            }
            if (*p == L'"') {
                result.append(backslashes / 2, L'\\');
                if (backslashes % 2 == 0) {
                    p++;
                    if (!quoted || *p != L'"') {
                        quoted = !quoted;
                        continue; // while (true)
                    }
                }
            } else {
                result.append(backslashes, L'\\');
            }
            if (*p == L'\0' || (!quoted && is_cmd_line_sep(*p)))
                break;
            result.push_back(*p);
            p++;
        }
        while (is_cmd_line_sep(*p))
            p++;
        *next_p = p;
        return result;
    }

    static const wchar_t* get_cmd_line_params()
    {
        const wchar_t* p = GetCommandLineW();
        if (p == nullptr)
            return L"";
        // we use the same rules as the CRT parser to delimit argv[0]:
        for (bool quoted = false; *p != L'\0' && (quoted || !is_cmd_line_sep(*p)); p++) {
            if (*p == L'"')
                quoted = !quoted;
        }
        while (is_cmd_line_sep(*p))
            p++;
        return p; // pointer to the first param (if any) in the command line
    }

    static std::wstring get_default_bash_launcher()
    {
        wchar_t buf[MAX_PATH+1];
        UINT res = ::GetSystemDirectoryW(buf, MAX_PATH+1);
        if (res == 0 || res > MAX_PATH) { std::fprintf(stderr, "xbash: GetSystemDirectory error\n"); std::abort(); }
        return buf + std::wstring(L"\\bash.exe");
    }

	static std::wstring get_default_x_launcher() {
		wchar_t* szXmingPath = NULL;
		DWORD cbBufSize = 0;
		HRESULT hr = AssocQueryStringW(0, ASSOCSTR_EXECUTABLE, L".xlaunch", NULL, NULL, &cbBufSize);
		if (FAILED(hr)) return NULL;
		szXmingPath = new WCHAR[cbBufSize + 1];
		hr = AssocQueryStringW(0, ASSOCSTR_EXECUTABLE, L".xlaunch", NULL, szXmingPath, &cbBufSize);
		if (FAILED(hr)) {
			delete[] szXmingPath;
			return NULL;
		}
		PathRemoveFileSpec(szXmingPath);
		return std::wstring(szXmingPath) + L"\\Xming.exe";
	}

private:
    std::wstring    m_bash_launcher;
    std::wstring    m_escaped_bash_cmd_line_params;
	std::wstring    m_x_launcher;
	std::wstring    m_x_args;
    bool            m_has_bash_exe_tilde;
	bool            m_hide_console;
};

int main()
{
	PROCESS_INFORMATION pi = { 0 };
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		Win32_perror("Could not bring up winsock");
		std::exit(EXIT_FAILURE);
	}

	if (CCmdLine().start_command(pi) != 0) {
		std::exit(EXIT_FAILURE);
	}
    CloseHandle(pi.hThread);
	Sleep(2000);
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
    std::exit(EXIT_SUCCESS);
}
