#!/usr/bin/env python3
# If you want to run tests for this project, this is the place.
# We are using python for our text fixtures, since it is a nicer scripting
# language to work in than C++. Also, we are mostly doing system tests and
# orchestrating multiple versions of the binary at once.
#
# Author: Leon Teichroeb
# Date:   20.11.2023

import sys
import os
import subprocess
from pathlib import Path
import shutil
import argparse
from helpers import TEST_NG, TEST_OK, TestException
from helpers.file_gen import bidir_conflictless
import helpers.fmerge_wrapper as fmerge_wrapper

SUPRESS_STDOUT = False


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FMERGE_BINARY = os.path.normpath(os.path.join(SCRIPT_DIR, "../build/bin/fmerge"))
TEST_PATH = Path('/tmp/fmerge_tests')
LOG_DIR = Path('/tmp/fmerge_logs')

###############################################################################
###########################   Test Definitions   ##############################
###############################################################################

def test_check_version():
    res = subprocess.run([FMERGE_BINARY, '-v'], capture_output=True)
    if res.returncode != 0:
        if not SUPRESS_STDOUT:
            print(res.stdout.decode('ASCII'))
        return (TEST_NG, f'Fmerge returned error code {res.returncode}')
    
    if b'version' in res.stdout.lower():
        return (TEST_OK, '')
    else:
        if not SUPRESS_STDOUT:
            print(res.stdout.decode('ASCII'))
        return (TEST_NG, 'Fmerge did not print version')


def test_bidir_small_files():
    # Transfer a small number of small files as a simple test.
    # Do not use conflicts. Do not include subfolders

    # Create dataset
    bidir_conflictless(TEST_PATH, 10, 1024, verbose=False)
    # Run client-server pair
    try:
        fmerge_wrapper.fmerge(FMERGE_BINARY, TEST_PATH, LOG_DIR / 'bidir_small_files', server_readiness_wait=1, timeout=3)
    except TestException as e:
        return (TEST_NG, str(e))

    return (TEST_OK, '')


def test_bidir_medium_files():
    # Transfer a moderately large number of medium sized files (reasonable good realistic workload)
    # Do not use conflicts. Do not include subfolders

    # Create dataset
    bidir_conflictless(TEST_PATH, 200, 1024*1024, verbose=False)
    # Run client-server pair
    try:
        fmerge_wrapper.fmerge(FMERGE_BINARY, TEST_PATH, LOG_DIR / 'bidir_medium_files', timeout=15)
    except TestException as e:
        return (TEST_NG, str(e))

    return (TEST_OK, '')



###############################################################################
########################   Start of Test Harness   ############################
###############################################################################

system_tests = [
    test_check_version,
    test_bidir_small_files,
    test_bidir_medium_files
]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog='run_tests.py',
        description='Run system tests for Fmerge',
        epilog='Multiple tests can be specified at once')
    parser.add_argument(f'--keep', action='store_true', help='Keep the testing files generated during the test. Only works for individual tests.')    

    for test in system_tests:
        option_string = test.__name__.replace('_', '-')
        parser.add_argument(f'--{option_string}', dest='targets', action='append_const', const=test)
    parser.add_argument('--all', action='store_true')

    args = parser.parse_args()

    if args.all and args.targets is not None:
        print('Specify EITHER --all or any number of specific tests')
        sys.exit(1)
    
    if args.all:
        targets = system_tests
    elif args.targets is not None:
        targets = args.targets
    else:
        print('No test specified')
        sys.exit(1)

    if len(targets) > 1 and args.keep:
        print("Only one test may be specified with the keep option")
        sys.exit(1)
    
    # Start of tests
    try:
        shutil.rmtree('/tmp/fmerge_tests')
    except FileNotFoundError:
        pass

    # Create one log folder for all tests. It is persistent
    Path('/tmp/fmerge_logs').mkdir(exist_ok=True)
    all_passed = True
    for test in targets:
        Path('/tmp/fmerge_tests').mkdir()

        print(f'Running {test.__name__}... ', end='')
        try:
            res, msg = test()
        except Exception as e:
            res = TEST_NG
            msg = str(e)
        
        if res == TEST_OK:
            print('\033[1;32mPASS\033[0m')
        else:
            print('\033[1;31mFAIL\033[0m')
            print(f'  Failure Message: {msg}')
            all_passed = False
        
        if not args.keep:
            shutil.rmtree('/tmp/fmerge_tests')

    if not all_passed:
        print("")
        print("Tests failed 🤕")
        sys.exit(1)
    else:
        print("")
        print("All tests passed 😎")
        sys.exit(0)
