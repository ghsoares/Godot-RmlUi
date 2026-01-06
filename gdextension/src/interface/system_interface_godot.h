#pragma once
#include <RmlUi/Core/SystemInterface.h>
#include <godot_cpp/classes/time.hpp>

namespace godot {

class SystemInterfaceGodot : public Rml::SystemInterface {
public:
	double GetElapsedTime() override {
		return Time::get_singleton()->get_ticks_msec() / 1000.0;
	}

	bool LogMessage(Rml::Log::Type, const Rml::String& msg) override {
		UtilityFunctions::print("[RmlUi] ", msg.c_str());
		return true;
	}
};

}
