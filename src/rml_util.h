#pragma once

#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/Variant.h>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/core/type_info.hpp>

#include <godot_cpp/classes/global_constants.hpp>
#include <RmlUi/Core/Input.h>

namespace godot {

Rml::String godot_to_rml_string(const String &p_str);
String rml_to_godot_string(const Rml::String &p_str);

Rml::Variant godot_to_rml_variant(const Variant &p_var);
Variant rml_to_godot_variant(const Rml::Variant &p_var);

Rml::Input::KeyIdentifier godot_to_rml_key(const Key &p_key);
Key rml_to_godot_key(const Rml::Input::KeyIdentifier &p_key);

Rml::Input::KeyModifier godot_to_rml_key_modifiers(const BitField<KeyModifierMask> &p_mod);
BitField<KeyModifierMask> rml_to_godot_key_modifiers(const Rml::Input::KeyModifier &p_mod);

Projection rml_to_godot_projection(const Rml::Matrix4f &p_mat);

}

