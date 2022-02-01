find_package(Python REQUIRED COMPONENTS Interpreter)

find_package(Vulkan REQUIRED)

# the vk.xml discovery is pretty primitive and assumes the folder structure
# I'm seeing for the Windows SDK - wouldn't be surprised if this fails..
set(VULKAN_REGISTRY_XML ${Vulkan_INCLUDE_DIR}/../share/vulkan/registry/vk.xml)
if (EXISTS ${VULKAN_REGISTRY_XML})
    message(STATUS "Using Vulkan registry at ${VULKAN_REGISTRY_XML}")
else ()
    message(NOTICE "Unsuccessfully tried to find vk.xml at '${VULKAN_REGISTRY_XML}'.")
    message(FATAL_ERROR "Could not discover vk.xml. Please fix me.")
endif ()

set(CVK_DIR ${CMAKE_CURRENT_LIST_DIR}/cvk/include/cvk)

set(PYTHON_SOURCES
        __main__.py
        cvkg/__init__.py
        cvkg/flextglutils.py
        cvkg/BuilderGenerator.py
        cvkg/EnumFmtGenerator.py
        cvkg/Generator.py
        cvkg/GeneratorBase.py
        cvkg/GeneratorUtils.py
        cvkg/SpecParser.py
        cvkg/StructFmtGenerator.py
        cvkg/Types.py
        )
set(TEMPLATE_SOURCES
        templates/Builder.tpl.h
        templates/FmtEnum.tpl.h
        templates/FmtStruct.tpl.h
        )
set(GENERATED_FILES
        ${CVK_DIR}/Builder.h
        ${CVK_DIR}/FmtEnum.h
        ${CVK_DIR}/FmtStruct.h
        )

add_custom_command(
        OUTPUT ${GENERATED_FILES}
        COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/__main__.py --output_folder ${CVK_DIR} --registry ${VULKAN_REGISTRY_XML}
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        DEPENDS ${PYTHON_SOURCES} ${TEMPLATE_SOURCES}
        )

add_library(cvk INTERFACE ${GENERATED_FILES})
target_link_libraries(cvk INTERFACE Vulkan::Vulkan)
target_include_directories(cvk INTERFACE ${CMAKE_CURRENT_LIST_DIR}/cvk/include)

add_library(cvk::cvk ALIAS cvk)