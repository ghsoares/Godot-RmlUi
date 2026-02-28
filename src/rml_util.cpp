#include "rml_util.h"

using namespace godot;

Rml::String godot::godot_to_rml_string(const String &p_str) {
	return Rml::String(p_str.utf8().get_data());
}

String godot::rml_to_godot_string(const Rml::String &p_str) {
	return String(p_str.c_str());
}

Rml::Variant godot::godot_to_rml_variant(const Variant &p_var) {
	switch (p_var.get_type()) {
		case Variant::Type::NIL:
			return Rml::Variant();
		case Variant::Type::BOOL:
			return Rml::Variant((bool)p_var);
		case Variant::Type::INT:
			return Rml::Variant((int)p_var);
		case Variant::Type::FLOAT:
			return Rml::Variant((double)p_var);
		case Variant::Type::STRING:
			return Rml::Variant(godot_to_rml_string(p_var));
		case Variant::Type::VECTOR2: {
			Vector2 v = p_var;
			return Rml::Variant(Rml::Vector2f(v.x, v.y));
		}
		case Variant::Type::VECTOR3: {
			Vector3 v = p_var;
			return Rml::Variant(Rml::Vector3f(v.x, v.y, v.z));
		}
		case Variant::Type::VECTOR4: {
			Vector4 v = p_var;
			return Rml::Variant(Rml::Vector4f(v.x, v.y, v.z, v.w));
		}
		case Variant::Type::COLOR: {
			Color v = p_var;
			return Rml::Variant(Rml::Colourf(v.r * 255, v.g * 255, v.b * 255, v.a * 255));
		}
		default: {
			ERR_FAIL_V_MSG(Rml::Variant(), vformat("Unimplemented for godot variant type %d", p_var.get_type()));
		} break;
	}
}

Variant godot::rml_to_godot_variant(const Rml::Variant &p_var) {
	switch (p_var.GetType()) {
		case Rml::Variant::Type::NONE:
			return Variant();
		case Rml::Variant::Type::BOOL:
			return Variant(p_var.Get<bool>());
		case Rml::Variant::Type::INT:
			return Variant(p_var.Get<int>());
		case Rml::Variant::Type::FLOAT:
			return Variant(p_var.Get<double>());
		case Rml::Variant::Type::STRING:
			return Variant(rml_to_godot_string(p_var.Get<Rml::String>()));
		case Rml::Variant::Type::VECTOR2: {
			Rml::Vector2f v = p_var.Get<Rml::Vector2f>();
			return Variant(Vector2(v.x, v.y));
		}
		case Rml::Variant::Type::VECTOR3: {
			Rml::Vector3f v = p_var.Get<Rml::Vector3f>();
			return Variant(Vector3(v.x, v.y, v.z));
		}
		case Rml::Variant::Type::VECTOR4: {
			Rml::Vector4f v = p_var.Get<Rml::Vector4f>();
			return Variant(Vector4(v.x, v.y, v.z, v.w));
		}
		case Rml::Variant::Type::COLOURF: {
			Rml::Colourf v = p_var.Get<Rml::Colourf>();
			return Variant(Color(v.red / 255, v.green / 255, v.blue / 255, v.alpha / 255));
		}
		default: {
			ERR_FAIL_V_MSG(Variant(), vformat("Unimplemented for RML variant type %d", p_var.GetType()));
		} break;
	}
}

Projection godot::rml_to_godot_projection(const Rml::Matrix4f &p_mat) {
	Rml::Vector4f c0, c1, c2, c3;
	c0 = p_mat.GetColumn(0);
	c1 = p_mat.GetColumn(1);
	c2 = p_mat.GetColumn(2);
	c3 = p_mat.GetColumn(3);
	return Projection(
	    c0.x, c0.y, c0.z, c0.w,
	    c1.x, c1.y, c1.z, c1.w,
	    c2.x, c2.y, c2.z, c2.w,
	    c3.x, c3.y, c3.z, c3.w
	);
}

