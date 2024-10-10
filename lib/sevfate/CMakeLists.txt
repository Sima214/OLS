add_executable(lemon lemon.c)
add_custom_command(
    OUTPUT tcode/Parser.hpp tcode/Parser.cpp
    COMMAND lemon -s -T${CMAKE_CURRENT_SOURCE_DIR}/tcode/LemonTemplate.cpp -H${CMAKE_CURRENT_SOURCE_DIR}/tcode/LemonTemplate.hpp -d${CMAKE_CURRENT_BINARY_DIR}/tcode/ ${CMAKE_CURRENT_SOURCE_DIR}/tcode/Parser.yy
    DEPENDS lemon ${CMAKE_CURRENT_SOURCE_DIR}/tcode/LemonTemplate.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tcode/LemonTemplate.hpp ${CMAKE_CURRENT_SOURCE_DIR}/tcode/Parser.yy
    COMMENT "Generating etcode parser file(lemon)"
)
add_custom_command(
    OUTPUT tcode/Tokenizer.cpp
    COMMAND re2c -W -T --case-ranges --depfile ${CMAKE_CURRENT_BINARY_DIR}/tcode/Tokenizer.l.d -o ${CMAKE_CURRENT_BINARY_DIR}/tcode/Tokenizer.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tcode/Tokenizer.l
    DEPFILE ${CMAKE_CURRENT_BINARY_DIR}/tcode/Tokenizer.l.d
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tcode/Tokenizer.l
    COMMENT "Generating etcode parser file(re2c)"
)
add_custom_target(etcode_parser ALL
    DEPENDS tcode/Tokenizer.cpp tcode/Parser.hpp tcode/Parser.cpp
)

# set_source_files_properties(
#     utils/Filenames.cpp tcode/ParserDispatcherPendSync.cpp
#     PROPERTIES CXX_STANDARD 20
# )
add_library(
    etcode_nxt STATIC
    utils/Filenames.cpp tcode/ParserDispatcherPendSync.cpp
)
target_include_directories(etcode_nxt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(etcode_nxt PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET etcode_nxt PROPERTY CXX_STANDARD 20)
target_link_libraries(etcode_nxt PRIVATE nlohmann_json asio)
add_dependencies(etcode_nxt etcode_parser)

add_library(
    etcode STATIC
    utils/Z85.cpp tcode/Tokenizer.cpp tcode/Parser.hpp tcode/Parser.cpp
    tcode/ParserDispatcher.cpp tcode/ParserDispatcherConn.cpp tcode/ParserDispatcherRegistry.cpp
    tcode/ParserDispatcherRequest.cpp tcode/ParserDispatcherResponse.cpp
)
target_include_directories(etcode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(etcode PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(etcode PRIVATE etcode_nxt nlohmann_json asio)
add_dependencies(etcode etcode_parser)
