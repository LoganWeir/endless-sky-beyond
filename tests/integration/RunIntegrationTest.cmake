set(TEST_CONFIG "${TEST_CONFIGS}/${test}")

# Clean the config folder of the integration test.
file(REMOVE_RECURSE "${TEST_CONFIG}")
file(COPY "${ES_CONFIG}" DESTINATION "${TEST_CONFIGS}")
file(RENAME "${TEST_CONFIGS}/config" "${TEST_CONFIG}")

# Tests named "eesky ..." also load this fork's own plugins from eesky/plugins.
# Other tests don't, so that its random encounters can't interfere with them.
if(test MATCHES "^eesky " AND EXISTS "${RESOURCE_PATH}/eesky/plugins")
	file(COPY "${RESOURCE_PATH}/eesky/plugins/" DESTINATION "${TEST_CONFIG}/plugins")
endif()

# Run the integration test
execute_process(COMMAND $ENV{ES_INTEGRATION_PREFIX} "${ES}" --config "${TEST_CONFIG}" --resources "${RESOURCE_PATH}" --test "${test}" ${DEBUG}
    OUTPUT_VARIABLE TEST_OUTPUT
    ERROR_VARIABLE TEST_OUTPUT
    RESULT_VARIABLE TEST_RESULT)

if(TEST_RESULT)
    string(STRIP "${TEST_OUTPUT}" TEST_OUTPUT_STRIPPED)
    message(FATAL_ERROR "Integration test failed with '${TEST_RESULT}':\n${TEST_OUTPUT_STRIPPED}")
endif()
