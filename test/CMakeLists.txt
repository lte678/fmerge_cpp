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
add_test(
    NAME bidir_simple_subdirs
    COMMAND python ${TEST_DIR}/run_tests.py --test-bidir-simple-subdirs
)
add_test(
    NAME simplex_medium_file
    COMMAND python ${TEST_DIR}/run_tests.py --test-simplex-medium-file
)
add_test(
    NAME simplex_simple_subdirs
    COMMAND python ${TEST_DIR}/run_tests.py --test-simplex-simple-subdirs
)