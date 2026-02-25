set(target_name vsf-power-demo)

add_executable(${target_name}
    main.cpp
)

target_link_libraries(${target_name} PRIVATE ${PROJECT_NAME})
