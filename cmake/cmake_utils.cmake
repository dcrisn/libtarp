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

function( extract_string INPUT_STRING LENGTH EXTRA_LENGTH OUTPUT_STRING )
	string( LENGTH ${INPUT_STRING} LENGTH_MATCH_ID )
	math( EXPR LENGTH_MATCH_ID "${LENGTH_MATCH_ID}-${LENGTH}-${EXTRA_LENGTH}" )
	string( SUBSTRING ${INPUT_STRING} ${LENGTH} ${LENGTH_MATCH_ID} LOCAL_OUTPUT_STRING )
	set( ${OUTPUT_STRING} ${LOCAL_OUTPUT_STRING} PARENT_SCOPE )
endfunction()

function(get_os_info PARSED_OS_ID PARSED_OS_VERSION_ID PARSED_OS_NAME_ID )
	file( READ "/etc/os-release" OS_RELEASE_FILE )
	string(REGEX MATCH "ID=[A-Z|a-z|0-9\.]+" MATCH_ID ${OS_RELEASE_FILE} )
	extract_string( ${MATCH_ID} 3 0 OS_ID )
	set( ${PARSED_OS_ID} ${OS_ID} PARENT_SCOPE )

	string(REGEX MATCH "VERSION_ID=\"[A-Z|a-z|0-9\.]+\"" MATCH_VERSION_ID ${OS_RELEASE_FILE} )
	extract_string( ${MATCH_VERSION_ID} 12 1 OS_VERSION_ID )
	set( ${PARSED_OS_VERSION_ID} ${OS_VERSION_ID} PARENT_SCOPE )

	set( ${PARSED_OS_NAME_ID} ${OS_ID}-${OS_VERSION_ID} PARENT_SCOPE )
endfunction( get_os_info )
