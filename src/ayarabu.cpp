

#include "ayarabu.h"

#include "win_filesystem.h"
#include "win_text.h"
#include "text_utility.h"
#include "json_minimal.h"

/* 内部用 */
namespace ayarabu
{
	enum class ECommandType
	{
		Unknown = -1,
		Message,
		Still,
		Video
	};

	struct ScriptCommand
	{
		ECommandType commandType = ECommandType::Unknown;
		size_t id;
	};

	struct MessageDatum
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
		const uint8_t* p = reinterpret_cast<const uint8_t*>(src);
		return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	}

	/* 脚本ファイル読み取り */
	static void ReadScript(
		const std::string& scriptFile,
		const uint32_t baseId,
		std::vector<MessageDatum>& messageData,
		std::vector<ScriptCommand>& scriptCommands)
	{
		static constexpr size_t kCommandOffsetAddress = 0x08;
		static constexpr size_t kCommandCountAddress = 0x0c;
		static constexpr size_t kTextOffsetAddress = 0x10;
		static constexpr size_t kTextLengthAddress = 0x14;

		if (scriptFile.size() < kTextLengthAddress + 4ULL)return;

		const uint32_t ulCommandOffset = ToUInt32(&scriptFile[kCommandOffsetAddress]);
		const uint32_t ulCommandCount = ToUInt32(&scriptFile[kCommandCountAddress]);
		const uint32_t ulTextOffset = ToUInt32(&scriptFile[kTextOffsetAddress]);
		const uint32_t ulTextLength = ToUInt32(&scriptFile[kTextLengthAddress]);

		const size_t nTextEndPos = static_cast<size_t>(ulTextOffset + ulTextLength);
		if (scriptFile.size() < nTextEndPos)return;
		const size_t nCommandEndPos = ulCommandOffset + (ulCommandCount * 8ULL);
		if (scriptFile.size() < nCommandEndPos)return;

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
		for (size_t nRead = ulCommandOffset; nRead < nCommandEndPos;)
		{
			CommandDatum c;
			c.type = scriptFile[nRead];
			c.datum.value = ToUInt32(&scriptFile[nRead + 4]);
			nRead += 8ULL;

			if (c.type == 0)
			{
				c.args.resize(c.datum.params.param2);
				for (uint16_t i = 0; i < c.datum.params.param2; ++i)
				{
					c.args[i].type = scriptFile[nRead];
					c.args[i].value = ToUInt32(&scriptFile[nRead + 4]);
					nRead += 8ULL;
				}
			}

			commandData.push_back(std::move(c));
		}

		for (const auto& c : commandData)
		{
			const auto IsMessageCommand = [&c]()
				{
					return c.args.size() > 4 && c.args[0].type == 3 && c.args[1].type == 3;
				};
			const auto IsStillCommand = [&c, &baseId]()
				{
					return c.args.size() == 4 && c.args[1].value == baseId;
				};
			const auto IsVideoCommand = [&c, &baseId]()
				{
					return c.args.size() == 2 && c.args[1].value != 0 && c.args[0].value == baseId;
				};
			if (IsMessageCommand())
			{
				/*
				* [0] 話者名開始位置,
				* [1] 台詞もしくは地の文開始位置,
				* [2] 23固定,
				* [3] 0固定,
				* [4] 音声ファイルID; 割り当て無しの場合0,
				*/
				MessageDatum s;

				uint32_t nStart = ulTextOffset + c.args[0].value;
				if (nStart >= nTextEndPos)continue;

				size_t nPos = scriptFile.find('\0', nStart);
				if (nPos == std::string::npos)continue;

				size_t nLength = nPos - nStart;
				if (nLength)
				{
					s.wstrName = win_text::WidenUtf8(&scriptFile[nStart], static_cast<int>(nLength));
				}

				nStart = ulTextOffset + c.args[1].value;
				if (nStart >= nTextEndPos)continue;

				nPos = scriptFile.find('\0', nStart);
				if (nPos == std::string::npos)continue;

				nLength = nPos - nStart;
				if (nLength)
				{
					s.wstrText = win_text::WidenUtf8(&scriptFile[nStart], static_cast<int>(nLength));
				}

				s.voiceId = c.args[4].value;

				messageData.push_back(std::move(s));

				scriptCommands.emplace_back(ScriptCommand{ ECommandType::Message, messageData.size() - 1 });
			}
			else if (IsStillCommand())
			{
				scriptCommands.emplace_back(ScriptCommand{ ECommandType::Still, c.args[2].value });
			}
			else if (IsVideoCommand())
			{
				scriptCommands.emplace_back(ScriptCommand{ ECommandType::Video, c.args[1].value });
			}
		}

		for (auto& storyDatum : messageData)
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
	/* 音声ファイル階層書式探索 */
	static const std::string* FindVoiceDirectoryFormat(const char* formarId, size_t nformatIdLength)
	{
		for (const auto& voiceFormatDatum : g_voiceFormatData)
		{
			if (voiceFormatDatum.size() > 1 
				&& voiceFormatDatum[0].size() == nformatIdLength
				&& (::memcmp(&voiceFormatDatum[0], formarId, nformatIdLength) == 0))
			{
				return &voiceFormatDatum[1];
			}
		}

		return nullptr;
	}

	static std::wstring DeriveSoundMasterDataPathFromScriptFilePath(const std::wstring& scriptFilePath)
	{
		size_t nPos = scriptFilePath.rfind(L"r18");
		if (nPos == std::wstring::npos)return {};

		return scriptFilePath.substr(0, nPos).append(LR"(mock\master_data\sound\SoundVoiceFormatMasterDatas.any)");
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
	/* 基底ID=>書式ID */
	static char* BaseIdToFormatId(unsigned long baseId, int* nLength)
	{
		static char s_szFormatIdBuffer[32]{};
		int iWritten = sprintf_s(s_szFormatIdBuffer, "%d10001", baseId);
		if (iWritten == -1)return nullptr;

		if (nLength != nullptr)
		{
			*nLength = iWritten;
		}

		return s_szFormatIdBuffer;
	}
	/* 基底ID=>静画・動画ID */
	static std::wstring BaseIdToStillOrVideoId(unsigned long baseId)
	{
		wchar_t swzBuffer[5]{};
		swprintf_s(swzBuffer, L"%04lu", baseId);
		return swzBuffer;
	}
	/* 脚本ファイル名から基底ID抽出 */
	static unsigned long ExtractIdFromScriptFileName(const std::wstring& wstrFilePath)
	{
		constexpr wchar_t swzStart[] = L"eventdata";
		constexpr size_t startLength = sizeof(swzStart) / sizeof(wchar_t) - 1;
		constexpr wchar_t swzEnd[] = L"03.evsc";

		size_t nPos1 = wstrFilePath.rfind(swzStart);
		size_t nPos2 = wstrFilePath.rfind(swzEnd);
		if (nPos1 == std::wstring::npos || nPos2 == std::wstring::npos)return -1;

		nPos1 += startLength;

		std::wstring wstrId = wstrFilePath.substr(nPos1, nPos2 - nPos1);
		unsigned long baseId = wcstoul(wstrId.c_str(), nullptr, 10);
		return baseId;
	}
	/* 音声ファイル探索 */
	static size_t FindVoiceFiles(unsigned long baseId, std::vector<std::wstring>& voiceFilePaths)
	{
		int nFormatIdLength = 0;
		char* pszFormatId = BaseIdToFormatId(baseId, &nFormatIdLength);
		if (pszFormatId == nullptr)return 0;

		const std::string* pUtf8VoiceDirectoryFormat = FindVoiceDirectoryFormat(pszFormatId, nFormatIdLength);
		if (pUtf8VoiceDirectoryFormat == nullptr || pUtf8VoiceDirectoryFormat->empty())return 0;

		size_t nFormatPos = pUtf8VoiceDirectoryFormat->find("{1}");
		if (nFormatPos == std::string::npos)nFormatPos = pUtf8VoiceDirectoryFormat->size();

		wchar_t utf16DirectoryFormatBuffer[128]{};
		static constexpr int nUtf16DirectoryFormatBufferSize = std::size(utf16DirectoryFormatBuffer);
		int nDirectoryFormatLength = win_text::WidenUtf8InBuffer(&(*pUtf8VoiceDirectoryFormat)[0], static_cast<int>(nFormatPos), utf16DirectoryFormatBuffer, nUtf16DirectoryFormatBufferSize);
		if (nDirectoryFormatLength == 0)return 0;

		wchar_t utf16VoiceFolderPathBuffer[512]{};
		int nVoiceFolderPathBufferLength = swprintf_s(utf16VoiceFolderPathBuffer, L"%.*s\\%.*s3",
			static_cast<int>(g_wstrVoiceFolderPath.size()), g_wstrVoiceFolderPath.data(),
			nDirectoryFormatLength, utf16DirectoryFormatBuffer
		);
		if (nVoiceFolderPathBufferLength < 1)return 0;

		win_filesystem::CreateFilePathList(utf16VoiceFolderPathBuffer, static_cast<size_t>(nVoiceFolderPathBufferLength), L".m4a", voiceFilePaths);
		const size_t nIntroVoiceFileCount = voiceFilePaths.size();

		utf16VoiceFolderPathBuffer[nVoiceFolderPathBufferLength - 1ULL] = L'4';
		win_filesystem::CreateFilePathList(utf16VoiceFolderPathBuffer, static_cast<size_t>(nVoiceFolderPathBufferLength), L".m4a", voiceFilePaths);

		return nIntroVoiceFileCount;
	}

	static void ExtractFileNameWithoutExtension(const std::wstring& filePath, std::wstring& fileName)
	{
		size_t nPos1 = filePath.find_last_of(L"\\/");
		if (nPos1 == std::wstring::npos)nPos1 = 0;
		else ++nPos1;

		size_t nPos2 = filePath.find(L'.', nPos1);
		if (nPos2 == std::wstring::npos)nPos2 = filePath.size();

		fileName.assign(&filePath[nPos1], &filePath[nPos2]);
	}
} /* namespace ayarabu */

bool ayarabu::LoadScenario(const std::wstring& scriptFilePath, std::vector<adv::TextDatum>& textData, std::vector<adv::PaintDatum>& paintData, std::vector<adv::SceneDatum>& sceneData, std::vector<adv::LabelDatum>& labelData)
{
	/* 初期作成 */
	if (g_voiceFormatData.empty())
	{
		std::wstring wstrVoiceMasterDataFilePath = DeriveSoundMasterDataPathFromScriptFilePath(scriptFilePath);
		if (wstrVoiceMasterDataFilePath.empty())return false;

		SetupVoiceFileNameFormatData(wstrVoiceMasterDataFilePath, g_voiceFormatData);
		if (g_voiceFormatData.empty())return false;

		if (!DeriveResourceFolderPathsFromScriptFilePath(scriptFilePath))return false;
	}

	uint32_t baseId = ExtractIdFromScriptFileName(scriptFilePath);
	if (baseId == static_cast<uint32_t>(-1L)) return false;

	std::string scriptFile = win_filesystem::LoadFileAsString(scriptFilePath.c_str());
	if (scriptFile.empty())return false;

	/* 文章・音声・静画・動画に関する指令文の抜粋 */

	std::vector<MessageDatum> messageData;
	std::vector<ScriptCommand> scriptCommands;
	ReadScript(scriptFile, baseId, messageData, scriptCommands);
	if (scriptCommands.empty())return false;

	const auto FindEpisode4StartIndex = [&scriptCommands]()
		-> long long
		{
			const auto& iter = std::find_if(scriptCommands.cbegin(), scriptCommands.cend(),
				[](const ScriptCommand& scriptCommand)
				{
					return scriptCommand.commandType == ECommandType::Still;
				});

			if (iter != scriptCommands.cend())
			{
				return std::distance(scriptCommands.cbegin(), iter);
			}

			return -1;
		};

	long long nEpisode4StartIndex = FindEpisode4StartIndex();
	if (nEpisode4StartIndex == -1)return false;

	/* 音声ファイル探索 */

	std::vector<std::wstring> voiceFilePaths;
	size_t nIntroVoiceFileCount = FindVoiceFiles(baseId, voiceFilePaths);

	/* 静画・動画ファイル探索 */

	std::wstring wstrImageId = BaseIdToStillOrVideoId(baseId);

	std::vector<std::wstring> stillIFilePaths;
	std::wstring wstrFolderPath = g_wstrStillFolderPath;
	wstrFolderPath.append(L"\\advstill").append(wstrImageId);
	win_filesystem::CreateFilePathList(wstrFolderPath, L".png", stillIFilePaths);

	std::vector<std::wstring> videoFilePaths;
	wstrFolderPath.assign(g_wstrVideoFolderPath).append(L"\\chara").append(wstrImageId);
	win_filesystem::CreateFilePathList(wstrFolderPath, L".mp4", videoFilePaths);

	std::wstring labelCaptionBuffer;
	size_t nLastPaintIndex = 0;
	
	for (size_t i = nEpisode4StartIndex; i < scriptCommands.size(); ++i)
	{
		const auto& c = scriptCommands[i];
		const size_t fileIndex = c.id - 1;
		switch (c.commandType)
		{
		case ECommandType::Message:
			if (c.id < messageData.size())
			{
				const auto& messageDatum = messageData[c.id];

				adv::TextDatum textDatum;
				if (!messageDatum.wstrName.empty())
				{
					textDatum.wstrText = messageDatum.wstrName;
					textDatum.wstrText += L':';
				}
				textDatum.wstrText += L" \n";
				textDatum.wstrText += messageDatum.wstrText;
				if (messageDatum.voiceId != 0)
				{
					/*
					* Voice ID and format ID is associated in L"SoundVoiceMasterDatas.any",
					* but this file is huge in size and avoided preferring less memory consumption.
					* The format is easily guessed as follows:
					* ep3 : baseId + '3' + voiceId
					* ep4 : baseId + '4' + voiceId
					*/
					size_t voiceFileIndex = nIntroVoiceFileCount - 1 + (messageDatum.voiceId % 1000);
					if (voiceFileIndex < voiceFilePaths.size())
					{
						textDatum.wstrVoicePath = voiceFilePaths[voiceFileIndex];
					}
				}

				textData.push_back(std::move(textDatum));
				sceneData.push_back(adv::SceneDatum{ textData.size() - 1, nLastPaintIndex });
				
				if (!labelCaptionBuffer.empty())
				{
					labelData.emplace_back(adv::LabelDatum{ std::move(labelCaptionBuffer), sceneData.size() - 1 });
				}
			}
			break;
		case ECommandType::Still:
			if (fileIndex < stillIFilePaths.size())
			{
				/* 古い寸劇では最終動画と最終静画の間に文章が存在しない。  */
				if (!labelCaptionBuffer.empty() && !sceneData.empty())
				{
					sceneData.back().nPaintIndex = nLastPaintIndex;
					labelData.emplace_back(adv::LabelDatum{ std::move(labelCaptionBuffer), sceneData.size() - 1 });
				}

				paintData.emplace_back(adv::PaintDatum{ false, stillIFilePaths[fileIndex] });
				nLastPaintIndex = paintData.size() - 1;

				ExtractFileNameWithoutExtension(stillIFilePaths[fileIndex], labelCaptionBuffer);
			}
			break;
		case ECommandType::Video:
			if (fileIndex < videoFilePaths.size())
			{
				paintData.emplace_back(adv::PaintDatum{ true, videoFilePaths[fileIndex] });
				nLastPaintIndex = paintData.size() - 1;

				ExtractFileNameWithoutExtension(videoFilePaths[fileIndex], labelCaptionBuffer);
			}
			break;
		default:
			break;
		}
	}

	return !textData.empty() && !paintData.empty();
}
