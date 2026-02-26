#include "file_interface_godot.h"
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include "../rml_util.h"

using namespace godot;

size_t read_bytes(const PackedByteArray &p_arr, void *p_dest, size_t p_offset, size_t p_bytes) {
	p_bytes = MIN(p_bytes, p_offset - p_arr.size());
	memcpy(p_dest, p_arr.ptr(), p_bytes);
	return p_bytes;
}

Rml::FileHandle FileInterfaceGodot::Open(const Rml::String& path) {
	String godot_path = rml_to_godot_string(path);
	
	FileHandle handle;
	bool loaded_from_bytes = false;

	if (ResourceLoader::get_singleton()->exists(godot_path)) {
		Ref<Resource> res = ResourceLoader::get_singleton()->load(godot_path);
		Ref<FontFile> font_file = res;
		if (font_file.is_valid()) {
			handle.byte_file = memnew(ByteFileAccess);
			handle.byte_file->bytes = font_file->get_data();
			handle.byte_file->position = 0;
			loaded_from_bytes = true;
		}
	}
	if (!loaded_from_bytes) {
		Ref<FileAccess> file = FileAccess::open(godot_path, FileAccess::READ);
		if (file == nullptr) {
			return 0;
		}
		handle.file = file;
	}
	 
	FileHandle *file_data = memnew(FileHandle(handle));
    return reinterpret_cast<uintptr_t>(file_data);
}

void FileInterfaceGodot::Close(Rml::FileHandle file) {
	FileHandle *file_data = reinterpret_cast<FileHandle *>(file);
	if (file_data->is_file()) {
		file_data->file->close();
		file_data->file.unref();
	}
}

size_t FileInterfaceGodot::Read(void* buffer, size_t size, Rml::FileHandle file) {
	FileHandle *file_data = reinterpret_cast<FileHandle *>(file);
	if (file_data->is_file()) {
		uint8_t *buf = static_cast<uint8_t*>(buffer);
		return file_data->file->get_buffer(buf, size);
	}
	size_t bytes_read = read_bytes(file_data->byte_file->bytes, buffer, file_data->byte_file->position, size);
	file_data->byte_file->position += bytes_read;
	return bytes_read;
}

bool FileInterfaceGodot::Seek(Rml::FileHandle file, long offset, int origin) {
	FileHandle *file_data = reinterpret_cast<FileHandle *>(file);
	if (file_data->is_file()) {
		switch (origin) {
			case SEEK_SET: {
				file_data->file->seek(offset);
			} break;
			case SEEK_CUR: {
				file_data->file->seek(file_data->file->get_position() + offset);
			} break;
			case SEEK_END: {
				file_data->file->seek_end(offset);
			} break;
		}
	} else {
		switch (origin) {
			case SEEK_SET: {
				file_data->byte_file->position = offset;
			} break;
			case SEEK_CUR: {
				file_data->byte_file->position += offset;
			} break;
			case SEEK_END: {
				file_data->byte_file->position = file_data->byte_file->bytes.size() - 1 - offset;
			} break;
		}
	}
	return true;
}

size_t FileInterfaceGodot::Tell(Rml::FileHandle file) {
	FileHandle *file_data = reinterpret_cast<FileHandle *>(file);
	if (file_data->is_file()) {
		return file_data->file->get_position();
	}
	return file_data->byte_file->position;
}