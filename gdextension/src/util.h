#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#ifndef __FUNCTION_NAME__
    #ifdef WIN32   //WINDOWS
        #define __FUNCTION_NAME__   __FUNCTION__  
    #else          //*NIX
        #define __FUNCTION_NAME__   __func__ 
    #endif
#endif

#define DEG_TO_RAD (Math_PI / 180.0)
#define RAD_TO_DEG (180.0 / Math_PI)

#define GP_STR(x) #x
#define GP_TOSTR(x) GP_STR(x)

#define DBG_STD_COUT

#ifdef DBG_STD_COUT
#define DBG() std::cout << __FILE__ << ":" << __FUNCTION_NAME__ << ":" << __LINE__ << std::endl << std::flush;
#else
#define DBG() UtilityFunctions::print(vformat("%s:%s:%s", __FILE__, __FUNCTION_NAME__, __LINE__));
#endif

#define MAKE_RESOURCE_TYPE_HINT(m_type) vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, m_type)

#define DEFINE_G(p_type, p_name) \
inline p_type get_##p_name() { return p_name; }

#define DEFINE_GS(p_type, p_name) \
inline void set_##p_name(p_type p_##p_name) { p_name = p_##p_name; } \
inline p_type get_##p_name() { return p_name; }

#define DEFINE_GS_EMIT_CHANGED(p_type, p_name) \
inline void set_##p_name(p_type p_##p_name) { p_name = p_##p_name; emit_changed(); } \
inline p_type get_##p_name() { return p_name; }

#define DEFINE_GS_REF(p_type, p_name) \
inline void set_##p_name(const Ref<p_type> p_##p_name) { p_name = p_##p_name; } \
inline Ref<p_type> get_##p_name() const { return p_name; }

#define DEFINE_GS_REF_EMIT_CHANGED(p_type, p_name) \
inline void set_##p_name(const Ref<p_type> p_##p_name) { \
    if (p_name.is_valid()) p_name->disconnect("changed", Callable(this, "emit_changed")); \
    p_name = p_##p_name; \
    if (p_name.is_valid()) p_name->connect("changed", Callable(this, "emit_changed")); \
    emit_changed(); \
} \
inline Ref<p_type> get_##p_name() const { return p_name; }

#define BIND_G_METHOD(p_class, p_name) \
ClassDB::bind_method(D_METHOD("get_" #p_name), &p_class::get_##p_name);

#define BIND_S_METHOD(p_class, p_name) \
ClassDB::bind_method(D_METHOD("set_" #p_name, #p_name), &p_class::set_##p_name);

#define BIND_GS_METHOD(p_class, p_name) \
BIND_G_METHOD(p_class, p_name) \
BIND_S_METHOD(p_class, p_name)

#define BIND_GS_PROPERTY(p_class, p_type, p_name, p_hint, p_hint_string, p_usage) \
BIND_GS_METHOD(p_class, p_name) \
ClassDB::add_property(#p_class, PropertyInfo(p_type, #p_name, p_hint, p_hint_string, p_usage), "set_" #p_name, "get_" #p_name);

#define BIND_G_PROPERTY(p_class, p_type, p_name, p_hint, p_hint_string, p_usage) \
BIND_G_METHOD(p_class, p_name) \
ClassDB::add_property(#p_class, PropertyInfo(p_type, #p_name, p_hint, p_hint_string, p_usage), "", "get_" #p_name);

#define BIND_S_PROPERTY(p_class, p_type, p_name, p_hint, p_hint_string, p_usage) \
BIND_S_METHOD(p_class, p_name) \
ClassDB::add_property(#p_class, PropertyInfo(p_type, #p_name, p_hint, p_hint_string, p_usage), "set_" #p_name, "");

#define BIND_GS_RES_PROPERTY(p_class, p_resource_type, p_name, p_usage) \
BIND_GS_PROPERTY(p_class, Variant::OBJECT, p_name, PROPERTY_HINT_RESOURCE_TYPE, "" #p_resource_type, p_usage)

#define BIND_G_RES_PROPERTY(p_class, p_resource_type, p_name, p_usage) \
BIND_G_PROPERTY(p_class, Variant::OBJECT, p_name, PROPERTY_HINT_RESOURCE_TYPE, "" #p_resource_type, p_usage)

#define BIND_S_RES_PROPERTY(p_class, p_resource_type, p_name, p_usage) \
BIND_S_PROPERTY(p_class, Variant::OBJECT, p_name, PROPERTY_HINT_RESOURCE_TYPE, "" #p_resource_type, p_usage)

template <typename T>
T wrap(T p_val, T &p_len) {
    while (p_val < 0.0) {
        p_val += p_len;
    }
    while (p_val >= p_len) {
        p_val -= p_len;
    }
    return p_val;
}

namespace godot {

template <class V>
using HashSetIt = typename HashSet<V>::Iterator;

template <class K, class V>
using HashMapIt = typename HashMap<K, V>::Iterator;

}