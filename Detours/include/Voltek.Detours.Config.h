#pragma once

#ifndef VOLTEK_LIB_BUILD
#	ifdef DETOURS_EXPORTS
#		define VOLTEK_DETOURS_API __declspec(dllexport)
#	else
#		define VOLTEK_DETOURS_API __declspec(dllimport)
#	endif // VOLTEK_DLL_BUILD
#else
#	define VOLTEK_DETOURS_API
#endif // !VOLTEK_LIB_BUILD