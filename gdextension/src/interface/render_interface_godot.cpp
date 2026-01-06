#include "render_interface_godot.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/texture.hpp>

using namespace godot;

RenderInterfaceGodot *RenderInterfaceGodot::singleton = nullptr;

RenderInterfaceGodot *RenderInterfaceGodot::get_singleton() {
    return singleton;
}

void RenderInterfaceGodot::set_drawing_canvas_item(const RID &p_ci) {
    canvas_item = p_ci;
}

Rml::CompiledGeometryHandle RenderInterfaceGodot::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
    PackedVector2Array points;
    PackedVector2Array uvs;
    PackedColorArray colors;
    PackedInt32Array indexes;

    points.resize(vertices.size());
    uvs.resize(vertices.size());
    colors.resize(vertices.size());
    indexes.resize(indices.size());

    for (int i = vertices.size() - 1; i >= 0; i--) {
        points[i] = Vector2(
            vertices[i].position.x,
            vertices[i].position.y
        );
        uvs[i] = Vector2(
            vertices[i].tex_coord.x,
            vertices[i].tex_coord.y
        );
        colors[i] = Color(
            vertices[i].colour.red / 255.0,
            vertices[i].colour.green / 255.0,
            vertices[i].colour.blue / 255.0,
            vertices[i].colour.alpha / 255.0
        );
    }

    for (int i = indices.size() - 1; i >= 0; i--) {
        indexes[i] = indices[i];
    }

    GeometryData *render_data = memnew(GeometryData());
    render_data->indices = indexes;
    render_data->points = points;
    render_data->colors = colors;
    render_data->uvs = uvs;

    return reinterpret_cast<uintptr_t>(render_data);
}

void RenderInterfaceGodot::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
    GeometryData *render_data = reinterpret_cast<GeometryData *>(geometry);

    RenderingServer *rs = RenderingServer::get_singleton();

    PackedVector2Array new_points;
    new_points.resize(render_data->points.size());
    for (int i = new_points.size() - 1; i >= 0; i--) {
        new_points[i] = Vector2(
            render_data->points[i].x + translation.x,
            render_data->points[i].y + translation.y
        );
    }

    rs->canvas_item_add_triangle_array(
        canvas_item,
        render_data->indices,
        new_points,
        render_data->colors,
        render_data->uvs
    );
}

void RenderInterfaceGodot::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
    GeometryData *render_data = reinterpret_cast<GeometryData *>(geometry);
    memdelete(render_data);
}

Rml::TextureHandle RenderInterfaceGodot::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    ResourceLoader *rl = ResourceLoader::get_singleton();

    String source_str = String(source.c_str());

    Ref<Texture> tex = rl->load(source_str);
    if (!tex.is_valid()) {
        return 0;
    }

    TextureData *tex_data = memnew(TextureData());
    tex_data->is_generated = false;
    tex_data->rid = tex->get_rid();

    return reinterpret_cast<uintptr_t>(tex_data);
}

Rml::TextureHandle RenderInterfaceGodot::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
    PackedByteArray p_data;
    p_data.resize(source.size());

    uint8_t *ptrw = p_data.ptrw();

    memcpy(ptrw, source.data(), source.size());

    Ref<Image> img = Image::create_from_data(source_dimensions.x, source_dimensions.y, false, Image::FORMAT_RGBA8, p_data);
    if (!img.is_valid()) {
        return 0;
    }

    RenderingServer *rs = RenderingServer::get_singleton();
    RID tex_rid = rs->texture_2d_create(img);

    TextureData *tex_data = memnew(TextureData());
    tex_data->is_generated = true;
    tex_data->rid = tex_rid;

    return reinterpret_cast<uintptr_t>(tex_data);
}

void RenderInterfaceGodot::ReleaseTexture(Rml::TextureHandle texture) {
    TextureData *tex_data = reinterpret_cast<TextureData *>(texture);
    if (tex_data->is_generated) {
        RenderingServer *rs = RenderingServer::get_singleton();
        rs->free_rid(tex_data->rid);
    }
    memdelete(tex_data);
}

void RenderInterfaceGodot::EnableScissorRegion(bool enable) {}
void RenderInterfaceGodot::SetScissorRegion(Rml::Rectanglei region) {}

RenderInterfaceGodot::RenderInterfaceGodot() {
    singleton = this;
}
RenderInterfaceGodot::~RenderInterfaceGodot() {
    singleton = nullptr;
}
