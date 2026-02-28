
#include "image_utils.h"

using namespace godot;

using RD = RenderingDevice;

int godot::image_get_format_pixel_size(Image::Format p_format) {
	switch (p_format) {
		case Image::FORMAT_L8:
			return 1;
		case Image::FORMAT_LA8:
			return 2;
		case Image::FORMAT_R8:
			return 1;
		case Image::FORMAT_RG8:
			return 2;
		case Image::FORMAT_RGB8:
			return 3;
		case Image::FORMAT_RGBA8:
			return 4;
		case Image::FORMAT_RGBA4444:
			return 2;
		case Image::FORMAT_RGB565:
			return 2;
		case Image::FORMAT_RF:
			return 4;
		case Image::FORMAT_RGF:
			return 8;
		case Image::FORMAT_RGBF:
			return 12;
		case Image::FORMAT_RGBAF:
			return 16;
		case Image::FORMAT_RH:
			return 2;
		case Image::FORMAT_RGH:
			return 4;
		case Image::FORMAT_RGBH:
			return 6;
		case Image::FORMAT_RGBAH:
			return 8;
		case Image::FORMAT_RGBE9995:
			return 4;
		case Image::FORMAT_DXT1:
			return 1;
		case Image::FORMAT_DXT3:
			return 1;
		case Image::FORMAT_DXT5:
			return 1;
		case Image::FORMAT_RGTC_R:
			return 1;
		case Image::FORMAT_RGTC_RG:
			return 1;
		case Image::FORMAT_BPTC_RGBA:
			return 1;
		case Image::FORMAT_BPTC_RGBF:
			return 1;
		case Image::FORMAT_BPTC_RGBFU:
			return 1;
		case Image::FORMAT_ETC:
			return 1;
		case Image::FORMAT_ETC2_R11:
			return 1;
		case Image::FORMAT_ETC2_R11S:
			return 1;
		case Image::FORMAT_ETC2_RG11:
			return 1;
		case Image::FORMAT_ETC2_RG11S:
			return 1;
		case Image::FORMAT_ETC2_RGB8:
			return 1;
		case Image::FORMAT_ETC2_RGBA8:
			return 1;
		case Image::FORMAT_ETC2_RGB8A1:
			return 1;
		case Image::FORMAT_ETC2_RA_AS_RG:
			return 1;
		case Image::FORMAT_DXT5_RA_AS_RG:
			return 1;
		case Image::FORMAT_ASTC_4x4:
			return 1;
		case Image::FORMAT_ASTC_4x4_HDR:
			return 1;
		case Image::FORMAT_ASTC_8x8:
			return 1;
		case Image::FORMAT_ASTC_8x8_HDR:
			return 1;
		case Image::FORMAT_R16:
			return 2;
		case Image::FORMAT_RG16:
			return 4;
		case Image::FORMAT_RGB16:
			return 6;
		case Image::FORMAT_RGBA16:
			return 8;
		case Image::FORMAT_R16I:
			return 2;
		case Image::FORMAT_RG16I:
			return 4;
		case Image::FORMAT_RGB16I:
			return 6;
		case Image::FORMAT_RGBA16I:
			return 8;
		case Image::FORMAT_MAX: {}
	}
	return 0;
}

int godot::image_get_format_pixel_rshift(Image::Format p_format) {
	if (p_format == Image::FORMAT_ASTC_8x8) {
		return 2;
	} else if (p_format == Image::FORMAT_DXT1 || p_format == Image::FORMAT_RGTC_R || p_format == Image::FORMAT_ETC || p_format == Image::FORMAT_ETC2_R11 || p_format == Image::FORMAT_ETC2_R11S || p_format == Image::FORMAT_ETC2_RGB8 || p_format == Image::FORMAT_ETC2_RGB8A1) {
		return 1;
	} else {
		return 0;
	}
}

int godot::image_get_format_block_size(Image::Format p_format) {
	switch (p_format) {
		case Image::FORMAT_DXT1:
		case Image::FORMAT_DXT3:
		case Image::FORMAT_DXT5:
		case Image::FORMAT_RGTC_R:
		case Image::FORMAT_RGTC_RG: {
			return 4;
		}
		case Image::FORMAT_ETC: {
			return 4;
		}
		case Image::FORMAT_BPTC_RGBA:
		case Image::FORMAT_BPTC_RGBF:
		case Image::FORMAT_BPTC_RGBFU: {
			return 4;
		}
		case Image::FORMAT_ETC2_R11:
		case Image::FORMAT_ETC2_R11S:
		case Image::FORMAT_ETC2_RG11:
		case Image::FORMAT_ETC2_RG11S:
		case Image::FORMAT_ETC2_RGB8:
		case Image::FORMAT_ETC2_RGBA8:
		case Image::FORMAT_ETC2_RGB8A1:
		case Image::FORMAT_ETC2_RA_AS_RG:
		case Image::FORMAT_DXT5_RA_AS_RG: {
			return 4;
		}
		case Image::FORMAT_ASTC_4x4:
		case Image::FORMAT_ASTC_4x4_HDR: {
			return 4;
		}
		case Image::FORMAT_ASTC_8x8:
		case Image::FORMAT_ASTC_8x8_HDR: {
			return 8;
		}
		default: {
		}
	}

	return 1;
}

int64_t godot::image_get_dst_size(int p_width, int p_height, Image::Format p_format, int &r_mipmaps, int p_mipmaps, int *r_mm_width, int *r_mm_height) {
	// Data offset in mipmaps (including the original texture).
	int64_t size = 0;

	int w = p_width;
	int h = p_height;

	// Current mipmap index in the loop below. p_mipmaps is the target mipmap index.
	// In this function, mipmap 0 represents the first mipmap instead of the original texture.
	int mm = 0;

	int pixsize = image_get_format_pixel_size(p_format);
	int pixshift = image_get_format_pixel_rshift(p_format);
	int block = image_get_format_block_size(p_format);

	// Technically, you can still compress up to 1 px no matter the format, so commenting this.
	//int minw, minh;
	//get_format_min_pixel_size(p_format, minw, minh);
	int minw = 1, minh = 1;

	while (true) {
		int bw = w % block != 0 ? w + (block - w % block) : w;
		int bh = h % block != 0 ? h + (block - h % block) : h;

		int64_t s = bw * bh;

		s *= pixsize;
		s >>= pixshift;

		size += s;

		if (p_mipmaps >= 0) {
			w = MAX(minw, w >> 1);
			h = MAX(minh, h >> 1);
		} else {
			if (w == minw && h == minh) {
				break;
			}
			w = MAX(minw, w >> 1);
			h = MAX(minh, h >> 1);
		}

		// Set mipmap size.
		if (r_mm_width) {
			*r_mm_width = w;
		}
		if (r_mm_height) {
			*r_mm_height = h;
		}

		// Reach target mipmap.
		if (p_mipmaps >= 0 && mm == p_mipmaps) {
			break;
		}

		mm++;
	}

	r_mipmaps = mm;
	return size;
}

void godot::image_get_rd_format(RenderingDevice::DataFormat p_rd_format, Image::Format &p_image_format, RenderingDevice::DataFormat &p_srgb_format) {
	p_srgb_format = p_rd_format;
	switch (p_rd_format) {
		case RD::DATA_FORMAT_R8_UNORM: {
			p_image_format = Image::FORMAT_L8;
		} break;
		case RD::DATA_FORMAT_R8G8_UNORM: {
			p_image_format = Image::FORMAT_LA8;
		} break;
		case RD::DATA_FORMAT_R8G8B8_UNORM:
		case RD::DATA_FORMAT_R8G8B8_SRGB: {
			p_image_format = Image::FORMAT_RGB8;
			p_srgb_format = RD::DATA_FORMAT_R8G8B8_SRGB;
		} break;
		case RD::DATA_FORMAT_R8G8B8A8_UNORM:
		case RD::DATA_FORMAT_R8G8B8A8_SRGB: {
			p_image_format = Image::FORMAT_RGBA8;
			p_srgb_format = RD::DATA_FORMAT_R8G8B8A8_SRGB;
		} break;
		case RD::DATA_FORMAT_B8G8R8A8_UNORM:
		case RD::DATA_FORMAT_B8G8R8A8_SRGB: {
			p_image_format = Image::FORMAT_RGBA8;
			p_srgb_format = RD::DATA_FORMAT_B8G8R8A8_SRGB;
		} break;
		case RD::DATA_FORMAT_B4G4R4A4_UNORM_PACK16: {
			p_image_format = Image::FORMAT_RGBA4444;
		} break;
		case RD::DATA_FORMAT_B5G6R5_UNORM_PACK16: {
			p_image_format = Image::FORMAT_RGB565;
		} break;
		case RD::DATA_FORMAT_R32_SFLOAT: {
			p_image_format = Image::FORMAT_RF;
		} break; //float
		case RD::DATA_FORMAT_R32G32_SFLOAT: {
			p_image_format = Image::FORMAT_RGF;
		} break;
		case RD::DATA_FORMAT_R32G32B32_SFLOAT: {
			p_image_format = Image::FORMAT_RGBF;
		} break;
		case RD::DATA_FORMAT_R32G32B32A32_SFLOAT: {
			p_image_format = Image::FORMAT_RGBAF;
		} break;
		case RD::DATA_FORMAT_R16_SFLOAT: {
			p_image_format = Image::FORMAT_RH;
		} break;
		case RD::DATA_FORMAT_R16G16_SFLOAT: {
			p_image_format = Image::FORMAT_RGH;
		} break;
		case RD::DATA_FORMAT_R16G16B16_SFLOAT: {
			p_image_format = Image::FORMAT_RGBH;
		} break;
		case RD::DATA_FORMAT_R16G16B16A16_SFLOAT: {
			p_image_format = Image::FORMAT_RGBAH;
		} break;
		case RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32: {
			p_image_format = Image::FORMAT_RGBE9995;
		} break;
		case RD::DATA_FORMAT_BC1_RGB_UNORM_BLOCK:
		case RD::DATA_FORMAT_BC1_RGB_SRGB_BLOCK: {
			p_image_format = Image::FORMAT_DXT1;
			p_srgb_format = RD::DATA_FORMAT_BC1_RGB_SRGB_BLOCK;
		} break; //s3tc bc1
		case RD::DATA_FORMAT_BC2_UNORM_BLOCK:
		case RD::DATA_FORMAT_BC2_SRGB_BLOCK: {
			p_image_format = Image::FORMAT_DXT3;
			p_srgb_format = RD::DATA_FORMAT_BC2_SRGB_BLOCK;
		} break;
		case RD::DATA_FORMAT_BC3_UNORM_BLOCK:
		case RD::DATA_FORMAT_BC3_SRGB_BLOCK: {
			p_image_format = Image::FORMAT_DXT5;
			p_srgb_format = RD::DATA_FORMAT_BC3_SRGB_BLOCK;
		} break;
		case RD::DATA_FORMAT_BC4_UNORM_BLOCK: {
			p_image_format = Image::FORMAT_RGTC_R;
		} break;
		case RD::DATA_FORMAT_BC5_UNORM_BLOCK: {
			p_image_format = Image::FORMAT_RGTC_RG;
		} break;
		case RD::DATA_FORMAT_BC7_UNORM_BLOCK:
		case RD::DATA_FORMAT_BC7_SRGB_BLOCK: {
			p_image_format = Image::FORMAT_BPTC_RGBA;
			p_srgb_format = RD::DATA_FORMAT_BC7_SRGB_BLOCK;
		} break;
		case RD::DATA_FORMAT_BC6H_SFLOAT_BLOCK: {
			p_image_format = Image::FORMAT_BPTC_RGBF;
		} break;
		case RD::DATA_FORMAT_BC6H_UFLOAT_BLOCK: {
			p_image_format = Image::FORMAT_BPTC_RGBFU;
		} break;
		case RD::DATA_FORMAT_EAC_R11_UNORM_BLOCK: {
			p_image_format = Image::FORMAT_ETC2_R11;
		} break;
		case RD::DATA_FORMAT_EAC_R11_SNORM_BLOCK: {
			p_image_format = Image::FORMAT_ETC2_R11S;
		} break;
		case RD::DATA_FORMAT_EAC_R11G11_UNORM_BLOCK: {
			p_image_format = Image::FORMAT_ETC2_RG11;
		} break;
		case RD::DATA_FORMAT_EAC_R11G11_SNORM_BLOCK: {
			p_image_format = Image::FORMAT_ETC2_RG11S;
		} break;
		case RD::DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case RD::DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: {
			p_image_format = Image::FORMAT_ETC2_RGB8;
			p_srgb_format = RD::DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
		} break;
		case RD::DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case RD::DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: {
			p_image_format = Image::FORMAT_ETC2_RGBA8;
			p_srgb_format = RD::DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
		} break;
		case RD::DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case RD::DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: {
			p_image_format = Image::FORMAT_ETC2_RGB8A1;
			p_srgb_format = RD::DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
		} break;
		case RD::DATA_FORMAT_ASTC_4x4_UNORM_BLOCK: {
			p_image_format = Image::FORMAT_ASTC_4x4;
		} break;
		case RD::DATA_FORMAT_ASTC_4x4_SRGB_BLOCK: {
			p_image_format = Image::FORMAT_ASTC_4x4;
			p_srgb_format = RD::DATA_FORMAT_ASTC_4x4_SRGB_BLOCK;
		} break;
		case RD::DATA_FORMAT_ASTC_4x4_SFLOAT_BLOCK: {
			p_image_format = Image::FORMAT_ASTC_4x4_HDR;
		} break;
		case RD::DATA_FORMAT_ASTC_8x8_UNORM_BLOCK: {
			p_image_format = Image::FORMAT_ASTC_8x8;
		} break;
		case RD::DATA_FORMAT_ASTC_8x8_SRGB_BLOCK: {
			p_image_format = Image::FORMAT_ASTC_8x8;
			p_srgb_format = RD::DATA_FORMAT_ASTC_8x8_SRGB_BLOCK;
		} break;
		case RD::DATA_FORMAT_ASTC_8x8_SFLOAT_BLOCK: {
			p_image_format = Image::FORMAT_ASTC_8x8_HDR;
		} break;
		case RD::DATA_FORMAT_D16_UNORM: {
			p_image_format = Image::FORMAT_R16;
		} break;
		case RD::DATA_FORMAT_D32_SFLOAT: {
			p_image_format = Image::FORMAT_RF;
		} break;
		case RD::DATA_FORMAT_R16_UNORM: {
			p_image_format = Image::FORMAT_R16;
		} break;
		case RD::DATA_FORMAT_R16G16_UNORM: {
			p_image_format = Image::FORMAT_RG16;
		} break;
		case RD::DATA_FORMAT_R16G16B16_UNORM: {
			p_image_format = Image::FORMAT_RGB16;
		} break;
		case RD::DATA_FORMAT_R16G16B16A16_UNORM: {
			p_image_format = Image::FORMAT_RGBA16;
		} break;
		case RD::DATA_FORMAT_R16_UINT: {
			p_image_format = Image::FORMAT_R16I;
		} break;
		case RD::DATA_FORMAT_R16G16_UINT: {
			p_image_format = Image::FORMAT_RG16I;
		} break;
		case RD::DATA_FORMAT_R16G16B16_UINT: {
			p_image_format = Image::FORMAT_RGB16I;
		} break;
		case RD::DATA_FORMAT_R16G16B16A16_UINT: {
			p_image_format = Image::FORMAT_RGBA16I;
		} break;

		default: {
			ERR_FAIL_MSG("Unsupported image format");
		}
	}
}

Image::Format godot::image_get_rd_format(RenderingDevice::DataFormat p_rd_format) {
	Image::Format ret;
	RenderingDevice::DataFormat temp;
	image_get_rd_format(p_rd_format, ret, temp);
	return ret;
}

