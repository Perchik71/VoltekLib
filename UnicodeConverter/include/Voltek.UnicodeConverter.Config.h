﻿// Copyright © 2023 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: https://www.gnu.org/licenses/lgpl-3.0.html

#pragma once

#ifndef VOLTEK_LIB_BUILD
#	ifdef UNICODECONVERTER_EXPORTS
#		define VOLTEK_UP_API __declspec(dllexport)
#	else
#		define VOLTEK_UP_API __declspec(dllimport)
#	endif // VOLTEK_DLL_BUILD
#else
#	define VOLTEK_UP_API
#endif // !VOLTEK_LIB_BUILD
