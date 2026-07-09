# Application

## Load When
- editing app shell, windowing, layers, input, camera control, or demo wiring
- working on the `ImGui` layer integration used by examples and visual tools

## Main Paths
- `src/Cory/Application/Application.hpp` — owns `Context`, drives main loop (`runMainLoop`) via `FrameSource`.
- `src/Cory/Application/Window.hpp` — a `FrameSource` with swapchain-resize, mouse, keyboard signals.
- `src/Cory/Application/LayerStack.hpp` — event delivery (top-down) and render/update order (bottom-up).
- `src/Cory/Application/Common.hpp` — shared types: `LayerAttachInfo`, `LayerPassOutputs`.
- `src/Cory/Application/Event.hpp` — input events (`KeyEvent`, `MouseMovedEvent`, etc.) as `std::variant`.

## Important Concepts
- `Application` owns the `Context` and orchestrates the main loop, exposing `run()` for subclasses.
- `LayerStack` defines event order (top-down) and render/update order (bottom-up).
- `Window` is a `FrameSource` emitting resize and input signals; it drives resource recreation on resize.
- `ApplicationLayer` is the extension seam: layers attach to the stack, react to events, optionally enqueue render tasks.
- `ImGuiLayer` is attached via the priority slot (`emplacePriorityLayer`) — must be called once per app lifetime.

## Read Next
- `src/Cory/Application/ImGuiLayer.hpp` — layer bridge and ImGui entry point
- `src/Cory/Application/CameraLayer.hpp` — camera-focused example layer
- `src/Cory/Renderer/FrameSource.hpp` — base of `Window` and frame scheduling
- `examples/01-HelloTriangle/src/HelloTriangleApplication.cpp` — minimal runnable example wiring the app shell

## Related Skills
- `cbt` — configure, build, run, test, lint/format Cory code.

## Gotchas
- Layer order matters for both event delivery and rendering — earlier layers get events *last* but render first.
- `Window::onSwapchainResized` fires inside `nextSwapchainImage()` / `frames()`, not from user code.
- `ImGuiLayer` must be attached through the priority slot; it is not a regular layer in `addLayer`.
