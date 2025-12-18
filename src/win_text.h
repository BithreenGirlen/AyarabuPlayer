#ifndef WIN_TEXT_H_
#define WIN_TEXT_H_

#include <string>

namespace win_text
{
    std::wstring WidenUtf8(const std::string& str);
    std::wstring WidenUtf8(const char* str, int length);

    std::string NarrowUtf8(const std::wstring& wstr);
    std::string NarrowUtf8(const wchar_t* wstr, int length);

    std::wstring WidenANSI(const std::string& str);
    std::wstring WidenANSI(const char* str, int length);

    std::string NarrowANSI(const std::wstring& wstr);
    std::string NarrowANSI(const wchar_t* wstr, int length);
}

#endif //WIN_TEXT_H_
