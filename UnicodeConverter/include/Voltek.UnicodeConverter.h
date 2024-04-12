// Copyright © 2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: MIT

#pragma once

#include "Voltek.UnicodeConverter.Config.h"
#include "Voltek.UnicodeConverter.RawBaseString.h"

namespace voltek
{
	VOLTEK_UP_API bool is_utf8(const utf8string& str);
	VOLTEK_UP_API ansistring utf8_to_wincp(const utf8string& str);
	VOLTEK_UP_API utf8string wincp_to_utf8(const ansistring& str);
	VOLTEK_UP_API unicodestring utf8_to_utf16(const utf8string& str);
	VOLTEK_UP_API utf8string utf16_to_utf8(const unicodestring& str);
}
