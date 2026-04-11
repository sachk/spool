add_library(QCoroCore INTERFACE)
add_library(QCoroNetwork INTERFACE)

target_include_directories(QCoroCore INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/../include
)
target_include_directories(QCoroNetwork INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries(QCoroNetwork INTERFACE QCoroCore Qt6::Network)

add_library(QCoro::Core ALIAS QCoroCore)
add_library(QCoro::Network ALIAS QCoroNetwork)
