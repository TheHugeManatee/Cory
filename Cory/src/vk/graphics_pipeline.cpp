#include "Cory/vk/graphics_pipeline.h"

#include "Cory/vk/pipeline_utilities.h"

namespace cory::vk {

}

#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>
//
//#include "Cory/vk/test_utils.h"
//
// using namespace cory::vk;
//
// TEST_SUITE_BEGIN("pipeline");
//
// const char *vertex_shader_glsl = R"glsl(
//#version 450
//
// void main() {
//	const vec3 positions[3] = vec3[3](vec3(1.f,1.f, 0.0f),vec3(-1.f,1.f, 0.0f),vec3(0.f,-1.f,
// 0.0f)); 	gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
//})glsl";
//
// const char *fragment_shader_glsl = R"glsl(
//#version 450
//
// layout (location = 0) out vec4 outFragColor;
//
// void main() {
//	outFragColor = vec4(1.f,0.f,0.f,1.0f);
//})glsl";
//
// TEST_CASE("creating a pipeline") {
//	graphics_context ctx = cory::vk::test_context();
//	auto fragment_shader_src =
//      std::make_shared<shader_source>(shader_type::Fragment, fragment_shader_glsl,
//      "fragment.glsl");
//}

TEST_SUITE_END();

#endif