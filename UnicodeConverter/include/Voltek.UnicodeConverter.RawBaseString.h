// Copyright © 2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: MIT

#pragma once

#define _ALIGN(val, grow) ((val + grow - 1) & ~(grow - 1))

#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <limits.h>

namespace voltek
{
	template<typename _Ty>
	class vbasestring
	{
	public:
		inline static const _Ty EMPTY_STRING[1] = { 0 };
	private:
		_Ty* _raw;
		uint32_t _len;
		uint32_t _size;
	public:
		vbasestring() : _raw(nullptr), _len(0), _size(0) {}
		vbasestring(const _Ty c) : _raw(nullptr), _len(0), _size(0) { assign(c); }
		vbasestring(const _Ty* s) : _raw(nullptr), _len(0), _size(0) { assign(s); }
		vbasestring(const _Ty* s, size_t len) : _raw(nullptr), _len(0), _size(0) { assign(s, len); }
		vbasestring(const vbasestring& rhs) : _raw(nullptr), _len(0), _size(0) { assign(rhs); }
		~vbasestring() { clear(); }

		bool resize(size_t len)
		{
			if (!len)
			{
				if (!empty())
				{
					free(_raw);
					_raw = nullptr;
					_len = 0;
					_size = 0;

					return true;
				}
			}
			else
			{
				uint32_t newsize = _ALIGN((((uint32_t)len + 1) * sizeof(_Ty)), 32);
				
				void* newptr = !empty() ? realloc(_raw, newsize) : malloc(newsize);
				if (newptr)
				{
					_raw = (_Ty*)newptr;
					_len = (uint32_t)len;
					_size = newsize;
					return true;
				}
			}

			return false;
		}

		inline _Ty* begin() const { return _raw; }
		inline _Ty* end() const { return _raw + _len + 1; }

		inline void clear() { resize(0); }
		inline _Ty* data() { return _raw; }
		inline const _Ty* c_str() const { return empty() ? EMPTY_STRING : _raw; }
		inline size_t length() const { return (size_t)_len; }
		inline size_t allocate_size() const { return (size_t)_size; }
		inline bool empty() const { return !_raw; }
		inline vbasestring duplicate() const { return *this; }

		inline _Ty& operator[](size_t index) { return _raw[index]; }
		inline _Ty operator[](size_t index) const { return _raw[index]; }

		inline static size_t calculate_length(const _Ty* s)
		{
			size_t len = 0;
			for (; s[len] != 0; len++) {}
			return len;
		}

		inline vbasestring& assign(const _Ty c) { return assign({ c, 0 }, 1); }
		inline vbasestring& assign(const _Ty* s) { return assign(s, calculate_length(s)); }

		vbasestring& assign(const _Ty* s, size_t len) 
		{ 
			if (!s || !len || (len >= UINT32_MAX) || !resize(len))
				return *this;
			
			memcpy(_raw, s, len * sizeof(_Ty));
			_raw[len] = 0;

			return *this;
		}

		inline vbasestring& assign(const vbasestring& rhs) { return assign(rhs._raw, rhs._len); }

		inline vbasestring& operator=(const _Ty c) { return assign(c); }
		inline vbasestring& operator=(const _Ty* s) { return assign(s); }
		inline vbasestring& operator=(const vbasestring& rhs) { return assign(rhs); }

		inline vbasestring& append(_Ty c) { return append({ c, 0 }, 1); }
		inline vbasestring& append(const _Ty* s) { return append(s, calculate_length(s)); }

		vbasestring& append(const _Ty* s, size_t len)
		{
			if (!s || !len || (len >= UINT32_MAX))
				return *this;

			size_t none_type_size = (size_t)_size / sizeof(_Ty);
			size_t free_size = none_type_size - (size_t)_len;
			auto old_len = _len;

			if (len > free_size)
			{
				if (!resize(_len + len))
					return *this;
			}
			else
				_len += (uint32_t)len;

			memcpy(_raw + old_len, s, len * sizeof(_Ty));	
			_raw[_len] = 0;

			return *this;
		}


		inline vbasestring& append(const vbasestring& rhs) { return append(rhs._raw, rhs._len); }

		inline bool operator==(const vbasestring& rhs) const { return (_len == rhs.length()) ? (!memcmp(_raw, rhs._raw, _len)) : false; }
		inline bool operator!=(const vbasestring& rhs) const { return (_len == rhs.length()) ? (memcmp(_raw, rhs._raw, _len) != 0) : true; }

		inline vbasestring& operator+(_Ty rawchar) const { vbasestring ret(*this); return ret.append(rawchar); }
		inline vbasestring& operator+=(_Ty rawchar) { return append(rawchar); }
		inline vbasestring& operator+(const _Ty* rawstr) const { vbasestring ret(*this); return ret.append(rawstr); }
		inline vbasestring& operator+=(const _Ty* rawstr) { return append(rawstr); }
		inline vbasestring& operator+(const vbasestring& str) const { vbasestring ret(*this); return ret.append(str); }
		inline vbasestring& operator+=(const vbasestring& str) { return append(str); }
	};

	typedef vbasestring<char> rawbytestring, ansistring, utf8string;
	typedef vbasestring<wchar_t> unicodestring;
}