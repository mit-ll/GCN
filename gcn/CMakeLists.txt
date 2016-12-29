# The name of our project is "GCN". CMakeLists files in this project can
# refer to the root source directory of the project as ${GCN_Sample_SOURCE_DIR} and
# to the root binary directory of the project as ${GCN_Sample_BINARY_DIR}. 

#Specify the version being used
cmake_minimum_required(VERSION 2.8)

set (PROJECT_NAME "GCN") 


# Recurse into the "src" subdirectory.
add_subdirectory(src)
