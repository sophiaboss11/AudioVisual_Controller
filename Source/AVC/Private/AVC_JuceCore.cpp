// Compiles the JUCE core module into the AVC module.
#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
// Unreal undefines these Win32 macros; JUCE relies on them.
#ifndef MoveFile
#define MoveFile MoveFileW
#endif
#ifndef GetFileAttributes
#define GetFileAttributes GetFileAttributesW
#endif
#ifndef DeleteFile
#define DeleteFile DeleteFileW
#endif
#ifndef CreateDirectory
#define CreateDirectory CreateDirectoryW
#endif
#ifndef CopyFile
#define CopyFile CopyFileW
#endif
#ifndef InterlockedIncrement
#define InterlockedIncrement _InterlockedIncrement
#endif
#ifndef InterlockedDecrement
#define InterlockedDecrement _InterlockedDecrement
#endif
#ifndef CaptureStackBackTrace
#define CaptureStackBackTrace RtlCaptureStackBackTrace
#endif
#endif

THIRD_PARTY_INCLUDES_START
#include <juce_core/juce_core.cpp>
#include <juce_core/juce_core_CompilationTime.cpp>
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif
