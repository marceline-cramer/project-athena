set(TARGET_NAME model-networking)
setup_hifi_library()
link_hifi_libraries(shared shaders networking graphics fbx procedural model-baker)
include_hifi_library_headers(hfm)
include_hifi_library_headers(task)
include_hifi_library_headers(gpu)
include_hifi_library_headers(image)
include_hifi_library_headers(ktx)
include_hifi_library_headers(material-networking)