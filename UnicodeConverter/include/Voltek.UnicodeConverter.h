// Copyright © 2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: MIT

#pragma once

#include "Voltek.UnicodeConverter.Config.h"
#include "Voltek.UnicodeConverter.RawBaseString.h"

namespace voltek
{
	VOLTEK_UP_API int64_t utf8_to_unicode(const char* src, size_t src_len, wchar_t* dst, size_t dst_len);
	VOLTEK_UP_API int64_t unicode_to_utf8(const wchar_t* src, size_t src_len, char* dst, size_t dst_len);

	VOLTEK_UP_API bool is_ascii(const rawbytestring& str);
	VOLTEK_UP_API bool is_utf8(const utf8string& str);

	VOLTEK_UP_API int64_t find_invalid_utf8_codepoint(const char* str, int64_t length, bool stop_on_non_utf8 = true);

	VOLTEK_UP_API ansistring utf8_to_wincp(const utf8string& str);
	VOLTEK_UP_API utf8string wincp_to_utf8(const ansistring& str);
	VOLTEK_UP_API unicodestring utf8_to_utf16(const utf8string& str);
	VOLTEK_UP_API utf8string utf16_to_utf8(const unicodestring& str);
}
