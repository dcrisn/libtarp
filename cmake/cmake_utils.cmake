# strip all the symbols from the binary associated
# with the TARGET
function(strip_symbols TARGET)
    add_custom_command(
        TARGET "${TARGET}" POST_BUILD
        DEPENDS "${TARGET}"
        COMMAND $<$<CONFIG:release>:${CMAKE_STRIP}>
        ARGS --strip-all $<TARGET_FILE:${TARGET}>
    )
endfunction()
