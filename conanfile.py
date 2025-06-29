from conan import ConanFile
from conan.tools.cmake import CMakeToolchain
from conan.tools.cmake import CMakeDeps
from conan.tools.cmake import cmake_layout


class CoryProjectConan(ConanFile):
    name = "Cory"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("andreasbuhr-cppcoro/cci.20230629")
        self.requires("catch2/3.2.0")
        self.requires("cli11/2.2.0")
        self.requires("entt/3.11.1")
        self.requires("fmt/10.2.1")
        self.requires("glew/2.2.0")
        self.requires("kdbindings/1.1.0")
        self.requires("glm/1.0.1")
        self.requires("imgui/1.91.8-docking")
        # self.requires("libunifex/0.4.0")
        self.requires("magic_enum/0.8.1")
        self.requires("ms-gsl/4.0.0")
        self.requires("range-v3/0.12.0")
        self.requires("shaderc/2021.1")
        self.requires("spdlog/1.14.1")
        self.requires("vulkan-headers/1.3.290.0")

    def generate(self):
        tc = CMakeToolchain(self, generator="Ninja")
        tc.variables["BUILD_SHARED_LIBS"] = False
        tc.variables["CMAKE_CXX_STANDARD"] = 20
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()
