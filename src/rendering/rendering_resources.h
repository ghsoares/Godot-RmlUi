#pragma once

#include <map>
#include <set>
#include <functional>

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/texture.hpp>
#include <godot_cpp/templates/vector.hpp>

namespace godot {

#define DEFINE_RENDERING_RESOURCE(p_name) \
private: ResourceMap p_name ##_map = ResourceMap(#p_name); std::map<uint64_t, RID> p_name ##_cache;\
public: \
	RID create_##p_name(const std::map<String, Variant> &p_data = std::map<String, Variant>());\
	void free_##p_name(const RID &p_id); \
	RID get_or_create_##p_name(uint64_t p_id, const std::function<std::map<String, Variant>()> &p_get_data);

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
	DEFINE_RENDERING_RESOURCE(render_pipeline)
	DEFINE_RENDERING_RESOURCE(compute_pipeline)
	DEFINE_RENDERING_RESOURCE(vertex_buffer)
	DEFINE_RENDERING_RESOURCE(index_buffer)
	DEFINE_RENDERING_RESOURCE(vertex_array)
	DEFINE_RENDERING_RESOURCE(index_array)
	DEFINE_RENDERING_RESOURCE(storage_buffer)

	RenderingDevice *device() const { return rendering_device; }

	void free_all_resources();

	RenderingResources() {}
	RenderingResources(RenderingDevice *);
	~RenderingResources();
};

struct Pipeline {
	struct PipelineVersion {
		int64_t vertex_format_id;
		int64_t framebuffer_format_id;
		bool clipping;
		RID pipeline;
	};

	RenderingResources *resources = nullptr;

	RID shader;
	std::vector<PipelineVersion> pipeline_versions;
	std::map<String, Variant> base_params;

	RID get_pipeline_variant(int64_t p_vertex_format, int64_t p_framebuffer_format, bool p_clipping);
	void free(bool p_shader = false);

	Pipeline(RenderingResources *p_resources, const RID &p_shader, const std::map<String, Variant> &p_params);
};

#undef DEFINE_RENDERING_RESOURCE

}