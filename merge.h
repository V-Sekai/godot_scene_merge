/*************************************************************************/
/*  merge.h                                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

/*
xatlas
https://github.com/jpcy/xatlas
Copyright (c) 2018 Jonathan Young
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/*
thekla_atlas
https://github.com/Thekla/thekla_atlas
MIT License
Copyright (c) 2013 Thekla, Inc
Copyright NVIDIA Corporation 2006 -- Ignacio Castano <icastano@nvidia.com>
*/

#ifndef MERGE_H
#define MERGE_H

#include "core/object/ref_counted.h"

#include "core/math/vector2.h"
#include "core/object/ref_counted.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/main/node.h"

#include "thirdparty/xatlas/xatlas.h"

class MeshMergeMaterialRepack : public RefCounted {
private:
	struct TextureData {
		uint16_t width;
		uint16_t height;
		int numComponents;
		Ref<Image> image;
	};

	struct AtlasLookupTexel {
		uint16_t material_index;
		uint16_t x, y;
	};

	struct SetAtlasTexelArgs {
		Ref<Image> atlasData;
		Ref<Image> sourceTexture;
		AtlasLookupTexel *atlas_lookup = nullptr;
		uint16_t material_index = 0;
		Vector2 source_uvs[3];
		uint32_t atlas_width = 0;
		uint32_t atlas_height = 0;
	};

	const int32_t default_texture_length = 128;

	struct ModelVertex {
		Vector3 pos;
		Vector3 normal;
		Vector2 uv;
	};
	struct MeshState {
		Ref<Mesh> mesh;
		NodePath path;
		MeshInstance3D *mesh_instance;
		bool operator==(const MeshState &rhs) const;
	};
	struct MaterialImageCache {
		Ref<Image> albedo_img;
	};
	struct MergeState {
		Node *p_root;
		xatlas::Atlas *atlas;
		Vector<MeshState> &r_mesh_items;
		Array &vertex_to_material;
		const Vector<Vector<Vector2> > uvs;
		const Vector<Vector<ModelVertex> > &model_vertices;
		String p_name;
		String output_path;
		const xatlas::PackOptions &pack_options;
		Vector<AtlasLookupTexel> &atlas_lookup;
		Vector<Ref<Material> > &material_cache;
		HashMap<String, Ref<Image> > texture_atlas;
		HashMap<int32_t, MaterialImageCache> material_image_cache;
	};
	struct MeshMerge {
		Vector<MeshState> meshes;
		int vertex_count = 0;
	};
	static bool setAtlasTexel(void *param, int x, int y, const Vector3 &bar, const Vector3 &dx, const Vector3 &dy, float coverage);
	Ref<Image> dilate(Ref<Image> source_image);
	void _find_all_mesh_instances(Vector<MeshMerge> &r_items, Node *p_current_node, const Node *p_owner);
	void _generate_texture_atlas(MergeState &state, String texture_type);
	Ref<Image> _get_source_texture(MergeState &state, Ref<BaseMaterial3D> material);
	Error _generate_atlas(const int32_t p_num_meshes, Vector<Vector<Vector2> > &r_uvs, xatlas::Atlas *atlas, const Vector<MeshState> &r_meshes, const Vector<Ref<Material> > material_cache,
			xatlas::PackOptions &pack_options);
	void scale_uvs_by_texture_dimension_larger(const Vector<MeshState> &original_mesh_items, Vector<MeshState> &mesh_items, Vector<Vector<Vector2> > &uv_groups, Array &r_vertex_to_material, Vector<Vector<ModelVertex> > &r_model_vertices);
	void map_mesh_to_index_to_material(Vector<MeshState> mesh_items, Array &vertex_to_material, Vector<Ref<Material> > &material_cache);
	Node *_output(MergeState &state, int p_count);
	struct MeshMergeState {
		Vector<MeshMerge> mesh_items;
		Vector<MeshMerge> original_mesh_items;
		Node *root = nullptr;
		Node *original_root = nullptr;
		String output_path;
	};
	Node *_merge_mesh_instances(MeshMergeState p_mesh_merge_state, int p_index);

protected:
	static void _bind_methods();

public:
	Node *merge(Node *p_root, Node *p_original_root, String p_output_path);
};

#endif // MERGE_H