#include "merge_plugin.h"

SceneMergePlugin::~SceneMergePlugin() {
	EditorNode::get_singleton()->remove_tool_menu_item("Merge Scene");
}

void SceneMergePlugin::_action() {
	Node *node = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	if (!node) {
		EditorNode::get_singleton()->show_accept(TTR("This operation can't be done without a scene."), TTR("OK"));
		return;
	}
	scene_optimize->merge(node);
	EditorFileSystem::get_singleton()->scan_changes();
}

SceneMergePlugin::SceneMergePlugin() {
	EditorNode::get_singleton()->add_tool_menu_item("Merge Scene", callable_mp(this, &SceneMergePlugin::_action));
}
