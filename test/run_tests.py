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
import time
from pathlib import Path
import shutil
from helpers.file_gen import bidir_conflictless

SUPRESS_STDOUT = False

TEST_NG = False
TEST_OK = True


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FMERGE_BINARY = os.path.normpath(os.path.join(SCRIPT_DIR, "../build/bin/fmerge"))
TEST_PATH = Path('/tmp/fmerge_tests')

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


def test_bidir_medium_files():
    # Transfer a moderately large number of medium sized files (reasonable good realistic workload)
    # Do not use conflicts. Do not include subfolders

    # Should execute in 60s
    TEST_TIMEOUT = 5

    # Step 1: Create dataset
    bidir_conflictless(TEST_PATH, 200, 1024*1024, verbose=False)

    # Step 2: Run two fmerge instances
    p1 = subprocess.Popen([FMERGE_BINARY, '-s', (TEST_PATH / 'peer_a').as_posix()], stdout=subprocess.PIPE)
    # Allow p1 to start listening for clients
    time.sleep(5)
    p2 = subprocess.Popen([FMERGE_BINARY, '-c', 'localhost', (TEST_PATH / 'peer_b').as_posix()], stdout=subprocess.PIPE)
    time.sleep(1)
    # Allow both processes to fail now before we check for a timeout

    try:
        res1 = p1.wait(timeout=TEST_TIMEOUT)
    except subprocess.TimeoutExpired:
        p1.kill()
        out, _ = p1.communicate()
        print(out.decode('ASCII'), end='')
        return (TEST_NG, 'Fmerge timed out.')
    res2 = p2.poll()
    if res2 is None:
        p2.kill()
        out, _ = p2.communicate()
        print(out.decode('ASCII'), end='')
        return (TEST_NG, 'Fmerge timed out.')
    if res1 != 0 or res2 != 0:
        return (TEST_NG, f'Fmerge failed with error codes {res1} and {res2}.')

    return (TEST_OK, '')


system_tests = [
    test_check_version,
    test_bidir_medium_files
]


if __name__ == '__main__':
    try:
        shutil.rmtree('/tmp/fmerge_tests')
    except FileNotFoundError:
        pass

    all_passed = True
    for test in system_tests:
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
        
        shutil.rmtree('/tmp/fmerge_tests')

    if not all_passed:
        print("")
        print("Tests failed 🤕")
        sys.exit(1)
    else:
        print("")
        print("All tests passed 😎")
        sys.exit(0)
