#bubi cross module CmakeLists.txt -- libbubi_cross.a

set(LIB_BUBI_CROSS bubi_cross)

#automaticall get src file in specifical dir
aux_source_directory(${BUBI_SRC_DIR}/cross/ CROSS_SRC)

#generate static library file
add_library(${LIB_BUBI_CROSS} ${CROSS_SRC})

#specify compile options for target obj
target_compile_options(${LIB_BUBI_CROSS}
    PUBLIC -std=c++11 
    PUBLIC -DASIO_STANDALONE
    PUBLIC -D_WEBSOCKETPP_CPP11_STL_
)
