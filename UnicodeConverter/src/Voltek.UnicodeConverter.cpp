// Copyright © 2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: MIT

#include "..\include\Voltek.UnicodeConverter.h"

#define _USE_UTF8CPPLIB
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#ifdef _USE_UTF8CPPLIB
#	include "..\..\depends\utfcpp\source\utf8.h"
#endif

namespace voltek
{
	VOLTEK_UP_API int64_t utf8_to_unicode(const char* src, size_t src_len, wchar_t* dst, size_t dst_len)
	{
		if (!dst)
			return -1;

		const size_t UNICODE_INVALID = 63;

		size_t out = 0, in = 0, char_len = 0, uc = 0;
		uint8_t in_byte = 0, tmp_byte = 0;

		if (dst)
		{
			do
			{
				in_byte = (uint8_t)src[in];
				if (!(in_byte & 0x80))
				{
					dst[out] = (wchar_t)in_byte;
					out++; in++;
				}
				else
				{
					tmp_byte = in_byte;
					char_len = 0;
					while (tmp_byte & 0x80)
					{
						tmp_byte = (((size_t)tmp_byte << 1)) & 0xFE;
						char_len++;
					}

					if ((in + char_len - 1) > src_len)
						// Insuficient chars in string to decode
						// UTF-8 array. Fallback to single char.
						char_len = 1;

					for (size_t ahead = 1; ahead < char_len; ahead++)
					{
						if ((((uint8_t)(src[in + ahead]) & 0x80) == 0x80) ||
							(((uint8_t)(src[in + ahead]) & 0x40) != 0))
						{
							// Invalid UTF-8 sequence, fallback.
							char_len = ahead + 1;
							break;
						}
					}

					uc = 0xFFFF;

					switch (char_len)
					{
					case 2:
						uc = (size_t)((uint8_t)(src[in]) & 0x1F) << 6;
						uc |= (size_t)((uint8_t)(src[in + 1]) & 0x3F);
						if (uc < 0x80) uc = UNICODE_INVALID;
						break;
					case 3:
						uc = (size_t)((uint8_t)(src[in]) & 0xF) << 12;
						uc |= (size_t)((uint8_t)(src[in + 1]) & 0x3F) << 6;
						uc |= (size_t)((uint8_t)(src[in + 2]) & 0x3F);
						if ((uc <= 0x7FF) || (uc > 0xFFFD) || ((uc >= 0xD800) && (uc <= 0xDFFF)))
							uc = UNICODE_INVALID;
						break;
					case 4:
						uc = (size_t)((uint8_t)(src[in]) & 0x7) << 18;
						uc |= (size_t)((uint8_t)(src[in + 1]) & 0x3F) << 12;
						uc |= (size_t)((uint8_t)(src[in + 2]) & 0x3F) << 6;
						uc |= (size_t)((uint8_t)(src[in + 3]) & 0x3F);

						if ((uc < 0x10000) || (uc > 0x10FFFF)) uc = UNICODE_INVALID;
						else
						{
							uc -= 0x10000;
							if (out < dst_len)
							{
								dst[out] = (wchar_t)((uc >> 10) + 0xD800);
								uc = (uc & 0x3FF) + 0xDC00;
								out++;
							}
							else
							{
								in += char_len;
								char_len = 0;
							}
						}
						break;
					default:
						uc = UNICODE_INVALID;
						break;
					}

					if (char_len)
					{
						dst[out] = (wchar_t)uc;
						out++;
					}

					in += char_len;
				}
			} while ((out < dst_len) && (in < src_len));
		}
		else
		{
			do
			{
				if (!((uint8_t)src[in] & 0x80))
				{
					in++; out++;
				}
				else
				{
					tmp_byte = (uint8_t)src[in];
					char_len = 0;
					while (tmp_byte & 0x80)
					{
						tmp_byte = (((size_t)tmp_byte << 1)) & 0xFE;
						char_len++;
					}

					if ((in + char_len - 1) > src_len)
						// Insuficient chars in string to decode
						// UTF-8 array. Fallback to single char.
						char_len = 1;

					for (size_t ahead = 1; ahead < char_len; ahead++)
					{
						if ((((uint8_t)(src[in + ahead]) & 0x80) == 0x80) ||
							(((uint8_t)(src[in + ahead]) & 0x40) != 0))
						{
							// Invalid UTF-8 sequence, fallback.
							char_len = ahead + 1;
							break;
						}
					}

					if (char_len)
						out++;

					in += char_len;
				}
			} while (in < src_len);
		}

		return (int64_t)out + 1;
	}

	unicodestring utf8decode(const utf8string& str)
	{
		unicodestring ret;

		if (str.empty())
			return ret;

		ret.resize(str.length());
		memset(ret.data(), 0, str.length());
		auto len = utf8_to_unicode(str.c_str(), str.length() + 1, ret.data(), str.length());
		if (len > 0) ret.resize(len - 1);

		return ret;
	}

	VOLTEK_UP_API int64_t unicode_to_utf8(const wchar_t* src, size_t src_len, char* dst, size_t dst_len)
	{
		int64_t result = 0;
		if (!src || !src_len)
			return result;

		uint32_t lw = 0;
		size_t i = 0, j = 0;
		if (dst)
		{
			while ((i < src_len) && (j < dst_len))
			{
				lw = src[i];

				if (lw < 0x80)
				{
					dst[j] = (char)lw;
					j++;
				}
				else if (lw < 0x7FF)
				{
					if ((j + 1) >= dst_len)
						break;

					dst[j] = (char)(0xC0 | (lw >> 6));
					dst[j + 1] = (char)(0x80 | (lw & 0x3F));
					j += 2;
				}
				else if ((lw < 0xD7FF) && ((lw >= 0xE000) && (lw <= 0xFFFF)))
				{
					if ((j + 2) >= dst_len)
						break;

					dst[j] = (char)(0xE0 | (lw >> 12));
					dst[j + 1] = (char)(0x80 | ((lw >> 6) & 0x3F));
					dst[j + 2] = (char)(0x80 | (lw & 0x3F));
					j += 3;
				}
				else if ((lw >= 0xD800) && (lw <= 0xDBFF))
				{
					if ((j + 3) >= dst_len)
						break;

					if (((j + 1) < src_len) && (src[i + 1] >= 0xDC00) && (src[i + 1] <= 0xDFFF))
					{
						lw = ((lw - 0xD7C0) << 10) + ((src[i + 1]) ^ 0xDC00);

						dst[j] = (char)(0xF0 | (lw >> 18));
						dst[j + 1] = (char)(0x80 | ((lw >> 12) & 0x3F));
						dst[j + 2] = (char)(0x80 | ((lw >> 6) & 0x3F));
						dst[j + 3] = (char)(0x80 | (lw & 0x3F));
						j += 4;
						i++;
					}
				}

				i++;
			}
		}
		else
		{
			while ((i < src_len) && (j < dst_len))
			{
				lw = src[i];

				if (lw < 0x80)
					j++;
				else if (lw < 0x7FF)
					j += 2;
				else if ((lw < 0xD7FF) && ((lw >= 0xE000) && (lw <= 0xFFFF)))
					j += 3;
				else if ((lw >= 0xD800) && (lw <= 0xDBFF))
				{
					if (((j + 1) < src_len) && (src[i + 1] >= 0xDC00) && (src[i + 1] <= 0xDFFF))
					{
						j += 4;
						i++;
					}
				}

				i++;
			}
		}

		return j + 1;
	}

	utf8string utf8encode(const unicodestring& str)
	{
		utf8string result;

		if (!str.empty())
		{
			result.resize(str.length() * 4);
			auto i = unicode_to_utf8(str.c_str(), str.length(), result.data(), result.length());
			if (i > 0)
			{
				result.resize(i - 1);
				result.data()[result.length()] = 0;
			}
		}

		return result;
	}

	VOLTEK_UP_API bool is_ascii(const rawbytestring& str)
	{
		for (size_t i = 0; i < str.length(); i++)
			if (str[i] & 0x80) return false;
		return true;
	}

	VOLTEK_UP_API int64_t find_invalid_utf8_codepoint(const char* str, int64_t length, bool stop_on_non_utf8)
	{
		// return -1 if ok
		if (!str || !length)
			return -1;

		auto src = (const uint8_t*)(str);
		int32_t charlen = 0;
		int64_t result = 0;

		while (result < length)
		{
			uint8_t c = *src;
			if (c < 0x80)
				// regular single byte ASCII character
				charlen = 1;
			else if (c <= 0xC1)
			{
				// single byte character, between valid UTF-8 encodings
				// 11000000 and 11000001 map 2 byte to 0..128, which is invalid and used for XSS attacks
				if (stop_on_non_utf8 || (c >= 192))
					return result;
				charlen = 1;
			}
			else if (c <= 0xDF)
			{
				// could be 2 byte character (%110xxxxx %10xxxxxx)
				if ((result < (length - 1)) && ((src[1] & 0xC0) == 0x80))
					charlen = 2;
				else
					// missing following bytes
					return result;
			}
			else if (c <= 0xEF)
			{
				// could be 3 byte character (%1110xxxx %10xxxxxx %10xxxxxx)
				if ((result < (length - 2)) && ((src[1] & 0xC0) == 0x80) && ((src[2] & 0xC0) == 0x80))
				{
					if ((c == 0xE0) && (src[1] <= 0x9F))
						// XSS attack: 3 bytes are mapped to the 1 or 2 byte codes
						return result;
					charlen = 3;
				}
				else
					// missing following bytes
					return result;
			}
			else if (c <= 0xF7)
			{
				// could be 4 byte character (%11110xxx %10xxxxxx %10xxxxxx %10xxxxxx)
				if ((result < (length - 3)) && ((src[1] & 0xC0) == 0x80) && ((src[2] & 0xC0) == 0x80) && ((src[3] & 0xC0) == 0x80))
				{
					if ((c == 0xF0) && (src[1] <= 0x8F))
						// XSS attack: 4 bytes are mapped to the 1-3 byte codes
						return result;

					if ((c > 0xF4) || ((c == 0xF4) && (src[1] > 0x8F)))
						// out of range U+10FFFF
						return result;

					charlen = 4;
				}
				else
					// missing following bytes
					return result;
			}
			else if (stop_on_non_utf8)
			{
				return result;
			}
			else
				charlen = 1;

			result += charlen;
			src += charlen;

			if (result > length)
			{
				// missing following bytes
				result -= charlen;
				return result;
			}
        }

        return -1;
	}

	VOLTEK_UP_API bool is_utf8(const utf8string& str)
	{
		return !str.empty() && (find_invalid_utf8_codepoint(str.c_str(), (int64_t)str.length()) == -1);
	}

	VOLTEK_UP_API ansistring _utf8_to_wincp(const utf8string& str)
	{
		if (is_ascii(str))
			return str;

#ifdef _USE_UTF8CPPLIB
		std::wstring utf16line;
		utf8::utf8to16(str.begin(), str.end(), back_inserter(utf16line));

		if (utf16line.empty())
			return str;

		ansistring ret;
		auto len = WideCharToMultiByte(CP_ACP, 0, utf16line.c_str(), (int)utf16line.length(), nullptr, 0, nullptr, nullptr);

		if (len > 0)
		{
			ret.resize(len);
			WideCharToMultiByte(CP_ACP, 0, utf16line.c_str(), (int)utf16line.length(), ret.data(), len, nullptr, nullptr);
	}
#else
		auto src = utf8decode(str);
		if (src.empty())
			return str;

		ansistring ret;
		auto len = WideCharToMultiByte(CP_ACP, 0, src.c_str(), (int)src.length(), nullptr, 0, nullptr, nullptr);

		if (len > 0)
		{
			ret.resize(len);
			WideCharToMultiByte(CP_ACP, 0, src.c_str(), (int)src.length(), ret.data(), len, nullptr, nullptr);
		}
#endif
		return ret;
	}

	VOLTEK_UP_API utf8string _wincp_to_utf8(const ansistring& str)
	{
		if (is_ascii(str))
			return str;

		auto len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str.c_str(), (int)str.length(), nullptr, 0);
		if (len <= 0)
			return str;

		unicodestring src;
		src.resize(len);
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str.c_str(), (int)str.length(), src.data(), len);

#ifdef _USE_UTF8CPPLIB
		std::string utf8line;
		utf8::utf16to8(str.begin(), str.end(), back_inserter(utf8line));
		return utf8line.c_str();
#else
		return utf8encode(src);
#endif
	}

	VOLTEK_UP_API int utf8_to_wincp(const char* asource, char* adest, bool test_on_invalid)
	{
		if (test_on_invalid && (find_invalid_utf8_codepoint(asource, strlen(asource)) != -1))
			return -1;

		auto ansi = _utf8_to_wincp(asource);
		int alen = (int)ansi.length();
		if (!alen) return 0;
		if (!adest) return alen + 1;
		else memcpy(adest, ansi.c_str(), alen);
		return alen;
	}

	VOLTEK_UP_API int wincp_to_utf8(const char* asource, char* adest)
	{
		auto utf8 = _wincp_to_utf8(asource);
		int alen = (int)utf8.length();
		if (!alen) return 0;
		if (!adest) return alen + 1;
		else memcpy(adest, utf8.c_str(), alen);
		return alen;
	}
}