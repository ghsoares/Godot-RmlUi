#include "rml_godot_plugin.h"
#include "../server/rml_server.h"

using namespace godot;

RmlPluginGodot *RmlPluginGodot::singleton = nullptr;

RmlPluginGodot *RmlPluginGodot::get_singleton() {
	return singleton;
}

void RmlPluginGodot::set_default_stylesheet(Rml::SharedPtr<Rml::StyleSheetContainer> stylesheet) {
	default_stylesheet = stylesheet;
}

void RmlPluginGodot::OnDocumentLoad(Rml::ElementDocument* document) {
	// Inject user agent stylesheet for default style
	if (default_stylesheet) {
		Rml::SharedPtr<Rml::StyleSheetContainer> new_stylesheet = std::make_shared<Rml::StyleSheetContainer>();
		new_stylesheet->MergeStyleSheetContainer(*default_stylesheet);
		if (document->GetStyleSheetContainer()) {
			new_stylesheet->MergeStyleSheetContainer(*document->GetStyleSheetContainer());
		}
		document->SetStyleSheetContainer(new_stylesheet);
	}
}

RmlPluginGodot::RmlPluginGodot() {
	singleton = this;
}