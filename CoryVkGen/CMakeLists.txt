find_package(Python REQUIRED)

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


set(PYTHON_SOURCES
        __main__.py
        cvkg/__init__.py
        cvkg/flextglutils.py
        cvkg/SpecParser.py
        cvkg/Types.py
        )
set(TEMPLATE_SOURCES
        cvkg/templates/CreateInfo.tpl.h
        )

set(CVK_DIR ${CMAKE_CURRENT_LIST_DIR}/cvk)


add_custom_target(cvk
        COMMAND python ${CMAKE_CURRENT_LIST_DIR}/__main__.py --output_folder ${CVK_DIR} --registry ${VULKAN_REGISTRY_XML}
        SOURCES ${PYTHON_SOURCES} ${TEMPLATE_SOURCES}
        )