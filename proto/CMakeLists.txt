include("${BuildboxCommon_DIR}/BuildboxCommonProtoc.cmake") # protoc_compile()

protoc_compile(PROTOS build/buildbox/local_execution)

add_custom_target(generate_protobufs DEPENDS ${PROTO_GENERATED_SRCS})
set(PROTO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
set(PROTO_GENERATED_SRCS ${PROTO_GENERATED_SRCS} PARENT_SCOPE)
