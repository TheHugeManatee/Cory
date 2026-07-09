# ImGui

## Load When
- editing ImGui rendering, widgets, texture registration, or UI-layer glue
- working on the visual reviewer UI or headless ImGui test rendering
- integrating ImGuizmo transforms (see `src/Cory/Systems/ImGuizmoTransformSystem.*`)

## Main Paths
- `src/Cory/ImGui/ImguiRenderer.hpp` — public API: initialize/cleanup, registerTexture/unregisterTexture, updateGeometryBuffers, recordCommands
- `src/Cory/ImGui/ImguiRenderer.cpp` — KDGpu adapter: inline vertex/fragment shaders compiled via `Shader::CompileToSpv`, cmrc-resourced TTF font, mesh buffer management, push-constant scale/translate block
- `src/Cory/ImGui/Inputs.hpp` — generic Slider/Input/ComboBox/CheckBox templates over `MutableValueHolder` / `NamedValueHolder` / `StringOptionsHolder` concepts; float/int/glm vec support; works with `Cory::NumericParameter`
- `src/Cory/ImGui/Widgets.cpp` — profiling records table with min/max/avg + histogram plot
- `src/Cory/Application/ImGuiLayer.hpp` — public API: renderTask, recordFrameCommands, registerTexture/unregisterTexture (delegates to `ImGuiRenderer`)
- `src/Cory/Application/ImGuiLayer.cpp` — Application layer bridge; owns `ImGuiRenderer` and `ImGuiContext`; GLFW init/shutdown, render pass declaration with SkipPipelineBind, event routing for resize/mouse events, ImGuizmo integration via `ImGuizmo::SetImGuiContext`

## Important Concepts
- `ImGuiRenderer` adapts ImGui draw data to KDGpu — manages vertex/index meshes per in-flight frame and user/font textures.
- Texture registration is part of the renderer API; font texture uses cmrc-resourced Roboto-Medium TTF.
- `ImGuiLayer` bridges ImGui into the application layer stack via a `RenderTaskDeclaration<LayerPassOutputs>` with `SkipPipelineBind`.
- Visual tests use a small headless ImGui renderer from `test-support/include/Cory/Testing/VisualTestUtils.hpp`.

## Read Next
- `src/Cory/ImGui/ImguiRenderer.hpp` — renderer API surface
- `src/Cory/Application/ImGuiLayer.hpp` — application-layer integration
- `src/Cory/Renderer/Context.hpp` — device/queue access used by `ImGuiRenderer`
- `tools/visual/src/VisualReviewUi.cpp` — reviewer UI behavior and ImGui usage in tools

## Related Skills
- `renderer` — when working with KDGpu rendering pipeline details
- `testing` — when writing or debugging visual regression tests
- `cbt` — for configure/build/test/run/analyze/fmt of the project

## Gotchas
- Texture bookkeeping is cached; rebuild bind groups (`rebuildRegisteredTextureBindGroups`) when registrations change.
- `ImGuiRenderer::updateGeometryBuffers` assumes per-frame geometry uploads (vertex/index count grow monotonically).
- The visual reviewer UI and test helper share the same review flow via ImGui.
