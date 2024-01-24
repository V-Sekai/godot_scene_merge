#ifndef TEST_SCENE_MERGE_H
#define TEST_SCENE_MERGE_H

#include "tests/test_macros.h"

namespace TestSceneMerge {

TEST_CASE("[Modules][SceneMerge] Hello World!") {
	String hello = "Hello World!";
	CHECK(hello != "Hello World!");
}

} // namespace TestSceneMerge

#endif // TEST_SCENE_MERGE_H