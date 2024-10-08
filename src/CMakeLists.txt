configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/builtins/tzdata.h.in" "${CMAKE_CURRENT_BINARY_DIR}/tzdata.h" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/windows_zones.h.in" "${CMAKE_CURRENT_BINARY_DIR}/windows_zones.h" @ONLY)

set( SOURCES
bigint.cc
interpreter.cc
rego.cc
parse.cc
args.cc
unifier.cc
value.cc
value_map.cc
resolver.cc
variable.cc
builtins.cc
internal.cc
rego_c.cc
encoding.cc
json.cc
yaml.cc
reader.cc
unify.cc
unify/strings.cc
unify/merge_data.cc
unify/varrefheads.cc
unify/lift_refheads.cc
unify/symbols.cc
unify/replace_argvals.cc
unify/lift_query.cc
unify/expand_imports.cc
unify/constants.cc
unify/enumerate.cc
unify/locals.cc
unify/compr.cc
unify/absolute_refs.cc
unify/merge_modules.cc
unify/skips.cc
unify/infix.cc
unify/assign.cc
unify/skip_refs.cc
unify/simple_refs.cc
unify/init.cc
unify/rulebody.cc
unify/lift_to_rule.cc
unify/functions.cc
unify/unify.cc
unify/result.cc
builtins/aggregates.cc
builtins/arrays.cc
builtins/bits.cc
builtins/casts.cc
builtins/encoding.cc
builtins/graph.cc
builtins/numbers.cc
builtins/objects.cc
builtins/regex.cc
builtins/semver.cc
builtins/sets.cc
builtins/strings.cc
builtins/time.cc
builtins/types.cc
builtins/units.cc
builtins/uuid.cc
# third-party
builtins/base64/base64.cpp
)

add_library(rego STATIC ${SOURCES})

add_library(regocpp::rego ALIAS rego)

if (REGOCPP_SPECULATIVE_LOAD_HARDENING)
  target_compile_options(rego PRIVATE -mllvm -x86-speculative-load-hardening)
endif()

if (MANUAL_TZ_DB)
  target_compile_definitions(rego PUBLIC "MANUAL_TZ_DB")
endif()

target_link_libraries(rego
  PUBLIC
    trieste::trieste
    trieste::json
    trieste::yaml
    date-tz
)

if(MSVC)
  target_compile_options(rego PUBLIC "/Zc:__cplusplus")
  target_compile_definitions(rego PUBLIC "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")
endif()

if (REGOCPP_SANITIZE)
  target_compile_options(rego PUBLIC -g -fsanitize=${REGOCPP_SANITIZE} -fno-omit-frame-pointer)
  target_link_libraries(rego PUBLIC -fsanitize=${REGOCPP_SANITIZE})
endif()

if(REGOCPP_USE_CXX17)
  target_compile_features(rego PUBLIC cxx_std_17)
  target_compile_definitions(rego PUBLIC REGOCPP_USE_CXX17)
else()
  target_compile_features(rego PUBLIC cxx_std_20)
endif()

if(REGOCPP_ACTION_METRICS)
  target_compile_definitions(rego PUBLIC REGOCPP_ACTION_METRICS)
endif()

target_include_directories( rego
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/rego>
)
