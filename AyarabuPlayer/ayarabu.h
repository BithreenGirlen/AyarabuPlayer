#ifndef AYARABU_H_
#define AYARABU_H_

#include <string>
#include <vector>

#include "adv.h"

namespace ayarabu
{
	bool LoadScenario(const std::wstring& wstrFilePath, std::vector<adv::TextDatum>& textData, std::vector<adv::PaintDatum>& paintData);
}
#endif // !AYARABU_H_
