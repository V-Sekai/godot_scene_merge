
#ifndef TEST_SCENE_MERGE_H
#define TEST_SCENE_MERGE_H

#include "tests/test_macros.h"

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

bool mock_callback_negative(void *param, int x, int y, const Vector3 &bar, const Vector3 &dx, const Vector3 &dy, float coverage) {
	CHECK(x != 1);
	CHECK(y != 2);
	CHECK(bar != Vector3(3, 4, 5));
	CHECK(dx != Vector3(6, 7, 8));
	CHECK(dy != Vector3(9, 10, 11));
	CHECK(coverage != 0.5f);

	return false;
}

TEST_CASE("[Modules][SceneMerge] MeshMergeTriangle drawAA - Negative Test") {
	Vector2 v0(16, 2), v1(3, 4), v2(5, 6);
	Vector3 t0(7, 8, 9), t1(10, 11, 12), t2(13, 14, 15);
	MeshMergeTriangle triangle(v0, v1, v2, t0, t1, t2);

	CHECK_FALSE(triangle.drawAA(mock_callback_negative, nullptr));
}

} // namespace TestSceneMerge

#endif // TEST_SCENE_MERGE_H