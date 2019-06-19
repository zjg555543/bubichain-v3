#bubi notary module CmakeLists.txt -- bubi

set(APP_BUBI notary)

set(APP_BUBI_SRC
    main.cpp
    configure.cpp
    notary_mgr.cpp
)

set(INNER_LIBS bubi_cross bubi_ledger bubi_consensus bubi_overlay bubi_common bubi_utils bubi_proto bubi_http bubi_ed25519 bubi_monitor)

#generate executable file
add_executable(${APP_BUBI} ${APP_BUBI_SRC})

#specify dependent libraries for target obj
target_link_libraries(${APP_BUBI}
    -Wl,-dn ${INNER_LIBS} -Wl,--start-group -Wl,--end-group ${BUBI_DEPENDS_LIBS} ${BUBI_LINKER_FLAGS})

#specify compile options for target obj
target_compile_options(${APP_BUBI}
    PUBLIC -std=c++11 
    PUBLIC -DASIO_STANDALONE
    PUBLIC -D_WEBSOCKETPP_CPP11_STL_
)

install(TARGETS ${APP_BUBI}
    RUNTIME DESTINATION ${BUBICHAIN}/bin
)
