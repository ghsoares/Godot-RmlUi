#pragma once

#include <map>
#include <set>

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/texture.hpp>
#include <godot_cpp/templates/vector.hpp>

namespace godot {

struct MeshData {
	RID position_buffer;
	RID color_buffer;
	RID uv_buffer;
	RID index_buffer;
	RID vertex_array;
	RID index_array;
};

struct TextureData {
	RID rid;
	Ref<Texture> tex_ref;
};

// #define SWAP(a, b) { auto t = a; a = b; b = t; }

struct RenderTarget {
	RID color0, color1;
	RID alpha_mask0, alpha_mask1;
	RID framebuffer0, framebuffer1;
};

struct RenderFrame {
	RenderTarget main_target;
	RID main_tex;

	RID clip_mask;
	RID clip_mask_framebuffer;

	Vector2i current_size;
	Vector2i desired_size;

	bool is_valid() { return main_tex.is_valid(); }

	RID get_texture() { return main_tex; }
};

#define DEFINE_RENDERING_RESOURCE(p_name) \
private: ResourceMap p_name ##_map = ResourceMap(#p_name);\
public: \
	RID create_##p_name(const std::map<String, Variant> &p_data = std::map<String, Variant>());\
	void free_##p_name(const RID &p_id);

class RenderingResources {
private:
	struct ResourceMap {
		std::set<RID> resources;
		String resource_name;

		void insert(const RID &p_rid) {
			resources.insert(p_rid);
		}

		void erase(const RID &p_rid) {
			resources.erase(p_rid);
		}

		std::set<RID>::iterator begin() {
			return resources.begin();
		}

		std::set<RID>::iterator end() {
			return resources.end();
		}

		std::set<RID>::iterator find(const RID &p_rid) {
			return resources.find(p_rid);
		}

		std::set<RID>::const_iterator find(const RID &p_rid) const {
			return resources.find(p_rid);
		}

		std::set<RID>::const_iterator begin() const {
			return resources.begin();
		}

		std::set<RID>::const_iterator end() const {
			return resources.end();
		}

		void clear() {
			resources.clear();
		}

		ResourceMap() {}
		ResourceMap(const String &p_name) {
			resource_name = p_name;
		}
	};

	RenderingDevice *rendering_device = nullptr;

	void map_resource(const RID &p_rid, ResourceMap &p_map);
	void free_resource(const RID &p_rid, ResourceMap &p_map);
	void free_all_resources(ResourceMap &p_map);

public:
	DEFINE_RENDERING_RESOURCE(sampler)
	DEFINE_RENDERING_RESOURCE(texture)
	DEFINE_RENDERING_RESOURCE(framebuffer)
	DEFINE_RENDERING_RESOURCE(shader)
	DEFINE_RENDERING_RESOURCE(pipeline)
	DEFINE_RENDERING_RESOURCE(vertex_buffer)
	DEFINE_RENDERING_RESOURCE(index_buffer)
	DEFINE_RENDERING_RESOURCE(vertex_array)
	DEFINE_RENDERING_RESOURCE(index_array)

	RenderingDevice *device() const { return rendering_device; }

	void free_all_resources();

	RenderingResources() {}
	RenderingResources(RenderingDevice *);
	~RenderingResources();
};

#undef DEFINE_RENDERING_RESOURCE

}