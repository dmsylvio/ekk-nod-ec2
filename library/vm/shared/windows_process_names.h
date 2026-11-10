#ifndef EKK_WINDOWS_PROCESS_NAMES_H
#define EKK_WINDOWS_PROCESS_NAMES_H
// Include after Windows headers. VM process names are UTF-8 strings.
inline BOOL process_name_matches(const WCHAR* actual, const char* expected)
{
    if (!actual || !expected || !*expected) return 0;
    WCHAR converted[MAX_PATH]{};
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, expected, -1, converted, MAX_PATH)) return 0;
    return CompareStringOrdinal(actual, -1, converted, -1, TRUE) == CSTR_EQUAL;
}
#endif
