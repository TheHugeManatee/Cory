from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeToolchain, CMakeDeps
from conan.tools.files import get
import os


class KDGpuConan(ConanFile):
    name = "kdgpu"
    license = "GPL-3.0-or-later"
    url = "https://github.com/KDAB/KDGpu"
    description = "A modern C++ Vulkan abstraction library by KDAB."
    topics = ("vulkan", "graphics", "gpu", "rendering", "abstraction")

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_examples": [True, False],
        "with_tests": [True, False],
        "with_kdxr": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_examples": False,
        "with_tests": False,
        "with_kdxr": True
    }

    exports_sources = "CMakeLists.txt", "cmake/*", "src/*"

    # generators = "CMakeDeps"

    def requirements(self):
        self.requires("vulkan-loader/1.3.290.0")
        # self.requires("kdutils/0.2.0")
        self.requires("kdbindings/1.1.0")
        self.requires("spdlog/1.14.1")
        self.requires("glm/[>=1.0.0 <2.0.0]")
        if self.options.with_tests:
            self.requires("doctest/2.4.11")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["BUILD_EXAMPLES"] = self.options.with_examples
        tc.variables["BUILD_TESTING"] = self.options.with_tests
        tc.variables["KDGPU_BUILD_KDXR"] = self.options.with_kdxr
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def source(self):
        get(self, **self.conan_data["sources"][self.version])

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["KDGpu"]
        if self.options.with_kdxr:
            self.cpp_info.libs.append("KDXr")
