
#include <memory>

#include "ayarabu.h"

#include "win_filesystem.h"
#include "win_text.h"
#include "text_utility.h"
#include "json_minimal.h"

namespace ayarabu
{
	struct StoryDatum
	{
		std::wstring wstrName;
		std::wstring wstrText;
	};

	/*
	* "voiceFormatId",
	* "directoryPath",
	* "assetBundleName",
	* "assetDataName"
	*/
	static std::vector<std::vector<std::string>> g_formatData;
	static std::wstring g_wstrStillFolderPath;
	static std::wstring g_wstrVideoFolderPath;
	static std::wstring g_wstrVoiceFolderPath;

	static unsigned long ToUInt32(const char* src)
	{
		unsigned long ul = 0;
		const unsigned char* p = reinterpret_cast<const unsigned char*>(src);
		ul = p[3];
		ul <<= 8;
		ul |= p[2];
		ul <<= 8;
		ul |= p[1];
		ul <<= 8;
		ul |= p[0];
		return ul;
	}

	/*脚本ファイル読み取り*/
	void ReadScript(const std::wstring &wstrFilePath, std::vector<StoryDatum>& storyData)
	{
		std::string strFile = win_filesystem::LoadFileAsString(wstrFilePath.c_str());

		constexpr int kTextOffset = 0x10;
		if (strFile.size() <= kTextOffset + 4ULL)return;

		unsigned long ulPos = ToUInt32(&strFile[kTextOffset]);
		std::wstring wstrText = win_text::WidenUtf8(strFile.substr(ulPos));

		std::vector<size_t> counts;
		std::vector<std::wstring> texts;
		std::wstring wstrTemp;
		size_t nCount = 0;
		for (size_t nRead = 0; nRead < wstrText.size(); ++nRead)
		{
			if (wstrText.at(nRead) == L'\0')
			{
				++nCount;
				if (!wstrTemp.empty())
				{
					texts.push_back(wstrTemp);
					wstrTemp.clear();
				}
				continue;
			}

			wstrTemp.push_back(wstrText.at(nRead));
			if (nCount > 0)
			{
				counts.push_back(nCount);
			}
			nCount = 0;
		}

		if (nCount > 0)
		{
			counts.push_back(nCount);
		}
		if (!wstrTemp.empty())
		{
			texts.push_back(wstrTemp);
		}

		constexpr int iNarattiveCounts = 4;

		StoryDatum storyDatumBuffer;
		bool bNarrative = !texts.empty() && texts.at(0).size() > 6;
		if (bNarrative)
		{
			for (size_t i = 0; i < counts.size() && i < texts.size(); ++i)
			{
				if (i == 0)
				{
					bNarrative = counts.at(i) >= iNarattiveCounts;
				}
				else
				{
					bNarrative = counts.at(i) > iNarattiveCounts;
				}
				if (bNarrative)
				{
					storyDatumBuffer.wstrText = texts.at(i);
					storyData.push_back(storyDatumBuffer);
					storyDatumBuffer = StoryDatum{};
					continue;
				}

				if (storyDatumBuffer.wstrName.empty())
				{
					storyDatumBuffer.wstrName = texts.at(i);
				}
				else
				{
					storyDatumBuffer.wstrText = texts.at(i);
					storyData.push_back(storyDatumBuffer);
					storyDatumBuffer = StoryDatum{};
				}
			}
		}
		else
		{
			for (size_t i = 0; i < counts.size() && i < texts.size(); ++i)
			{
				if (bNarrative)
				{
					storyDatumBuffer.wstrText = texts.at(i);
					storyData.push_back(storyDatumBuffer);
					storyDatumBuffer = StoryDatum{};
					if (counts.at(i) <= iNarattiveCounts)
					{
						bNarrative = false;
					}
					continue;
				}

				if (storyDatumBuffer.wstrName.empty())
				{
					storyDatumBuffer.wstrName = texts.at(i);
				}
				else
				{
					storyDatumBuffer.wstrText = texts.at(i);
					storyData.push_back(storyDatumBuffer);
					storyDatumBuffer = StoryDatum{};

					if (counts.at(i) > iNarattiveCounts)
					{
						bNarrative = true;
					}
				}
			}
		}

		for (auto& storyDatum : storyData)
		{
			text_utility::ReplaceAll(storyDatum.wstrText, L"<name></name>", L"陰陽師");
			text_utility::ReplaceAll(storyDatum.wstrText, L"$n", L"\n");
		}
	}
	/*音声ファイル名称書式表構築*/
	void SetupVoiceFileNameFormatInfo(const std::wstring& wstrFilePath)
	{
		std::string strFile = win_filesystem::LoadFileAsString(wstrFilePath.c_str());
		if (strFile.empty())return;
		text_utility::ReplaceAll(strFile, "\t", "");
		text_utility::ReplaceAll(strFile, "\n", "");

		char* p = &strFile[0];
		auto pp = std::make_unique<char*>();
		/*Actually masterData is not JSON, but partially can be regared as JSON.*/
		bool bRet = json_minimal::ExtractJsonArray(&p, nullptr, &*pp);
		if (!bRet)return;

		std::vector<std::string> lists;
		p = *pp + 1;
		for (;;)
		{
			pp = std::make_unique<char*>();
			bool bRet = json_minimal::ExtractJsonArray(&p, nullptr, &*pp);
			if (!bRet)break;

			lists.push_back(*pp);
		}

		std::vector<std::vector<std::string>> formatData;
		std::vector<char> vBuffer(512, '\0');
		for (auto& list : lists)
		{
			std::vector<std::string> formatDatum;
			p = &list[0];
			for (;;)
			{
				bRet = json_minimal::ReadNextArrayValue(&p, vBuffer.data(), vBuffer.size());
				if (!bRet)break;
				formatDatum.push_back(vBuffer.data());
			}

			formatData.push_back(formatDatum);
		}

		g_formatData = formatData;
	}
	/*音声ファイル名称書式探索*/
	std::string FindVoiceFileFormat(const std::string& strKey)
	{
		for (size_t i = 0; i < g_formatData.size(); ++i)
		{
			if (g_formatData.at(i).at(0) == strKey)
			{
				return g_formatData.at(i).at(1);
			}
		}
		return std::string();
	}

	std::wstring DeriveSoundMasterDataPathFromScriptFilePath(const std::wstring& wstrFilePath)
	{
		size_t nPos = wstrFilePath.rfind(L"r18");
		if (nPos == std::wstring::npos)return std::wstring();

		return wstrFilePath.substr(0, nPos) + LR"(mock\master_data\sound\SoundVoiceFormatMasterDatas.any)";
	}
	/*脚本ファイル経路=>静画階層*/
	std::wstring DeriveStillImageFolderPathFromScriptFilePath(const std::wstring& wstrFilePath)
	{
		size_t nPos = wstrFilePath.rfind(L"eventdata\\eventdata");
		if (nPos == std::wstring::npos)return std::wstring();

		return wstrFilePath.substr(0, nPos) + LR"(advstill)";
	}
	/*脚本ファイル経路=>動画階層*/
	std::wstring DeriveVideoFolderPathFromScriptFilePath(const std::wstring& wstrFilePath)
	{
		size_t nPos = wstrFilePath.rfind(L"adventure");
		if (nPos == std::wstring::npos)return std::wstring();

		return wstrFilePath.substr(0, nPos) + LR"(movie\harem)";
	}
	
	std::wstring DeriveVoiceFolderPathFromScriptFilePath(const std::wstring& wstrFilePath)
	{
		size_t nPos = wstrFilePath.rfind(L"adventure");
		if (nPos == std::wstring::npos)return std::wstring();

		return wstrFilePath.substr(0, nPos) + LR"(sound\voice)";
	}
	/*基底ID=>書式ID*/
	std::string BaseIdToFormatId(long long llBaseId)
	{
		return std::to_string(llBaseId) + "10001";
	}
	/*基底ID=>静画・動画ID*/
	std::wstring BaseIdToStillOrVideoId(long long llBaseId)
	{
		wchar_t swzBuffer[5]{};
		swprintf_s(swzBuffer, L"%04lld", llBaseId);
		return swzBuffer;
	}
	/*脚本ファイル名から基底ID抽出*/
	long long ExtractIdFromScriptFileName(const std::wstring& wstrFilePath)
	{
		constexpr wchar_t swzStart[] = L"eventdata";
		constexpr wchar_t swzEnd[] = L"03.evsc";

		size_t nPos1 = wstrFilePath.rfind(swzStart);
		size_t nPos2 = wstrFilePath.rfind(swzEnd);
		if (nPos1 == std::wstring::npos || nPos2 == std::wstring::npos)return -1;

		nPos1 += sizeof(swzStart) / sizeof(wchar_t) - 1;

		std::wstring wstrId = wstrFilePath.substr(nPos1, nPos2 - nPos1);
		long long llBaseId = wcstol(wstrId.c_str(), nullptr, 10);
		return llBaseId;
	}
}

bool ayarabu::LoadScenario(const std::wstring& wstrFilePath, std::vector<adv::TextDatum>& textData, std::vector<adv::PaintDatum>& paintData)
{
	/*初期作成*/
	if (g_formatData.empty())
	{
		std::wstring wstrVoiceMasterDataFilePath = DeriveSoundMasterDataPathFromScriptFilePath(wstrFilePath);
		if (wstrVoiceMasterDataFilePath.empty())return false;

		SetupVoiceFileNameFormatInfo(wstrVoiceMasterDataFilePath);
		if (g_formatData.empty())return false;

		g_wstrStillFolderPath = DeriveStillImageFolderPathFromScriptFilePath(wstrFilePath);
		g_wstrVideoFolderPath = DeriveVideoFolderPathFromScriptFilePath(wstrFilePath);
		g_wstrVoiceFolderPath = DeriveVoiceFolderPathFromScriptFilePath(wstrFilePath);
	}

	if (g_wstrStillFolderPath.empty() || g_wstrVideoFolderPath.empty() ||
		g_wstrVoiceFolderPath.empty() || g_formatData.empty())return false;

	long long llBaseId = ExtractIdFromScriptFileName(wstrFilePath);
	if (llBaseId <= 0)return false;

	/*文章・音声対応作成*/

	std::vector<StoryDatum> storyData;
	ReadScript(wstrFilePath, storyData);
	if (storyData.empty())return false;

	std::string strFormatId = BaseIdToFormatId(llBaseId);
	std::string strFilePathFormat = FindVoiceFileFormat(strFormatId);
	if (strFilePathFormat.empty())return false;

	text_utility::ReplaceAll(strFilePathFormat, "ep{1}", "ep");

	std::vector<std::wstring> voiceFilePaths;
	std::wstring wstrFolderPath = g_wstrVoiceFolderPath + L"\\" +  win_text::WidenUtf8(strFilePathFormat) + L"3";
	win_filesystem::CreateFilePathList(wstrFolderPath.c_str(), L".m4a", voiceFilePaths);
	size_t nIntroVoiceFileCount = voiceFilePaths.size();
	wstrFolderPath.back() = L'4';
	win_filesystem::CreateFilePathList(wstrFolderPath.c_str(), L".m4a", voiceFilePaths);

	const auto FindMainCharacterName = [&storyData]()
		-> std::wstring
		{
			for (long long i = storyData.size() - 1; i >= 0; --i)
			{
				if (!storyData.at(i).wstrName.empty() && storyData.at(i).wstrName.find(L"雄二") == std::wstring::npos)
				{
					return storyData.at(i).wstrName;
				}
			}
			return std::wstring();
		};

	std::wstring wstrMainCharacterName = FindMainCharacterName();

	size_t nFilePathIndex = voiceFilePaths.size() -1;
	for (long long i = storyData.size() - 1; i >= 0; --i)
	{
		StoryDatum& storyDatum = storyData.at(i);
		adv::TextDatum textDatum;
		if (!storyDatum.wstrName.empty())
		{
			textDatum.wstrText = storyDatum.wstrName;
			textDatum.wstrText += L": ";
		}
		textDatum.wstrText += storyDatum.wstrText;
		if (storyDatum.wstrName == wstrMainCharacterName)
		{
			if (nFilePathIndex < voiceFilePaths.size())
			{
				textDatum.wstrVoicePath = voiceFilePaths.at(nFilePathIndex);
				--nFilePathIndex;
			}
		}
		textData.push_back(textDatum);

		if (nFilePathIndex < nIntroVoiceFileCount)
		{
			break;
		}
	}

	std::reverse(textData.begin(), textData.end());

	/*静画・動画探索*/

	std::wstring wstrImageId = BaseIdToStillOrVideoId(llBaseId);

	std::vector<std::wstring> stillImageFilePaths;
	wstrFolderPath = g_wstrStillFolderPath + L"\\advstill" + wstrImageId;
	win_filesystem::CreateFilePathList(wstrFolderPath.c_str(), L".png", stillImageFilePaths);

	stillImageFilePaths.erase(std::remove_if(stillImageFilePaths.begin(), stillImageFilePaths.end(),
		[](const std::wstring &wstr)
		-> bool
		{
			return wstr.find(L'#') != std::wstring::npos;
		}), stillImageFilePaths.end());

	std::vector<std::wstring> videoFilePaths;
	wstrFolderPath = g_wstrVideoFolderPath + L"\\chara" + wstrImageId;
	win_filesystem::CreateFilePathList(wstrFolderPath.c_str(), L".mp4", videoFilePaths);

	/*
	* 総数 | 配分
	* 旧版 215以前
	*   2  | 1: 1
	*   3  | 2: 1
	*   4  | 3: 1
	* 新版 216以降
	*   2  | 1: 1
	*   3  | 1: 2
	*   4  | 2: 2
	*/
	if (stillImageFilePaths.size() > 1)
	{
		if (llBaseId < 216)
		{
			for (size_t i = 0; i < stillImageFilePaths.size() - 1; ++i)
			{
				adv::PaintDatum imageDatum;
				imageDatum.bIsVideo = false;
				imageDatum.wstrFilePath = stillImageFilePaths.at(i);
				paintData.push_back(imageDatum);
			}

			for (const auto& path : videoFilePaths)
			{
				adv::PaintDatum imageDatum;
				imageDatum.bIsVideo = true;
				imageDatum.wstrFilePath = path;
				paintData.push_back(imageDatum);
			}

			paintData.emplace_back(adv::PaintDatum{ false, stillImageFilePaths.back() });
		}
		else
		{
			for (size_t i = 0; i < stillImageFilePaths.size()/2; ++i)
			{
				adv::PaintDatum imageDatum;
				imageDatum.bIsVideo = false;
				imageDatum.wstrFilePath = stillImageFilePaths.at(i);
				paintData.push_back(imageDatum);
			}

			for (const auto& path : videoFilePaths)
			{
				adv::PaintDatum imageDatum;
				imageDatum.bIsVideo = true;
				imageDatum.wstrFilePath = path;
				paintData.push_back(imageDatum);
			}

			for (size_t i = stillImageFilePaths.size() / 2; i < stillImageFilePaths.size(); ++i)
			{
				adv::PaintDatum imageDatum;
				imageDatum.bIsVideo = false;
				imageDatum.wstrFilePath = stillImageFilePaths.at(i);
				paintData.push_back(imageDatum);
			}
		}
	}

	return !textData.empty() && !paintData.empty();
}
