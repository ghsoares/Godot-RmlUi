#pragma once

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rendering_device.hpp>

namespace godot {

int image_get_format_pixel_size(Image::Format p_format);
int image_get_format_pixel_rshift(Image::Format p_format);
int image_get_format_block_size(Image::Format p_format);
int64_t image_get_dst_size(int p_width, int p_height, Image::Format p_format, int &r_mipmaps, int p_mipmaps, int *r_mm_width = nullptr, int *r_mm_height = nullptr);

void image_get_rd_format(RenderingDevice::DataFormat p_rd_format, Image::Format &p_image_format, RenderingDevice::DataFormat &p_srgb_format);

Image::Format image_get_rd_format(RenderingDevice::DataFormat p_rd_format);

}

