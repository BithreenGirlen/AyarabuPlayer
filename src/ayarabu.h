#ifndef AYARABU_H_
#define AYARABU_H_

#include <string>
#include <vector>

#include "adv.h"

namespace ayarabu
{
	bool LoadScenario(
		const std::wstring& wstrFilePath,
		std::vector<adv::TextDatum>& textData,
		std::vector<adv::PaintDatum>& paintData,
		std::vector<adv::SceneDatum>& sceneData,
		std::vector<adv::LabelDatum>& labelData
	);
}
#endif // !AYARABU_H_
