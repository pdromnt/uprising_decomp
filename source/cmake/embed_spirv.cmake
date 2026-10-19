# Build-time only: turn the compiled SPIR-V into an aligned C uint32_t array.
file(READ "${INPUT}" bytes HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])" "0x\\4\\3\\2\\1u,\n" words "${bytes}")
if(NOT DEFINED SYMBOL)
    set(SYMBOL fsr3_reactive_spv)
endif()
file(WRITE "${OUTPUT}" "#include <stdint.h>\nstatic const uint32_t ${SYMBOL}[]={\n${words}};\n")
