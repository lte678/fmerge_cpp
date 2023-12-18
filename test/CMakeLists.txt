add_test(
    NAME check_version
    COMMAND python ${TEST_DIR}/run_tests.py --test-check-version
)
add_test(
    NAME bidir_small_files
    COMMAND python ${TEST_DIR}/run_tests.py --test-bidir-small-files
)
add_test(
    NAME bidir_medium_files
    COMMAND python ${TEST_DIR}/run_tests.py --test-bidir-medium-files
)
