
#ifndef TEST_SCENE_MERGE_H
#define TEST_SCENE_MERGE_H

#include "tests/test_macros.h"

namespace TestSceneMerge {

TEST_CASE("[Modules][SceneMerge] SceneMerge instantiates") {
	String hello = "SceneMerge instantiates.";
	CHECK(hello == "SceneMerge instantiates.");
}

} // namespace TestSceneMerge

#endif // TEST_SCENE_MERGE_H