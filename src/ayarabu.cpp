

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
		uint32_t voiceId;
	};

	/* "voiceFormatId", "directoryPath", "assetBundleName", "assetDataName" */
	static std::vector<std::vector<std::string>> g_voiceFormatData;
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

	/* 脚本ファイル読み取り */
	static void ReadScript(const std::wstring& wstrFilePath, std::vector<StoryDatum>& storyData)
	{
		std::string strFile = win_filesystem::LoadFileAsString(wstrFilePath.c_str());

		static constexpr size_t kCommandOffset = 0x08;
		static constexpr size_t kCommandCount = 0x0c;
		static constexpr size_t kTextOffset = 0x10;
		static constexpr size_t kTextLength = 0x14;

		if (strFile.size() < kTextLength + 4ULL)return;

		uint32_t ulCommandPos = ToUInt32(&strFile[kCommandOffset]);
		uint32_t ulCommandCount = ToUInt32(&strFile[kCommandCount]);
		uint32_t ulTextPos = ToUInt32(&strFile[kTextOffset]);
		uint32_t ulTextLength = ToUInt32(&strFile[kTextLength]);

		size_t nTextEndPos = static_cast<size_t>(ulTextPos + ulTextLength);
		if (strFile.size() < nTextEndPos)return;
		size_t nCommandEndPos = ulCommandPos + (ulCommandCount * 8ULL);
		if (strFile.size() < nCommandEndPos)return;

		struct CommandArg
		{
			uint8_t type;
			uint32_t value;
		};

		struct CommandDatum
		{
			uint8_t type;
			union Datum
			{
				struct Params
				{
					uint16_t param1;
					uint16_t param2;
				};
				Params params;
				uint32_t value;
			};
			Datum datum;
			std::vector<CommandArg> args;
		};
		std::vector<CommandDatum> commandData;
		for (size_t nRead = ulCommandPos; nRead < nCommandEndPos;)
		{
			CommandDatum c;
			c.type = strFile[nRead];
			c.datum.value = ToUInt32(&strFile[nRead + 4]);
			nRead += 8ULL;

			if (c.type != 0)continue;

			c.args.resize(c.datum.params.param2);
			for (uint16_t i = 0; i < c.datum.params.param2; ++i)
			{
				c.args[i].type = strFile[nRead];
				c.args[i].value = ToUInt32(&strFile[nRead + 4]);
				nRead += 8ULL;
			}

			commandData.push_back(std::move(c));
		}

		for (const auto& c : commandData)
		{
			if (c.args.size() > 4 && c.args[0].type == 3 && c.args[0].type == 3)
			{
				/*
				* [0] 話者名開始位置,
				* [1] 台詞もしくは地の文開始位置,
				* [2] 23固定,
				* [3] 0固定,
				* [4] 音声ファイルID; 割り当て無しの場合0,
				* [5] loop?
				*/
				StoryDatum s;

				uint32_t nStart = ulTextPos + c.args[0].value;
				if (nStart >= nTextEndPos)continue;

				size_t nPos = strFile.find('\0', nStart);
				if (nPos == std::string::npos)continue;

				size_t nLength = nPos - nStart;
				if (nLength)
				{
					s.wstrName = win_text::WidenUtf8(&strFile[nStart], static_cast<int>(nLength));
				}

				nStart = ulTextPos + c.args[1].value;
				if (nStart >= nTextEndPos)continue;

				nPos = strFile.find('\0', nStart);
				if (nPos == std::string::npos)continue;

				nLength = nPos - nStart;
				if (nLength)
				{
					s.wstrText = win_text::WidenUtf8(&strFile[nStart], static_cast<int>(nLength));
				}

				s.voiceId = c.args[4].value;

				storyData.push_back(std::move(s));
			}
		}

		for (auto& storyDatum : storyData)
		{
			text_utility::ReplaceAll(storyDatum.wstrText, L"<name></name>", L"陰陽師");
			text_utility::ReplaceAll(storyDatum.wstrText, L"$n", L"\n");
		}
	}
	/* 音声ファイル名称書式表構築 */
	static void SetupVoiceFileNameFormatData(const std::wstring& wstrFilePath, std::vector<std::vector<std::string>>& formatTable)
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

		formatTable = std::move(formatData);
	}
	/* 音声ファイル名称書式探索 */
	static const std::string FindVoiceFileFormat(const std::string& strKey)
	{
		for (const auto& voiceFormatDatum : g_voiceFormatData)
		{
			if (voiceFormatDatum.size() > 1 && voiceFormatDatum[0] == strKey)
			{
				return voiceFormatDatum[1];
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
		constexpr size_t startLength = sizeof(swzStart) / sizeof(wchar_t) - 1;
		constexpr wchar_t swzEnd[] = L"03.evsc";

		size_t nPos1 = wstrFilePath.rfind(swzStart);
		size_t nPos2 = wstrFilePath.rfind(swzEnd);
		if (nPos1 == std::wstring::npos || nPos2 == std::wstring::npos)return -1;

		nPos1 += startLength;

		std::wstring wstrId = wstrFilePath.substr(nPos1, nPos2 - nPos1);
		long long llBaseId = wcstol(wstrId.c_str(), nullptr, 10);
		return llBaseId;
	}
	/* 音声ファイル探索 */
	static size_t FindVoiceFiles(long long baseId, std::vector<std::wstring>& voiceFilePaths)
	{
		std::string strFormatId = BaseIdToFormatId(baseId);
		const std::string& strFilePathFormat = FindVoiceFileFormat(strFormatId);
		if (strFilePathFormat.empty())return 0;

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
	if (g_voiceFormatData.empty())
	{
		std::wstring wstrVoiceMasterDataFilePath = DeriveSoundMasterDataPathFromScriptFilePath(wstrFilePath);
		if (wstrVoiceMasterDataFilePath.empty())return false;

		SetupVoiceFileNameFormatData(wstrVoiceMasterDataFilePath, g_voiceFormatData);
		if (g_voiceFormatData.empty())return false;

		if (!DeriveResourceFolderPathsFromScriptFilePath(wstrFilePath))return false;
	}

	long long llBaseId = ExtractIdFromScriptFileName(wstrFilePath);
	if (llBaseId <= 0)return false;

	/*文章・音声対応作成*/

	std::vector<StoryDatum> storyData;
	ReadScript(wstrFilePath, storyData);
	if (storyData.empty())return false;

	/*
	* The table for voice ID and its format ID is associated in L"SoundVoiceMasterDatas.any",
	* but this file is huge in size and avoided preferring less memory consumption.
	* The format is easily guessed as follows:
	* ep3 : llBaseId + '3' + voiceId
	* ep4 : llBaseId + '4' + voiceId
	*/

	const auto FindMainStartPos = [&llBaseId, &storyData]()
		-> size_t
		{
			/* uint32_t is 10 digits in maximum */
			char sBuffer1[16]{};
			char sBuffer2[16]{};
			int iBaseLen = sprintf_s(sBuffer1, "%lld4", llBaseId);
			for (size_t i = 0; i < storyData.size(); ++i)
			{
				if (storyData[i].voiceId == 0)continue;

				sprintf_s(sBuffer2, "%ld", storyData[i].voiceId);
				if (strncmp(sBuffer1, sBuffer2, iBaseLen) == 0)return i;
			}

			return 0;
		};

	size_t nMainStartIndex = FindMainStartPos();

	std::vector<std::wstring> voiceFilePaths;
	size_t nIntroVoiceFileCount = FindVoiceFiles(llBaseId, voiceFilePaths);

	for (size_t i = nMainStartIndex; i < storyData.size(); ++i)
	{
		const auto& storyDatum = storyData[i];

		adv::TextDatum textDatum;
		if (!storyDatum.wstrName.empty())
		{
			textDatum.wstrText = storyDatum.wstrName;
			textDatum.wstrText += L':';
		}
		textDatum.wstrText += L" \n";
		textDatum.wstrText += storyDatum.wstrText;
		if (storyDatum.voiceId != 0)
		{
			size_t voiceFileIndex = nIntroVoiceFileCount - 1 + (storyDatum.voiceId % 1000);
			if (voiceFileIndex < voiceFilePaths.size())
			{
				textDatum.wstrVoicePath = voiceFilePaths[voiceFileIndex];
			}
		}

		textData.push_back(std::move(textDatum));
	}

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
				paintData.emplace_back(adv::PaintDatum{ false, stillImageFilePaths[i] });
			}

			for (const auto& voiceFilePath : videoFilePaths)
			{
				paintData.emplace_back(adv::PaintDatum{ true, voiceFilePath });
			}

			paintData.emplace_back(adv::PaintDatum{ false, stillImageFilePaths.back() });
		}
		else
		{
			for (size_t i = 0; i < stillImageFilePaths.size() / 2; ++i)
			{
				paintData.emplace_back(adv::PaintDatum{ false, stillImageFilePaths[i] });
			}

			for (const auto& voiceFilePath : videoFilePaths)
			{
				paintData.emplace_back(adv::PaintDatum{ true, voiceFilePath });
			}

			for (size_t i = stillImageFilePaths.size() / 2; i < stillImageFilePaths.size(); ++i)
			{
				paintData.emplace_back(adv::PaintDatum{ false, stillImageFilePaths[i] });
			}
		}
	}

	return !textData.empty() && !paintData.empty();
}
