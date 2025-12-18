#ifndef ADV_H_
#define ADV_H_

#include <string>

namespace adv
{
	struct TextDatum
	{
		std::wstring wstrText;
		std::wstring wstrVoicePath;
	};

	struct PaintDatum
	{
		bool isVideo = false;
		std::wstring wstrFilePath;
	};
}
#endif // !ADV_H_
