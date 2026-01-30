# Build and Run on Ubuntu 24.04 (Clang)

This guide assumes a **fresh Ubuntu 24.04** install, **Clang**, and a local build directory under
`/home/<user>/cory-work` while the repo lives at `/mnt/c/dev/Cory`.

## 1) Build setup (dependencies, SDKs, Conan, presets)

### System packages

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  clang \
  lld \
  cmake \
  ninja-build \
  python3 \
  python3-venv \
  python3-pip \
  pkg-config \
  git \
  xorg-dev \
  libx11-xcb-dev \
  libxcb1-dev \
  libxcb-keysyms1-dev \
  libxcb-icccm4-dev \
  libxcb-image0-dev \
  libxcb-randr0-dev \
  libxcb-render0-dev \
  libxcb-shape0-dev \
  libxcb-sync-dev \
  libxcb-xfixes0-dev \
  libxcb-xinerama0-dev \
  libxcb-util-dev \
  libxcb-util0-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libvulkan-dev \
  glslang-tools \
  glslc \
  mesa-vulkan-drivers \
  vulkan-tools \
  vulkan-utility-libraries-dev
```

Notes:
- The X11/XCB packages are required by GLFW.
- `vulkan-tools` provides `vulkaninfo` for a quick driver sanity check.
- On Ubuntu 24.04, `vulkan-validationlayers-dev` is replaced by `vulkan-utility-libraries-dev`.
- `glslangValidator` (from `glslang-tools`) is required by KDGpu during configure.

### Vulkan SDK (for Slang)

Slang is expected to come from the Vulkan SDK. Install it under your home directory:

```bash
mkdir -p /home/$USER/VulkanSDK
cd /home/$USER/VulkanSDK
curl -L -o vulkan-sdk.tar.xz https://sdk.lunarg.com/sdk/download/latest/linux/vulkan-sdk.tar.xz
tar -xf vulkan-sdk.tar.xz
```

After extraction, your SDK path looks like:

```
/home/$USER/VulkanSDK/<version>/x86_64
```

### Conan (in a dedicated work dir)

Create a local work directory and venv:

```bash
mkdir -p /home/$USER/cory-work
python3 -m venv /home/$USER/cory-work/.venv
source /home/$USER/cory-work/.venv/bin/activate
python -m pip install --upgrade pip
python -m pip install "conan>=2.0"
```

Create/update a Conan profile for clang (example uses clang 22):

```bash
conan profile detect --force
conan profile path default
```

Copy the default profile to a custom one and edit it:

```
cp /home/$USER/.conan2/profiles/default /home/$USER/.conan2/profiles/codex-clang
```

Set these values in `/home/$USER/.conan2/profiles/codex-clang`:

```
[settings]
compiler=clang
compiler.version=22
compiler.libcxx=libstdc++11
compiler.cppstd=20

[conf]
tools.build:compiler_executables={"c":"clang","cpp":"clang++"}
```

If your installed clang version isn’t listed in Conan defaults, add it to:

```
/home/$USER/.conan2/settings.yml
```

### Conan install (dependencies)

```bash
source /home/$USER/cory-work/.venv/bin/activate
conan install /mnt/c/dev/Cory \
  --output-folder=/home/$USER/cory-work/build/codex \
  --build=missing \
  -s build_type=Debug \
  -pr:h codex-clang \
  -pr:b default
```

### CMake preset (codex)

Set the `codex` preset to use the local build directory and Vulkan SDK:

```
binaryDir: /home/$USER/cory-work/build/codex
toolchainFile: /home/$USER/cory-work/build/codex/conan_toolchain.cmake
VULKAN_SDK: /home/$USER/VulkanSDK/<version>/x86_64
Vulkan_INCLUDE_DIR: /home/$USER/VulkanSDK/<version>/x86_64/include
Vulkan_LIBRARY: /home/$USER/VulkanSDK/<version>/x86_64/lib/libvulkan.so
```

(In this repo, these are set in `CMakeUserPresets.json`.)

## 2) Build

```bash
cmake --preset codex
cmake --build --preset codex
```

## 3) Run

List targets:

```bash
cmake --build --preset codex --target help | rg -i 'cory|demo|example'
```

Run an executable (example):

```bash
/home/$USER/cory-work/build/codex/bin/SceneGraphDemo
```

## Troubleshooting

- **Vulkan not working**: run `vulkaninfo` to verify the loader and driver are visible.
- **Linker errors with Clang**: ensure `lld` is installed and consider adding
  `-DCMAKE_LINKER=lld` to the CMake configure step.
- **Wayland-only environment**: GLFW will still use X11 by default; keep the X11/XCB packages installed.
