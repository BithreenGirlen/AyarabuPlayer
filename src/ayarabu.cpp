

#include "ayarabu.h"

#include "win_filesystem.h"
#include "win_text.h"
#include "text_utility.h"
#include "json_minimal.h"
#include "static_string.h"

/* 内部用 */
namespace ayarabu
{
	static constexpr const size_t kMaxFilePathLength = 1024;
	using StaticWSringMaxPath = StaticString<wchar_t, kMaxFilePathLength>;

	/* 
	* Usually, naming convention for enum class is PascalCase,
	* but here MACRO_CASE is adopted for the nature of the scripts.
	*/
	enum class EAdvUserInstructionNo
	{
		NOP = 30,
		READ_MESSAGE,
		WAIT_WORK,
		OPEN_EPISODE,
		CLOSE_EPISODE,
		CHARA_DELETE,
		CHARA_DISP_REGISTRATION,
		CHARA_ACTION_JUMP,
		FILE_PRELOAD,
		CHARA_MOVE,
		DISP_EXECUTION,
		CHARA_OUT,
		CHARA_SLIDE,
		WHITE_IN,
		WHITE_OUT,
		BLACK_IN,
		BLACK_OUT,
		SHAKE,
		CAMERA_ZOOM_RESET,
		CAMERA_ZOOM,
		SE_STOP,
		SE_START,
		BGM_STOP,
		BGM_START,
		BG_DISP,
		BRANCH_DISP,
		BRANCH_RESULT,
		MESSAGE_WINDOW_DISP_SWITCH,
		EMOTION_SWITCH,
		MOVIE_PLAY,
		TALK_BRIGHTNESS,
		COLOR_IN_H,
		COLOR_OUT_H,
		FILE_LOAD,
		CHARA_EFFECT_ANIMATION,
		STOP_MOVIE,
		CHARA_MASK,
		BLACK_FRAME_ON,
		BLACK_FRAME_OFF,
		DARK_EFFECT_ON,
		DARK_EFFECT_OFF,
		STILL_DELETE,
		STILL_DISP_REGISTRATION,
		STILL_OUT,
		SUSPEND,
		EFFECT_ANIMATION_STOP,
		EFFECT_ANIMATION_DELETE,
		GRAY_SCALE,
		VOICE_STOP,
		VOICE_START,
		MESSAGE_WINDOW_TYPE,
		MESSAGE_WINDOW_TYPE_RESET,
		ALTER_KOMAGEKI,
		ALTER_CHANGE_CHARA_ATTACHMENT,
		MOVIE_PAUSE,
		MOVIE_RESUME,
		CHARA_FACE_PATTERN,
		EFFECT_ANIMATION_COORDINATE,
		PREFAB_PLAY,
		PREFAB_STOP,
		COLOR_IN,
		COLOR_OUT,
		COUNT_MAX
	};

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
	static StaticWSringMaxPath g_stillFolderPath;
	static StaticWSringMaxPath g_videoFolderPath;
	static StaticWSringMaxPath g_voiceFolderPath;

	static unsigned long ToUInt32(const char* src)
	{
		const uint8_t* p = reinterpret_cast<const uint8_t*>(src);
		return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	}

	/* 脚本ファイル読み取り */
	static void ReadScript(
		const std::string& scriptFile,
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
			const EAdvUserInstructionNo type = static_cast<EAdvUserInstructionNo>(c.datum.params.param1);
			switch (type)
			{
			case EAdvUserInstructionNo::READ_MESSAGE:
			case EAdvUserInstructionNo::VOICE_START:
				if (c.args.size() > 4)
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
				break;
			case EAdvUserInstructionNo::MOVIE_PLAY:
				if (c.args.size() > 1)
				{
					scriptCommands.emplace_back(ScriptCommand{ ECommandType::Video, c.args[1].value });
				}
				break;
			case EAdvUserInstructionNo::STILL_DISP_REGISTRATION:
				if (c.args.size() > 2)
				{
					scriptCommands.emplace_back(ScriptCommand{ ECommandType::Still, c.args[2].value });
				}
				break;
			default:
				break;
			}
		}

		for (auto& storyDatum : messageData)
		{
			text_utility::ReplaceAll(storyDatum.wstrText, L"<name></name>", L"陰陽師");
			text_utility::ReplaceAll(storyDatum.wstrText, L"$n", L"\n");
		}
	}
	/* 音声ファイル名称書式表構築 */
	static void SetupVoiceFileNameFormatData(const std::wstring& tableFilePath, std::vector<std::vector<std::string>>& formatTable)
	{
		std::string tableFile = win_filesystem::LoadFileAsString(tableFilePath.c_str());
		if (tableFile.empty())return;

		const char* p = &tableFile[0];
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
				&& (::memcmp(voiceFormatDatum[0].data(), formarId, nformatIdLength) == 0))
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
	static bool DeriveResourceFolderPathsFromScriptFilePath(const std::wstring& scriptFilePath)
	{
		size_t nAdvPos = scriptFilePath.rfind(L"adventure");
		if (nAdvPos == std::wstring::npos)return false;

		size_t nEventDataPos = scriptFilePath.rfind(L"eventdata\\eventdata");
		if (nEventDataPos == std::wstring::npos)return false;

		g_stillFolderPath.assign(&scriptFilePath[0], nEventDataPos).append(LR"(advstill)");
		g_videoFolderPath.assign(&scriptFilePath[0], nAdvPos).append(LR"(movie\harem)");
		g_voiceFolderPath.assign(&scriptFilePath[0], nAdvPos).append(LR"(sound\voice)");

		return !g_stillFolderPath.empty() && !g_videoFolderPath.empty() && !g_voiceFolderPath.empty();
	}

	/* 基底ID=>書式ID */
	template<size_t N>
	static bool BaseIdToFormatId(unsigned long baseId, char(&dst)[N], int& nLength)
	{
		int nWritten = ::sprintf_s(dst, "%d10001", baseId);
		if (nWritten == -1)return false;

		nLength = nWritten;

		return true;
	}

	/* 基底ID=>静画・動画ID */
	template<size_t N>
	static bool BaseIdToStillOrVideoId(unsigned long baseId, wchar_t(&dst)[N], int& nLength)
	{
		int nWritten = ::swprintf_s(dst, L"%04lu", baseId);
		if (nWritten == -1)return false;

		nLength = nWritten;

		return true;
	}

	/* 脚本ファイル名から基底ID抽出 */
	static unsigned long ExtractIdFromScriptFileName(const std::wstring& scriptFilePath)
	{
		constexpr wchar_t scriptFileIdentifier[] = L"eventdata";
		constexpr size_t startLength = sizeof(scriptFileIdentifier) / sizeof(wchar_t) - 1;
		constexpr wchar_t interestingIdentifier[] = L"03.evsc";

		size_t nPos1 = scriptFilePath.rfind(scriptFileIdentifier);
		size_t nPos2 = scriptFilePath.rfind(interestingIdentifier);
		if (nPos1 == std::wstring::npos || nPos2 == std::wstring::npos)return -1;

		nPos1 += startLength;

		const size_t nLength = nPos2 - nPos1;
		StaticString<wchar_t, 32> idBuffer;
		if (nLength > idBuffer.capacity())return -1;
		idBuffer.assign(&scriptFilePath[nPos1], nLength);

		unsigned long baseId = ::wcstoul(idBuffer.data(), nullptr, 10);

		return baseId;
	}
	/* 音声ファイル探索 */
	static size_t FindVoiceFiles(unsigned long baseId, std::vector<std::wstring>& voiceFilePaths)
	{
		char formatIdBuffer[32]{};
		int nFormatIdLength = 0;
		if (!BaseIdToFormatId(baseId, formatIdBuffer, nFormatIdLength))return 0;

		const std::string* pUtf8VoiceDirectoryFormat = FindVoiceDirectoryFormat(formatIdBuffer, nFormatIdLength);
		if (pUtf8VoiceDirectoryFormat == nullptr || pUtf8VoiceDirectoryFormat->empty())return 0;

		size_t nFormatPos = pUtf8VoiceDirectoryFormat->find("{1}");
		if (nFormatPos == std::string::npos)nFormatPos = pUtf8VoiceDirectoryFormat->size();

		wchar_t utf16DirectoryFormatBuffer[128]{};
		static constexpr int nUtf16DirectoryFormatBufferSize = std::size(utf16DirectoryFormatBuffer);
		int nDirectoryFormatLength = win_text::WidenUtf8InBuffer(&(*pUtf8VoiceDirectoryFormat)[0], static_cast<int>(nFormatPos), utf16DirectoryFormatBuffer, nUtf16DirectoryFormatBufferSize);
		if (nDirectoryFormatLength == 0)return 0;

		wchar_t utf16VoiceFolderPathBuffer[512]{};
		int nVoiceFolderPathBufferLength = swprintf_s(utf16VoiceFolderPathBuffer, L"%.*s\\%.*s3",
			static_cast<int>(g_voiceFolderPath.size()), g_voiceFolderPath.data(),
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
		std::wstring voiceMasterDataFilePath = DeriveSoundMasterDataPathFromScriptFilePath(scriptFilePath);
		if (voiceMasterDataFilePath.empty())return false;

		SetupVoiceFileNameFormatData(voiceMasterDataFilePath, g_voiceFormatData);
		if (g_voiceFormatData.empty())return false;

		if (!DeriveResourceFolderPathsFromScriptFilePath(scriptFilePath))return false;
	}

	const uint32_t baseId = ExtractIdFromScriptFileName(scriptFilePath);
	if (baseId == static_cast<uint32_t>(-1L)) return false;

	std::string scriptFile = win_filesystem::LoadFileAsString(scriptFilePath.c_str());
	if (scriptFile.empty())return false;

	/* 文章・音声・静画・動画に関する指令文の抜粋 */

	std::vector<MessageDatum> messageData;
	std::vector<ScriptCommand> scriptCommands;
	ReadScript(scriptFile, messageData, scriptCommands);
	if (scriptCommands.empty())return false;

	const auto FindEpisode4StartIndex = [&scriptCommands]()
		-> long long
		{
			const auto& iter = std::find_if(scriptCommands.cbegin(), scriptCommands.cend(),
				[](const ScriptCommand& scriptCommand)
				{
					/* The scene of "0048503" does not start with still, but with movie. */
					return scriptCommand.commandType == ECommandType::Still || scriptCommand.commandType == ECommandType::Video;
				});

			if (iter != scriptCommands.cend())
			{
				return std::distance(scriptCommands.cbegin(), iter);
			}

			return -1;
		};

	const long long nEpisode4StartIndex = FindEpisode4StartIndex();
	if (nEpisode4StartIndex == -1)return false;

	/* 音声ファイル探索 */

	std::vector<std::wstring> voiceFilePaths;
	const size_t nIntroVoiceFileCount = FindVoiceFiles(baseId, voiceFilePaths);

	/* 静画・動画ファイル探索 */
	wchar_t imageIdBuffer[32]{};
	int imageIdBufferLength = 0;
	if (!BaseIdToStillOrVideoId(baseId, imageIdBuffer, imageIdBufferLength))return false;

	StaticWSringMaxPath folderPathBuffer;

	std::vector<std::wstring> stillIFilePaths;
	folderPathBuffer.assign(g_stillFolderPath).append(L"\\advstill").append(imageIdBuffer, imageIdBufferLength);
	win_filesystem::CreateFilePathList(folderPathBuffer.data(), folderPathBuffer.size(), L".png", stillIFilePaths);

	std::vector<std::wstring> videoFilePaths;
	folderPathBuffer.assign(g_videoFolderPath).append(L"\\chara").append(imageIdBuffer, imageIdBufferLength);
	win_filesystem::CreateFilePathList(folderPathBuffer.data(), folderPathBuffer.size(), L".mp4", videoFilePaths);

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
					textDatum.wstrText.assign(messageDatum.wstrName);
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
