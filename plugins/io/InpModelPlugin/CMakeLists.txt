precess_add_io_plugin(InpModelPlugin 
    SOURCES "InpModelHandler.cpp" 
    SOURCES "abaqus_io.cpp" 
    SOURCES "AbaqusPrecessConverter.cpp" 
    PLUGIN_H "InpModelPlugin.h"
)

add_subdirectory(Inptest)
