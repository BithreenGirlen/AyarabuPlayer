
#include <shobjidl.h>
#include <atlbase.h>

#include "win_dialogue.h"


std::wstring win_dialogue::SelectFolder(const wchar_t* title, void* hOwnerWnd)
{
	CComPtr<IFileOpenDialog> pFolderDialog;
	HRESULT hr = pFolderDialog.CoCreateInstance(CLSID_FileOpenDialog);

	if (SUCCEEDED(hr))
	{
		FILEOPENDIALOGOPTIONS options{};
		pFolderDialog->GetOptions(&options);
		pFolderDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
		if (title != nullptr)pFolderDialog->SetTitle(title);

		if (SUCCEEDED(pFolderDialog->Show(static_cast<HWND>(hOwnerWnd))))
		{
			CComPtr<IShellItem> pSelectedItem;
			pFolderDialog->GetResult(&pSelectedItem);

			wchar_t* pPath = nullptr;
			pSelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath);

			if (pPath != nullptr)
			{
				std::wstring wstrPath = pPath;
				::CoTaskMemFree(pPath);

				return wstrPath;
			}
		}
	}

	return {};
}

std::wstring win_dialogue::SelectOpenFile(const wchar_t* fileType, const wchar_t* fileSpec, const wchar_t* title, void* hOwnerWnd, bool allowAll)
{
	CComPtr<IFileOpenDialog> pFileDialog;
	HRESULT hr = pFileDialog.CoCreateInstance(CLSID_FileOpenDialog);

	if (SUCCEEDED(hr))
	{
		COMDLG_FILTERSPEC filter[2]{};
		filter[0].pszName = fileType;
		filter[0].pszSpec = fileSpec;
		filter[1].pszName = L"All files";
		filter[1].pszSpec = L"*";
		hr = pFileDialog->SetFileTypes(allowAll ? 2 : 1, filter);
		if (SUCCEEDED(hr))
		{
			FILEOPENDIALOGOPTIONS options{};
			pFileDialog->GetOptions(&options);
			pFileDialog->SetOptions(options | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
			if (title != nullptr)pFileDialog->SetTitle(title);

			if (SUCCEEDED(pFileDialog->Show(static_cast<HWND>(hOwnerWnd))))
			{
				CComPtr<IShellItem> pSelectedItem;
				pFileDialog->GetResult(&pSelectedItem);

				wchar_t* pPath = nullptr;
				pSelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath);

				if (pPath != nullptr)
				{
					std::wstring wstrPath = pPath;
					::CoTaskMemFree(pPath);

					return wstrPath;
				}
			}
		}
	}

	return {};
}

std::wstring win_dialogue::SelectSaveFile(const wchar_t* fileType, const wchar_t* fileSpec, const wchar_t* defaultFileName, void* hOwnerWnd)
{
	CComPtr<IFileSaveDialog> pFileDialog;
	HRESULT hr = pFileDialog.CoCreateInstance(CLSID_FileSaveDialog);

	if (SUCCEEDED(hr))
	{
		COMDLG_FILTERSPEC filter[1]{};
		filter[0].pszName = fileType;
		filter[0].pszSpec = fileSpec;
		hr = pFileDialog->SetFileTypes(1, filter);
		if (SUCCEEDED(hr))
		{
			if (defaultFileName != nullptr)pFileDialog->SetFileName(defaultFileName);

			FILEOPENDIALOGOPTIONS options{};
			pFileDialog->GetOptions(&options);
			pFileDialog->SetOptions(options | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);

			if (SUCCEEDED(pFileDialog->Show(static_cast<HWND>(hOwnerWnd))))
			{
				CComPtr<IShellItem> pSelectedItem;
				pFileDialog->GetResult(&pSelectedItem);

				wchar_t* pPath = nullptr;
				pSelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath);

				if (pPath != nullptr)
				{
					std::wstring wstrPath = pPath;
					::CoTaskMemFree(pPath);

					return wstrPath;
				}
			}
		}
	}

	return {};
}
