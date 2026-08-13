# LumaEngine.exe - the runtime host that opens the window and runs the loop.
# This is the Milestone 1 deliverable.

add_executable(LumaEngine
    main.cpp)

target_link_libraries(LumaEngine
    PRIVATE Luma::Core Luma::Platform Luma::RenderVulkan Luma::VFS)

set_target_properties(LumaEngine PROPERTIES
    OUTPUT_NAME "LumaEngine"
    FOLDER "Runtime")
