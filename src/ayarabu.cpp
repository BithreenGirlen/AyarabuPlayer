

#include "ayarabu.h"

#include "win_filesystem.h"
#include "win_text.h"
#include "text_utility.h"
#include "json_minimal.h"

/* 内部用 */
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
	static void ReadScript(const std::wstring& wstrFilePath, std::vector<StoryDatum>& storyData)
	{
		std::string strFile = win_filesystem::LoadFileAsString(wstrFilePath.c_str());

		constexpr int kTextOffset = 0x10;
		if (strFile.size() <= kTextOffset + 4ULL)return;

		unsigned long ulPos = ToUInt32(&strFile[kTextOffset]);
		std::wstring wstrText = win_text::WidenUtf8(&strFile[ulPos], static_cast<int>(strFile.size() - ulPos));

		std::vector<size_t> counts;
		std::vector<std::wstring> texts;
		std::wstring wstrTemp;
		size_t nCount = 0;
		for (size_t nRead = 0; nRead < wstrText.size(); ++nRead)
		{
			if (wstrText[nRead] == L'\0')
			{
				++nCount;
				if (!wstrTemp.empty())
				{
					texts.push_back(wstrTemp);
					wstrTemp.clear();
				}
				continue;
			}

			wstrTemp.push_back(wstrText[nRead]);
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
		bool bNarrative = !texts.empty() && texts[0].size() > 6;
		if (bNarrative)
		{
			for (size_t i = 0; i < counts.size() && i < texts.size(); ++i)
			{
				if (i == 0)
				{
					bNarrative = counts[i] >= iNarattiveCounts;
				}
				else
				{
					bNarrative = counts[i] > iNarattiveCounts;
				}
				if (bNarrative)
				{
					storyDatumBuffer.wstrText = texts[i];
					storyData.push_back(storyDatumBuffer);
					storyDatumBuffer = StoryDatum{};
					continue;
				}

				if (storyDatumBuffer.wstrName.empty())
				{
					storyDatumBuffer.wstrName = texts[i];
				}
				else
				{
					storyDatumBuffer.wstrText = texts[i];
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
					storyDatumBuffer.wstrText = texts[i];
					storyData.push_back(storyDatumBuffer);
					storyDatumBuffer = StoryDatum{};
					if (counts[i] <= iNarattiveCounts)
					{
						bNarrative = false;
					}
					continue;
				}

				if (storyDatumBuffer.wstrName.empty())
				{
					storyDatumBuffer.wstrName = texts[i];
				}
				else
				{
					storyDatumBuffer.wstrText = texts[i];
					storyData.push_back(storyDatumBuffer);
					storyDatumBuffer = StoryDatum{};

					if (counts[i] > iNarattiveCounts)
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
	static void SetupVoiceFileNameFormatInfo(const std::wstring& wstrFilePath)
	{
		const std::string strFile = win_filesystem::LoadFileAsString(wstrFilePath.c_str());
		if (strFile.empty())return;

		const char* p = &strFile[0];
		const char* pStart = nullptr, * pEnd = nullptr;
		/*
		* Actually, masterData is not JSON, but partially can be regarded as JSON
		* in that (1) it represents nest structure with '[' and ']',
		* (2) quotes string using '"', and (3) represents array with ','.
		* Or might be regarded as CSV having replaced new line with '[' and ']'.
		* Anyway, trying to "parse" it will fail because it is not valid JSON as a whole.
		* What is done here is to "extract" a part of it without checking its validity as a whole.
		*/
		bool bRet = json_minimal::FindNextArray(&p, nullptr, &pStart, &pEnd);
		if (!bRet)return;

		std::vector<std::string> lists;
		p = pStart + 1;
		for (;;)
		{
			bRet = json_minimal::FindNextArray(&p, nullptr, &pStart, &pEnd);
			if (!bRet)break;

			lists.emplace_back(pStart, pEnd);
		}
		std::vector<std::vector<std::string>> formatData;
		for (const auto& list : lists)
		{
			std::vector<std::string> formatDatum;
			p = &list[0];
			for (;;)
			{
				bRet = json_minimal::util::ReadNextValueInArray(&p, &pStart, &pEnd);
				if (!bRet)break;

				formatDatum.emplace_back(pStart, pEnd);
			}
			formatData.push_back(std::move(formatDatum));
		}

		g_formatData = std::move(formatData);
	}
	/*音声ファイル名称書式探索*/
	static const std::string FindVoiceFileFormat(const std::string& strKey)
	{
		for (size_t i = 0; i < g_formatData.size(); ++i)
		{
			if (g_formatData[i].size() > 2 && g_formatData[i][0] == strKey)
			{
				return g_formatData[i][1];
			}
		}
		return std::string{};
	}

	static std::wstring DeriveSoundMasterDataPathFromScriptFilePath(const std::wstring& wstrFilePath)
	{
		size_t nPos = wstrFilePath.rfind(L"r18");
		if (nPos == std::wstring::npos)return std::wstring();

		return wstrFilePath.substr(0, nPos).append(LR"(mock\master_data\sound\SoundVoiceFormatMasterDatas.any)");
	}
	/* 脚本ファイル経路から各種資源階層導出 */
	static bool DeriveResourceFolderPathsFromScriptFilePath(const std::wstring& wstrFilePath)
	{
		size_t nAdvPos = wstrFilePath.rfind(L"adventure");
		if (nAdvPos == std::wstring::npos)return false;

		size_t nEventDataPos = wstrFilePath.rfind(L"eventdata\\eventdata");
		if (nEventDataPos == std::wstring::npos)return false;

		g_wstrStillFolderPath.assign(&wstrFilePath[0], nEventDataPos).append(LR"(advstill)");

		g_wstrVideoFolderPath.assign(&wstrFilePath[0], nAdvPos).append(LR"(movie\harem)");
		g_wstrVoiceFolderPath.assign(&wstrFilePath[0], nAdvPos).append(LR"(sound\voice)");

		return !g_wstrStillFolderPath.empty() && !g_wstrVideoFolderPath.empty() && !g_wstrVoiceFolderPath.empty();
	}
	/*基底ID=>書式ID*/
	static std::string BaseIdToFormatId(long long llBaseId)
	{
		return std::to_string(llBaseId).append("10001");
	}
	/*基底ID=>静画・動画ID*/
	static std::wstring BaseIdToStillOrVideoId(long long llBaseId)
	{
		wchar_t swzBuffer[5]{};
		swprintf_s(swzBuffer, L"%04lld", llBaseId);
		return swzBuffer;
	}
	/*脚本ファイル名から基底ID抽出*/
	static long long ExtractIdFromScriptFileName(const std::wstring& wstrFilePath)
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
	/* 音声ファイル探索 */
	static size_t FindVoiceFiles(long long baseId, std::vector<std::wstring>& voiceFilePaths)
	{
		std::string strFormatId = BaseIdToFormatId(baseId);
		const std::string& strFilePathFormat = FindVoiceFileFormat(strFormatId);
		if (strFilePathFormat.empty())return false;

		size_t nFormatPos = strFilePathFormat.find("{1}");
		if (nFormatPos == std::string::npos)nFormatPos = strFilePathFormat.size();

		std::wstring wstrFolderPath = g_wstrVoiceFolderPath;
		wstrFolderPath.append(L"\\").append(win_text::WidenUtf8(&strFilePathFormat[0], static_cast<int>(nFormatPos))).push_back('3');
		win_filesystem::CreateFilePathList(wstrFolderPath.c_str(), L".m4a", voiceFilePaths);

		size_t nIntroVoiceFileCount = voiceFilePaths.size();
		wstrFolderPath.back() = L'4';
		win_filesystem::CreateFilePathList(wstrFolderPath.c_str(), L".m4a", voiceFilePaths);

		return nIntroVoiceFileCount;
	}
} /* namespace ayarabu */

bool ayarabu::LoadScenario(const std::wstring& wstrFilePath, std::vector<adv::TextDatum>& textData, std::vector<adv::PaintDatum>& paintData)
{
	/*初期作成*/
	if (g_formatData.empty())
	{
		std::wstring wstrVoiceMasterDataFilePath = DeriveSoundMasterDataPathFromScriptFilePath(wstrFilePath);
		if (wstrVoiceMasterDataFilePath.empty())return false;

		SetupVoiceFileNameFormatInfo(wstrVoiceMasterDataFilePath);
		if (g_formatData.empty())return false;

		if (!DeriveResourceFolderPathsFromScriptFilePath(wstrFilePath))return false;
	}

	long long llBaseId = ExtractIdFromScriptFileName(wstrFilePath);
	if (llBaseId <= 0)return false;

	/*文章・音声対応作成*/

	std::vector<StoryDatum> storyData;
	ReadScript(wstrFilePath, storyData);
	if (storyData.empty())return false;

	std::vector<std::wstring> voiceFilePaths;
	size_t nIntroVoiceFileCount = FindVoiceFiles(llBaseId, voiceFilePaths);

	const auto FindMainCharacterName = [&storyData]()
		-> const std::wstring
		{
			for (long long i = storyData.size() - 1; i >= 0; --i)
			{
				if (!storyData[i].wstrName.empty() && storyData[i].wstrName.find(L"雄二") == std::wstring::npos)
				{
					return storyData[i].wstrName;
				}
			}
			return std::wstring();
		};

	std::wstring wstrMainCharacterName = FindMainCharacterName();

	size_t nFilePathIndex = voiceFilePaths.size() - 1;
	for (long long i = storyData.size() - 1; i >= 0; --i)
	{
		const StoryDatum& storyDatum = storyData[i];
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
				textDatum.wstrVoicePath = voiceFilePaths[nFilePathIndex];
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
	std::wstring wstrFolderPath = g_wstrStillFolderPath;
	wstrFolderPath.append(L"\\advstill").append(wstrImageId);
	win_filesystem::CreateFilePathList(wstrFolderPath.c_str(), L".png", stillImageFilePaths);

	stillImageFilePaths.erase(std::remove_if(stillImageFilePaths.begin(), stillImageFilePaths.end(),
		[](const std::wstring& wstr)
		-> bool
		{
			return wstr.find(L'#') != std::wstring::npos;
		}), stillImageFilePaths.end());

	std::vector<std::wstring> videoFilePaths;
	wstrFolderPath.assign(g_wstrVideoFolderPath).append(L"\\chara").append(wstrImageId);
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
				imageDatum.isVideo = false;
				imageDatum.wstrFilePath = stillImageFilePaths[i];
				paintData.push_back(std::move(imageDatum));
			}

			for (const auto& path : videoFilePaths)
			{
				adv::PaintDatum imageDatum;
				imageDatum.isVideo = true;
				imageDatum.wstrFilePath = path;
				paintData.push_back(std::move(imageDatum));
			}

			paintData.emplace_back(adv::PaintDatum{ false, stillImageFilePaths.back() });
		}
		else
		{
			for (size_t i = 0; i < stillImageFilePaths.size() / 2; ++i)
			{
				adv::PaintDatum imageDatum;
				imageDatum.isVideo = false;
				imageDatum.wstrFilePath = stillImageFilePaths[i];
				paintData.push_back(std::move(imageDatum));
			}

			for (const auto& path : videoFilePaths)
			{
				adv::PaintDatum imageDatum;
				imageDatum.isVideo = true;
				imageDatum.wstrFilePath = path;
				paintData.push_back(std::move(imageDatum));
			}

			for (size_t i = stillImageFilePaths.size() / 2; i < stillImageFilePaths.size(); ++i)
			{
				adv::PaintDatum imageDatum;
				imageDatum.isVideo = false;
				imageDatum.wstrFilePath = stillImageFilePaths[i];
				paintData.push_back(std::move(imageDatum));
			}
		}
	}

	return !textData.empty() && !paintData.empty();
}
