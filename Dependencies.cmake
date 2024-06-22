function(TARGET_DEPENDENCIES PROJ)
    target_include_directories(${PROJ} PRIVATE ${INCLUDE_PATH})
    target_include_directories(${PROJ} PRIVATE ${refl}/include)
    target_include_directories(${PROJ} PRIVATE ${SDK_PATH}/include)
    target_include_directories(${PROJ} PRIVATE ${SDK_PATH}/vendor)

    # Add Wildcards submodule
    target_include_directories(${PROJ} PRIVATE ${VENDOR}/wildcards/include)

    # Add ReflectCPP
    target_include_directories(${PROJ} PRIVATE ${VENDOR}/reflect-cpp/src)
    target_include_directories(${PROJ} PRIVATE ${VENDOR}/reflect-cpp/include)

    target_precompile_headers(${PROJ} PRIVATE ${INCLUDE_PATH}/PCH.hpp)

    ## vcpkg dependencies

    find_package(re2 CONFIG REQUIRED)
    target_link_libraries("${PROJ}" PUBLIC re2::re2)

    find_package(magic_enum CONFIG REQUIRED)
    target_link_libraries("${PROJ}" PUBLIC magic_enum::magic_enum)

    find_package(spdlog CONFIG REQUIRED)
    target_link_libraries("${PROJ}" PUBLIC spdlog::spdlog)

    find_package(glm CONFIG REQUIRED)
    target_link_libraries("${PROJ}" PUBLIC glm::glm)

    find_package(amqpcpp CONFIG REQUIRED)
    target_link_libraries("${PROJ}" PUBLIC amqpcpp)

    find_package(uvw CONFIG REQUIRED)
    target_link_libraries("${PROJ}" PUBLIC uvw::uvw)

    find_package(stduuid CONFIG REQUIRED)
    target_link_libraries("${PROJ}" PUBLIC stduuid)

    find_package(xbyak CONFIG REQUIRED)
    target_link_libraries("${PROJ}" PUBLIC xbyak::xbyak)

    # MongoCXX
    find_package(mongocxx REQUIRED)
    find_package(bsoncxx REQUIRED)
    find_package(mongoc-1.0 CONFIG REQUIRED)
    include_directories(${LIBMONGOCXX_INCLUDE_DIR})
    include_directories(${LIBBSONCXX_INCLUDE_DIR})

    target_link_libraries(${PROJ} PUBLIC mongo::mongoc_shared)
    target_link_libraries(${PROJ} PUBLIC mongo::bsoncxx_shared)
    target_link_libraries(${PROJ} PUBLIC mongo::mongocxx_shared)

    # FLCore

    target_link_libraries(${PROJ} PUBLIC "${SDK_PATH}/lib/FLCoreCommon.lib")
    target_link_libraries(${PROJ} PUBLIC "${SDK_PATH}/lib/FLCoreDACom.lib")
    target_link_libraries(${PROJ} PUBLIC "${SDK_PATH}/lib/FLCoreDALib.lib")
    target_link_libraries(${PROJ} PUBLIC "${SDK_PATH}/lib/FLCoreFLServerEXE.lib")
    target_link_libraries(${PROJ} PUBLIC "${SDK_PATH}/lib/FLCoreRemoteClient.lib")
    target_link_libraries(${PROJ} PUBLIC "${SDK_PATH}/lib/FLCoreServer.lib")
endfunction()