
add_executable(rego_interpreter main.cc)
target_link_libraries(rego_interpreter
  PRIVATE 
  regocpp::rego)

set_target_properties(rego_interpreter PROPERTIES OUTPUT_NAME "rego")

add_executable(rego_fuzzer fuzzer.cc)

target_link_libraries(rego_fuzzer
  PRIVATE
  regocpp::rego)

add_custom_command(TARGET rego_interpreter POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/examples $<TARGET_FILE_DIR:rego_interpreter>/examples)

add_executable(rego_compiler compile.cc)
target_link_libraries(rego_compiler
  PRIVATE
  regocpp::rego
)

set_target_properties(rego_compiler PROPERTIES OUTPUT_NAME "regoc")

add_custom_command(TARGET rego_compiler POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ir $<TARGET_FILE_DIR:rego_compiler>/ir)
