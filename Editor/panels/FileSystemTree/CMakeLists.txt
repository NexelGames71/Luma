add_library(LumaPanelFileSystemTree STATIC
    src/FileSystemTree.cpp)

add_library(Luma::PanelFileSystemTree ALIAS LumaPanelFileSystemTree)

target_include_directories(LumaPanelFileSystemTree
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(LumaPanelFileSystemTree
    PUBLIC Luma::Core Luma::Asset Luma::Slate)

set_target_properties(LumaPanelFileSystemTree PROPERTIES FOLDER "Editor/Panels")
