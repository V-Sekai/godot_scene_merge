/**************************************************************************/
/*  merge.cpp                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

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

#include "core/core_bind.h"
#include "core/io/image.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/os/os.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_file_dialog.h"
#include "modules/scene_merge/mesh_merge_triangle.h"
#include "scene/3d/node_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/mesh_data_tool.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/surface_tool.h"

#include "thirdparty/misc/rjm_texbleed.h"
#include "thirdparty/xatlas/xatlas.h"
#include <time.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "merge.h"

bool MeshMergeMeshInstanceWithMaterialAtlas::setAtlasTexel(void *param, int x, int y, const Vector3 &bar, const Vector3 &, const Vector3 &, float) {
	SetAtlasTexelArgs *args = static_cast<SetAtlasTexelArgs *>(param);
	if (args->sourceTexture.is_valid()) {
		// Interpolate source UVs using barycentrics.
		const Vector2 sourceUv = args->source_uvs[0] * bar.x + args->source_uvs[1] * bar.y + args->source_uvs[2] * bar.z;

		// Keep coordinates in range of texture dimensions.
		int _width = args->sourceTexture->get_width() - 1;
		int _height = args->sourceTexture->get_height() - 1;

		int sx = static_cast<int>(sourceUv.x * _width) % _width;
		int sy = static_cast<int>(sourceUv.y * _height) % _height;

		if (sx < 0) {
			sx += _width;
		}
		if (sy < 0) {
			sy += _height;
		}

		const Color color = args->sourceTexture->get_pixel(sx, sy);
		args->atlasData->set_pixel(x, y, color);

		AtlasLookupTexel &lookup = args->atlas_lookup[x * y + args->atlas_width];
		lookup.material_index = args->material_index;
		lookup.x = static_cast<uint16_t>(sx);
		lookup.y = static_cast<uint16_t>(sy);

		return true;
	}
	return false;
}

void MeshMergeMeshInstanceWithMaterialAtlas::_find_all_mesh_instances(Vector<MeshMerge> &r_items, Node *p_current_node, const Node *p_owner) {
	MeshInstance3D *mi = cast_to<MeshInstance3D>(p_current_node);
	if (mi && mi->is_visible() && mi->get_mesh().is_valid()) {
		Ref<Mesh> array_mesh = mi->get_mesh();
		bool has_blends = false, has_bones = false, has_transparency = false;

		// Store the original materials
		Vector<Ref<Material>> original_materials;
		for (int32_t surface_i = 0; surface_i < array_mesh->get_surface_count(); surface_i++) {
			original_materials.push_back(array_mesh->surface_get_material(surface_i));
		}

		for (int32_t surface_i = 0; surface_i < array_mesh->get_surface_count(); surface_i++) {
			Ref<Material> active_material = mi->get_active_material(surface_i);
			if (active_material.is_null()) {
				active_material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
			}
			array_mesh->surface_set_material(surface_i, active_material);

			Array array = array_mesh->surface_get_arrays(surface_i).duplicate(true);
			has_bones |= PackedFloat32Array(array[ArrayMesh::ARRAY_BONES]).size() != 0;
			has_blends |= array_mesh->get_blend_shape_count() != 0;
			Ref<BaseMaterial3D> base_mat = array_mesh->surface_get_material(surface_i);
			if (base_mat.is_valid()) {
				has_transparency |= base_mat->get_transparency() != BaseMaterial3D::TRANSPARENCY_DISABLED;
			}

			MeshState mesh_state;
			mesh_state.mesh = array_mesh;
			if (mi->is_inside_tree()) {
				mesh_state.path = mi->get_path();
			}
			mesh_state.mesh_instance = mi;
			MeshMerge &mesh = r_items.write[r_items.size() - 1];
			mesh.vertex_count += PackedVector3Array(array[ArrayMesh::ARRAY_VERTEX]).size();
			mesh.meshes.push_back(mesh_state);
		}
	}

	for (int32_t child_i = 0; child_i < p_current_node->get_child_count(); child_i++) {
		_find_all_mesh_instances(r_items, p_current_node->get_child(child_i), p_owner);
	}
}


void MeshMergeMeshInstanceWithMaterialAtlas::_bind_methods() {
	ClassDB::bind_method(D_METHOD("merge", "root", "original_root", "output_path"), &MeshMergeMeshInstanceWithMaterialAtlas::merge);
}

Node *MeshMergeMeshInstanceWithMaterialAtlas::merge(Node *p_root, Node *p_original_root, String p_output_path) {
	MeshMergeState mesh_merge_state;
	mesh_merge_state.root = p_root;
	mesh_merge_state.original_root = p_original_root;
	mesh_merge_state.output_path = p_output_path;
	mesh_merge_state.mesh_items.resize(1);
	_find_all_mesh_instances(mesh_merge_state.mesh_items, p_root, p_root);

	mesh_merge_state.original_mesh_items.resize(1);
	_find_all_mesh_instances(mesh_merge_state.original_mesh_items, p_original_root, p_original_root);
	if (mesh_merge_state.original_mesh_items.size() != mesh_merge_state.mesh_items.size()) {
		return p_root;
	}

	for (int32_t items_i = 0; items_i < mesh_merge_state.mesh_items.size(); items_i++) {
		p_root = _merge_mesh_instances(mesh_merge_state, items_i);
	}
	return p_root;
}

Node *MeshMergeMeshInstanceWithMaterialAtlas::_merge_mesh_instances(MeshMergeState p_mesh_merge_state, int p_index) {
	Vector<MeshState> mesh_items = p_mesh_merge_state.mesh_items[p_index].meshes;
	Node *p_root = p_mesh_merge_state.root->duplicate();
	const Vector<MeshState> &original_mesh_items = p_mesh_merge_state.original_mesh_items[p_index].meshes;
	Array mesh_to_index_to_material;
	Vector<Ref<Material> > material_cache;

	map_mesh_to_index_to_material(mesh_items, mesh_to_index_to_material, material_cache);

	Vector<Vector<Vector2> > uv_groups;
	Vector<Vector<ModelVertex> > model_vertices;
	scale_uvs_by_texture_dimension_larger(original_mesh_items, mesh_items, uv_groups, mesh_to_index_to_material, model_vertices);
	xatlas::Atlas *atlas = xatlas::Create();

	int32_t num_surfaces = 0;
	for (const MeshState &mesh_item : mesh_items) {
		num_surfaces += mesh_item.mesh->get_surface_count();
	}

	xatlas::PackOptions pack_options;
	Vector<AtlasLookupTexel> atlas_lookup;
	Error err = _generate_atlas(num_surfaces, uv_groups, atlas, mesh_items, material_cache, pack_options);
	ERR_FAIL_COND_V(err != OK, p_root);
	atlas_lookup.resize(atlas->width * atlas->height);
	HashMap<String, Ref<Image> > texture_atlas;
	HashMap<int32_t, MaterialImageCache> material_image_cache;

	MergeState state{
		p_root,
		atlas,
		mesh_items,
		mesh_to_index_to_material,
		uv_groups,
		model_vertices,
		p_root->get_name(),
		p_mesh_merge_state.output_path,
		pack_options,
		atlas_lookup,
		material_cache,
		texture_atlas,
		material_image_cache,
	};

#ifdef TOOLS_ENABLED
	EditorProgress progress_scene_merge("gen_get_source_material", TTR("Get source material"), state.material_cache.size());
	int step = 0;
#endif

	for (const Ref<Material> &abstract_material : state.material_cache) {
#ifdef TOOLS_ENABLED
		step++;
#endif
		Ref<BaseMaterial3D> material = abstract_material;
		MaterialImageCache cache{
			_get_source_texture(state, material),
		};
		int32_t material_i = state.material_cache.find(abstract_material);
		state.material_image_cache[material_i == -1 ? state.material_image_cache.size() : material_i] = cache;

#ifdef TOOLS_ENABLED
		progress_scene_merge.step(TTR("Getting Source Material: ") + material->get_name() + " (" + itos(step) + "/" + itos(state.material_cache.size()) + ")", step);
#endif
	}

	_generate_texture_atlas(state, "albedo");

	if (state.atlas->width <= 0 && state.atlas->height <= 0) {
		xatlas::Destroy(atlas);
		return state.p_root;
	}

	p_root = _output(state, p_index);

	xatlas::Destroy(atlas);
	return p_root;
}

void MeshMergeMeshInstanceWithMaterialAtlas::_generate_texture_atlas(MergeState &state, String texture_type) {
	Ref<Image> atlas_img = Image::create_empty(state.atlas->width, state.atlas->height, false, Image::FORMAT_RGBA8);
	ERR_FAIL_COND_MSG(atlas_img.is_null(), "Failed to create empty atlas image.");

	// Rasterize chart triangles.
#ifdef TOOLS_ENABLED
	EditorProgress progress_texture_atlas("gen_mesh_atlas", TTR("Generate Atlas"), state.atlas->meshCount);
	int step = 0;
#endif
	for (uint32_t mesh_i = 0; mesh_i < state.atlas->meshCount; mesh_i++) {
		const xatlas::Mesh &mesh = state.atlas->meshes[mesh_i];
		for (uint32_t chart_i = 0; chart_i < mesh.chartCount; chart_i++) {
			const xatlas::Chart &chart = mesh.chartArray[chart_i];
			Ref<Image> img;
			if (texture_type == "albedo") {
				img = state.material_image_cache[chart.material].albedo_img;
			} else {
				ERR_PRINT("Unknown texture type: " + texture_type);
				continue;
			}
			ERR_CONTINUE(img.is_null());
			ERR_CONTINUE(img->is_empty());
			ERR_CONTINUE_MSG(Image::get_format_pixel_size(img->get_format()) > 4, "Float textures are not supported yet for texture type: " + texture_type);

			img->convert(Image::FORMAT_RGBA8);
			SetAtlasTexelArgs args;
			args.sourceTexture = img;
			args.atlasData = atlas_img;
			args.atlas_lookup = state.atlas_lookup.ptrw();
			args.atlas_height = state.atlas->height;
			args.atlas_width = state.atlas->width;
			args.material_index = (uint16_t)chart.material;
			for (uint32_t face_i = 0; face_i < chart.faceCount; face_i++) {
				Vector2 v[3];
				for (uint32_t l = 0; l < 3; l++) {
					const uint32_t index = mesh.indexArray[chart.faceArray[face_i] * 3 + l];
					const xatlas::Vertex &vertex = mesh.vertexArray[index];
					v[l] = Vector2(vertex.uv[0], vertex.uv[1]);
					int img_width = img->get_width();
					int img_height = img->get_height();
					ERR_CONTINUE_MSG(img_width == 0 || img_height == 0, "Image width or height is zero for texture type: " + texture_type);

					args.source_uvs[l].x = state.uvs[mesh_i][vertex.xref].x / img_width;
					args.source_uvs[l].y = state.uvs[mesh_i][vertex.xref].y / img_height;
				}
				MeshMergeTriangle tri(v[0], v[1], v[2], Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1));

				tri.drawAA(setAtlasTexel, &args);
			}
		}
#ifdef TOOLS_ENABLED
		progress_texture_atlas.step(TTR("Process Mesh for Atlas: ") + texture_type + " (" + itos(step) + "/" + itos(state.atlas->meshCount) + ")", step);
		step++;
#endif
	}
	if (atlas_img.is_valid()) {
		print_line(vformat("Generated atlas for %s: width=%d, height=%d", texture_type, atlas_img->get_width(), atlas_img->get_height()));
	} else {
		print_line(vformat("Failed to generate atlas for %s", texture_type));
	}
	if (!atlas_img->is_empty()) {
		atlas_img->generate_mipmaps();
	}
	state.texture_atlas.insert(texture_type, atlas_img);
}

Ref<Image> MeshMergeMeshInstanceWithMaterialAtlas::_get_source_texture(MergeState &state, Ref<BaseMaterial3D> material) {
	int32_t width = 0, height = 0;
	Vector<Ref<Texture2D> > textures = { material->get_texture(BaseMaterial3D::TEXTURE_ALBEDO) };
	Vector<Ref<Image> > images;
	images.resize(textures.size());

	for (int i = 0; i < textures.size(); ++i) {
		if (textures[i].is_valid()) {
			images.write[i] = textures[i]->get_image();
			if (images[i].is_valid() && !images[i]->is_empty()) {
				width = MAX(width, images[i]->get_width());
				height = MAX(height, images[i]->get_height());
			}
		}
	}

	for (int i = 0; i < images.size(); ++i) {
		if (images[i].is_valid()) {
			if (!images[i]->is_empty() && images[i]->is_compressed()) {
				images.write[i]->decompress();
			}
			images.write[i]->resize(width, height, Image::INTERPOLATE_LANCZOS);
		}
	}

	Ref<Image> img = Image::create_empty(width, height, false, Image::FORMAT_RGBA8);

	bool has_albedo_texture = images[0].is_valid() && !images[0]->is_empty();
	Color color_mul = has_albedo_texture ? material->get_albedo() : Color(0, 0, 0, 0);
	Color color_add = has_albedo_texture ? Color(0, 0, 0, 0) : material->get_albedo();
	for (int32_t y = 0; y < img->get_height(); y++) {
		for (int32_t x = 0; x < img->get_width(); x++) {
			Color c = has_albedo_texture ? images[0]->get_pixel(x, y) : Color();
			c *= color_mul;
			c += color_add;
			img->set_pixel(x, y, c);
		}
	}

	return img;
}

Error MeshMergeMeshInstanceWithMaterialAtlas::_generate_atlas(const int32_t p_num_meshes, Vector<Vector<Vector2> > &r_uvs, xatlas::Atlas *atlas, const Vector<MeshState> &r_meshes, const Vector<Ref<Material> > material_cache,
		xatlas::PackOptions &pack_options) {
	uint32_t mesh_count = 0;
	for (int32_t mesh_i = 0; mesh_i < r_meshes.size(); mesh_i++) {
		for (int32_t j = 0; j < r_meshes[mesh_i].mesh->get_surface_count(); j++) {
			Array mesh = r_meshes[mesh_i].mesh->surface_get_arrays(j);
			if (mesh.is_empty()) {
				xatlas::UvMeshDecl meshDecl;
				xatlas::AddUvMesh(atlas, meshDecl);
				mesh_count++;
				continue;
			}
			Array indices = mesh[ArrayMesh::ARRAY_INDEX];
			if (!indices.size()) {
				xatlas::UvMeshDecl meshDecl;
				xatlas::AddUvMesh(atlas, meshDecl);
				mesh_count++;
				continue;
			}
			xatlas::UvMeshDecl meshDecl;
			meshDecl.vertexCount = r_uvs[mesh_count].size();
			meshDecl.vertexUvData = r_uvs[mesh_count].ptr();
			meshDecl.vertexStride = sizeof(Vector2);
			Vector<int32_t> mesh_indices = mesh[Mesh::ARRAY_INDEX];
			Vector<uint32_t> indexes;
			indexes.resize(mesh_indices.size());
			Vector<uint32_t> materials;
			materials.resize(mesh_indices.size());
			for (int32_t index_i = 0; index_i < mesh_indices.size(); index_i++) {
				indexes.write[index_i] = mesh_indices[index_i];
			}
			for (int32_t index_i = 0; index_i < mesh_indices.size(); index_i++) {
				Ref<Material> mat = r_meshes[mesh_i].mesh->surface_get_material(j);
				int32_t material_i = material_cache.find(mat);
				if (material_i < 0 || material_i >= material_cache.size()) {
					continue;
				}
				if (material_i != -1) {
					materials.write[index_i] = material_i;
				}
			}
			meshDecl.indexCount = indexes.size();
			meshDecl.indexData = indexes.ptr();
			meshDecl.indexFormat = xatlas::IndexFormat::UInt32;
			meshDecl.faceMaterialData = materials.ptr();
			xatlas::AddMeshError error = xatlas::AddUvMesh(atlas, meshDecl);
			ERR_CONTINUE_MSG(error != xatlas::AddMeshError::Success, String("Error adding mesh ") + itos(mesh_i) + String(": ") + xatlas::StringForEnum(error));
			mesh_count++;
		}
	}
	pack_options.bilinear = true;
	pack_options.padding = 16;
	pack_options.texelsPerUnit = 20.0f;
	pack_options.bruteForce = true;
	pack_options.blockAlign = true;
	pack_options.resolution = 0;
	xatlas::ComputeCharts(atlas);
	xatlas::PackCharts(atlas, pack_options);
}

void MeshMergeMeshInstanceWithMaterialAtlas::scale_uvs_by_texture_dimension_larger(const Vector<MeshState> &original_mesh_items, Vector<MeshState> &mesh_items, Vector<Vector<Vector2> > &uv_groups, Array &r_mesh_to_index_to_material, Vector<Vector<ModelVertex> > &r_model_vertices) {
	int32_t total_surface_count = 0;
	for (int32_t mesh_i = 0; mesh_i < mesh_items.size(); mesh_i++) {
		total_surface_count += mesh_items[mesh_i].mesh->get_surface_count();
	}

	r_model_vertices.resize(total_surface_count);
	uv_groups.resize(total_surface_count);

	int32_t mesh_count = 0;
	for (int32_t mesh_i = 0; mesh_i < mesh_items.size(); mesh_i++) {
		for (int32_t surface_i = 0; surface_i < mesh_items[mesh_i].mesh->get_surface_count(); surface_i++) {
			Ref<ArrayMesh> array_mesh = mesh_items[mesh_i].mesh;
			Array mesh = array_mesh->surface_get_arrays(surface_i);
			Vector<ModelVertex> model_vertices;
			if (mesh.is_empty()) {
				mesh_count++;
				r_model_vertices.write[mesh_count] = model_vertices;
				continue;
			}
			Array vertices = mesh[ArrayMesh::ARRAY_VERTEX];
			if (vertices.size() == 0) {
				mesh_count++;
				r_model_vertices.write[mesh_count] = model_vertices;
				continue;
			}
			Vector<Vector3> vertex_arr = mesh[Mesh::ARRAY_VERTEX];
			Vector<Vector3> normal_arr = mesh[Mesh::ARRAY_NORMAL];
			Vector<Vector2> uv_arr = mesh[Mesh::ARRAY_TEX_UV];
			Vector<int32_t> index_arr = mesh[Mesh::ARRAY_INDEX];
			Vector<Plane> tangent_arr = mesh[Mesh::ARRAY_TANGENT];
			Transform3D xform = original_mesh_items[mesh_i].mesh_instance->get_global_transform();
			model_vertices.resize(vertex_arr.size());
			for (int32_t vertex_i = 0; vertex_i < vertex_arr.size(); vertex_i++) {
				ModelVertex vertex;
				vertex.pos = xform.xform(vertex_arr[vertex_i]);
				if (uv_arr.size()) {
					vertex.uv = uv_arr[vertex_i];
				}
				if (normal_arr.size()) {
					vertex.normal = normal_arr[vertex_i];
				}
				model_vertices.write[vertex_i] = vertex;
			}
			r_model_vertices.write[mesh_count] = model_vertices;
			mesh_count++;
		}
	}

	mesh_count = 0;
	for (int32_t mesh_i = 0; mesh_i < mesh_items.size(); mesh_i++) {
		for (int32_t j = 0; j < mesh_items[mesh_i].mesh->get_surface_count(); j++) {
			Array mesh = mesh_items[mesh_i].mesh->surface_get_arrays(j);
			if (mesh.is_empty()) {
				uv_groups.push_back(Vector<Vector2>());
				mesh_count++;
				continue;
			}
			Vector<Vector3> vertices = mesh[ArrayMesh::ARRAY_VERTEX];
			if (vertices.size() == 0) {
				mesh_count++;
				uv_groups.push_back(Vector<Vector2>());
				continue;
			}
			Vector<Vector2> uvs;
			uvs.resize(vertices.size());
			Vector<int32_t> indices = mesh[ArrayMesh::ARRAY_INDEX];
			for (int32_t vertex_i = 0; vertex_i < vertices.size(); vertex_i++) {
				if (mesh_count >= r_mesh_to_index_to_material.size()) {
					uvs.resize(0);
					break;
				}
				Array index_to_material = r_mesh_to_index_to_material[mesh_count];
				if (!index_to_material.size()) {
					continue;
				}
				int32_t index = indices.find(vertex_i);
				if (index >= index_to_material.size()) {
					continue;
				}
				ERR_CONTINUE(index == -1);
				const Ref<Material> material = index_to_material.get(index);
				Ref<BaseMaterial3D> Node3D_material = material;
				const Ref<Texture2D> tex = Node3D_material->get_texture(BaseMaterial3D::TextureParam::TEXTURE_ALBEDO);
				uvs.write[vertex_i] = r_model_vertices[mesh_count][vertex_i].uv;
				if (tex.is_valid()) {
					uvs.write[vertex_i].x *= tex->get_width();
					uvs.write[vertex_i].y *= tex->get_height();
				}
			}
			uv_groups.write[mesh_count] = uvs;
			mesh_count++;
		}
	}
}

Ref<Image> MeshMergeMeshInstanceWithMaterialAtlas::dilate(Ref<Image> source_image) {
	Ref<Image> target_image = source_image->duplicate();
	target_image->convert(Image::FORMAT_RGBA8);
	Vector<uint8_t> pixels;
	int32_t height = target_image->get_size().y;
	int32_t width = target_image->get_size().x;
	const int32_t bytes_in_pixel = 4;
	pixels.resize(height * width * bytes_in_pixel);
	for (int32_t y = 0; y < height; y++) {
		for (int32_t x = 0; x < width; x++) {
			int32_t pixel_index = x + (width * y);
			int32_t index = pixel_index * bytes_in_pixel;
			Color pixel = target_image->get_pixel(x, y);
			pixels.write[index + 0] = uint8_t(pixel.r * 255.0f);
			pixels.write[index + 1] = uint8_t(pixel.g * 255.0f);
			pixels.write[index + 2] = uint8_t(pixel.b * 255.0f);
			pixels.write[index + 3] = uint8_t(pixel.a * 255.0f);
		}
	}
	rjm_texbleed(pixels.ptrw(), width, height, 3, bytes_in_pixel, bytes_in_pixel * width);
	for (int32_t y = 0; y < height; y++) {
		for (int32_t x = 0; x < width; x++) {
			Color pixel;
			int32_t pixel_index = x + (width * y);
			int32_t index = bytes_in_pixel * pixel_index;
			pixel.r = pixels[index + 0] / 255.0f;
			pixel.g = pixels[index + 1] / 255.0f;
			pixel.b = pixels[index + 2] / 255.0f;
			pixel.a = 1.0f;
			target_image->set_pixel(x, y, pixel);
		}
	}
	target_image->generate_mipmaps();
	return target_image;
}

void MeshMergeMeshInstanceWithMaterialAtlas::map_mesh_to_index_to_material(Vector<MeshState> mesh_items, Array &mesh_to_index_to_material, Vector<Ref<Material> > &material_cache) {
	float largest_dimension = 0;
	for (int32_t mesh_i = 0; mesh_i < mesh_items.size(); mesh_i++) {
		Ref<ArrayMesh> array_mesh = mesh_items[mesh_i].mesh;
		for (int32_t j = 0; j < array_mesh->get_surface_count(); j++) {
			Ref<BaseMaterial3D> mat = array_mesh->surface_get_material(j);
			Ref<Texture2D> texture = mat->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
			if (texture.is_valid()) {
				largest_dimension = MAX(texture->get_size().x, texture->get_size().y);
			}
		}
	}
	largest_dimension = MAX(largest_dimension, default_texture_length);
	for (int32_t mesh_i = 0; mesh_i < mesh_items.size(); mesh_i++) {
		Ref<ArrayMesh> array_mesh = mesh_items[mesh_i].mesh;
		array_mesh->lightmap_unwrap(Transform3D(), 1.0f / largest_dimension, true);
		for (int32_t j = 0; j < array_mesh->get_surface_count(); j++) {
			Array mesh = array_mesh->surface_get_arrays(j);
			Vector<Vector3> indices = mesh[ArrayMesh::ARRAY_INDEX];
			Ref<BaseMaterial3D> material = mesh_items[mesh_i].mesh->surface_get_material(j);
			if (material->get_texture(BaseMaterial3D::TEXTURE_ALBEDO).is_null()) {
				Ref<Image> img = Image::create_empty(default_texture_length, default_texture_length, true, Image::FORMAT_RGBA8);
				img->fill(material->get_albedo());
				material->set_albedo(Color(1.0f, 1.0f, 1.0f));
				Ref<ImageTexture> tex = ImageTexture::create_from_image(img);
				material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);
			}

			if (material_cache.find(material) == -1) {
				material_cache.push_back(material);
			}
			Array materials;
			materials.resize(indices.size());
			for (int32_t index_i = 0; index_i < indices.size(); index_i++) {
				materials[index_i] = material;
			}
			mesh_to_index_to_material.push_back(materials);
		}
	}
}

Node *MeshMergeMeshInstanceWithMaterialAtlas::_output(MergeState &state, int p_count) {
	if (state.atlas->width == 0 || state.atlas->height == 0) {
		return state.p_root;
	}
	print_line(vformat("Atlas size: (%d, %d)", state.atlas->width, state.atlas->height));
	MeshMergeMeshInstanceWithMaterialAtlas::TextureData texture_data;
	for (int32_t mesh_i = 0; mesh_i < state.r_mesh_items.size(); mesh_i++) {
		if (state.r_mesh_items[mesh_i].mesh_instance->get_parent()) {
			Node3D *node_3d = memnew(Node3D);
			Transform3D xform = state.r_mesh_items[mesh_i].mesh_instance->get_transform();
			node_3d->set_transform(xform);
			node_3d->set_name(state.r_mesh_items[mesh_i].mesh_instance->get_name());
			state.r_mesh_items[mesh_i].mesh_instance->replace_by(node_3d);
		}
	}
	Ref<SurfaceTool> st_all;
	st_all.instantiate();
	st_all->begin(Mesh::PRIMITIVE_TRIANGLES);
	for (uint32_t mesh_i = 0; mesh_i < state.atlas->meshCount; mesh_i++) {
		Ref<SurfaceTool> st;
		st.instantiate();
		st->begin(Mesh::PRIMITIVE_TRIANGLES);
		const xatlas::Mesh &mesh = state.atlas->meshes[mesh_i];
		print_line(vformat("Mesh %d: vertexCount=%d, indexCount=%d", mesh_i, mesh.vertexCount, mesh.indexCount));
		for (uint32_t v = 0; v < mesh.vertexCount; v++) {
			const xatlas::Vertex vertex = mesh.vertexArray[v];
			const ModelVertex &sourceVertex = state.model_vertices[mesh_i][vertex.xref];
			Vector2 uv = Vector2(vertex.uv[0] / state.atlas->width, vertex.uv[1] / state.atlas->height);
			st->set_uv(uv);
			st->set_normal(sourceVertex.normal);
			st->set_color(Color(1.0f, 1.0f, 1.0f));
			st->add_vertex(sourceVertex.pos);
		}
		for (uint32_t f = 0; f < mesh.indexCount; f++) {
			const uint32_t index = mesh.indexArray[f];
			st->add_index(index);
		}
		st->generate_tangents();
		Ref<ArrayMesh> array_mesh = st->commit();
		st_all->append_from(array_mesh, 0, Transform3D());
	}
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_name("Atlas");
	HashMap<String, Ref<Image> >::Iterator A = state.texture_atlas.find("albedo");
	Image::CompressMode compress_mode = Image::COMPRESS_ETC;
	if (Image::_image_compress_bc_func) {
		compress_mode = Image::COMPRESS_S3TC;
	}
	if (A && !A->key.is_empty()) {
		Ref<Image> img = dilate(A->value);
		print_line(vformat("Albedo image size: (%d, %d)", img->get_width(), img->get_height()));
		img->compress(compress_mode, Image::COMPRESS_SOURCE_SRGB);
		String path = state.output_path;
		String base_dir = path.get_base_dir();
		path = base_dir.path_to_file(path.get_basename().get_file() + "_albedo");
		Ref<DirAccess> directory = DirAccess::create(DirAccess::AccessType::ACCESS_FILESYSTEM);
		path += "_" + itos(p_count) + ".res";
		Ref<ImageTexture> tex = ImageTexture::create_from_image(img);
		ResourceSaver::save(tex, path);
		Ref<Texture2D> res = ResourceLoader::load(path, "Texture2D");
		mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, res);
	}
	mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	MeshInstance3D *mi = memnew(MeshInstance3D);
	Ref<ArrayMesh> array_mesh = st_all->commit();
	mi->set_mesh(array_mesh);
	mi->set_name(state.p_name);
	Transform3D root_xform;
	Node3D *node_3d = cast_to<Node3D>(state.p_root);
	if (node_3d) {
		root_xform = node_3d->get_transform();
	}
	mi->set_transform(root_xform.affine_inverse());
	array_mesh->surface_set_material(0, mat);
	state.p_root->add_child(mi, true);
	if (mi != state.p_root) {
		mi->set_owner(state.p_root);
	}
	return state.p_root;
}

bool MeshMergeMeshInstanceWithMaterialAtlas::MeshState::operator==(const MeshState &rhs) const {
	if (rhs.mesh == mesh && rhs.path == path && rhs.mesh_instance == mesh_instance) {
		return true;
	}
	return false;
}
