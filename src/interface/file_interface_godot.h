#pragma once
#include <RmlUi/Core/FileInterface.h>
#include <godot_cpp/classes/file_access.hpp>

namespace godot {

class FileInterfaceGodot : public Rml::FileInterface {
	struct ByteFileAccess {
		PackedByteArray bytes;
		size_t position = 0;
	};
	struct FileHandle {
		Ref<FileAccess> file;
		ByteFileAccess *byte_file = nullptr;

		bool is_file() const { return file.is_valid(); }
	};

public:
	Rml::FileHandle Open(const Rml::String& path) override;
	void Close(Rml::FileHandle file) override;
	size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
	bool Seek(Rml::FileHandle file, long offset, int origin) override;
	size_t Tell(Rml::FileHandle file) override;
};

}
