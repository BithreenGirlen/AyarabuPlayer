#ifndef WIN_FILESYSTEM_H_
#define WIN_FILESYSTEM_H_

#include <string>
#include <vector>

namespace win_filesystem
{
	bool CreateFilePathList(const wchar_t* folderPath, size_t folderPathLength, const wchar_t* fileSpec, std::vector<std::wstring>& paths, bool toAddParent = true);
	bool CreateFilePathList(const std::wstring& folderPath, const wchar_t* fileSpec, std::vector<std::wstring>& paths, bool toAddParent = true);
	bool GetFilePathListAndIndex(const std::wstring& path, const wchar_t* fileSpec, std::vector<std::wstring>& paths, size_t* nIndex);

	/// @brief Get the directory where the executable exists.
	/// @param length A pointer to receive length
	/// @return A pointer to static buffer
	const wchar_t* GetCurrentProcessPath(size_t* length);

	std::string LoadFileAsString(const wchar_t* filePath);
}
#endif // WIN_FILESYSTEM_H_
