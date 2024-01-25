
#ifndef TEST_SCENE_MERGE_H
#define TEST_SCENE_MERGE_H

#include "tests/test_macros.h"

#include "modules/scene_merge/merge.h"
#include "modules/scene_merge/mesh_merge_triangle.h"
namespace TestSceneMerge {

TEST_CASE("[Modules][SceneMerge] SceneMerge instantiates") {
	String hello = "SceneMerge instantiates.";
	CHECK(hello == "SceneMerge instantiates.");
}

bool mock_callback(void *param, int x, int y, const Vector3 &bar, const Vector3 &dx, const Vector3 &dy, float coverage) {
	CHECK(x == 1);
	CHECK(y == 2);
	CHECK(bar == Vector3(3, 4, 5));
	CHECK(dx == Vector3(6, 7, 8));
	CHECK(dy == Vector3(9, 10, 11));
	CHECK(coverage == 0.5f);

	return true;
}

TEST_CASE("[Modules][SceneMerge] MeshMergeTriangle drawAA") {
	Vector2 v0(1, 2), v1(3, 4), v2(5, 6);
	Vector3 t0(7, 8, 9), t1(10, 11, 12), t2(13, 14, 15);
	MeshMergeTriangle triangle(v0, v1, v2, t0, t1, t2);

	CHECK(triangle.drawAA(mock_callback, nullptr));
}

TEST_CASE("[Modules][SceneMerge] MeshMergeMeshInstanceWithMaterialAtlasTest") {
	MeshMergeMeshInstanceWithMaterialAtlas::SetAtlasTexelArgs args;
	args.atlas_data = Image::create_empty(1024, 1024, false, Image::FORMAT_RGBA8);
	args.atlas_data->fill(Color());
	args.source_texture = Image::create_empty(1024, 1024, false, Image::FORMAT_RGBA8);
	args.source_texture->fill(Color());
	MeshMergeMeshInstanceWithMaterialAtlas::AtlasLookupTexel lookup;
    args.atlas_lookup = &lookup;
    lookup.x = 512;
    lookup.y = 512;
    bool result = MeshMergeMeshInstanceWithMaterialAtlas::set_atlas_texel(&args, 512, 512, Vector3(0.33, 0.33, 0.33), Vector3(), Vector3(), 0.0f);
    CHECK(result);
    lookup.x = 1023;
    lookup.y = 1023;
    result = MeshMergeMeshInstanceWithMaterialAtlas::set_atlas_texel(&args, 1023, 1023, Vector3(0.33, 0.33, 0.33), Vector3(), Vector3(), 0.0f);
    CHECK(result);
}

TEST_CASE("[Modules][SceneMerge] CalculateCoordinates") {
    Vector2 source_uv(0.5, 0.5);
    int width = 100;
    int height = 200;
    Pair<int, int> result = MeshMergeMeshInstanceWithMaterialAtlas::calculate_coordinates(source_uv, width, height);
    CHECK_EQ(result.first, 50);
    CHECK_EQ(result.second, 100);

    source_uv = Vector2(1.0, 1.0);
    result = MeshMergeMeshInstanceWithMaterialAtlas::calculate_coordinates(source_uv, width, height);
    CHECK_EQ(result.first, 0);
    CHECK_EQ(result.second, 0);

    source_uv = Vector2(1.5, 1.5);
    result = MeshMergeMeshInstanceWithMaterialAtlas::calculate_coordinates(source_uv, width, height);
    CHECK_EQ(result.first, 50);
    CHECK_EQ(result.second, 100);
}

} // namespace TestSceneMerge

#endif // TEST_SCENE_MERGE_H