#include "scene_merge.h"

#include "modules/scene_merge/merge.h"
#include "scene/resources/packed_scene.h"

void SceneMerge::merge(const String p_file, Node *p_root_node) {
	PackedScene *scene = memnew(PackedScene);
	scene->pack(p_root_node);
	Node *root = scene->instantiate();
	Ref<MeshMergeMaterialRepack> repack;
	repack.instantiate();
	root = repack->merge(root, p_root_node, p_file);
	ERR_FAIL_COND(!root);
	scene->pack(root);
	ResourceSaver::save(scene, p_file);
}
