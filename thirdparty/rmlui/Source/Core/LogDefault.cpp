#include "LogDefault.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"

#ifdef RMLUI_PLATFORM_WIN32_NATIVE
	#include <windows.h>
#else
	#include <stdio.h>
#endif

namespace Rml {

bool LogDefault::LogMessage(Log::Type /*type*/, const String& message)
{
	#ifdef RMLUI_PLATFORM_EMSCRIPTEN
	puts(message.c_str());
	#else
	fprintf(stderr, "%s\n", message.c_str());
	#endif
	return true;
}

} // namespace Rml
