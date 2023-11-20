#!/usr/bin/env python3
import argparse
from pathlib import Path


def bidir_conflictless(path, number_of_files, payload_size, verbose=True):
    """
    Generate number_of_files divided over both hosts with no conflicts and no subdirectories.
    This tests large, asynchronous, bidirectional file-transfers.
    """

    peer_a = path / 'peer_a'
    peer_b = path / 'peer_b'

    try:
        peer_a.mkdir()
        peer_b.mkdir()
    except FileExistsError:
        if verbose:
            print('Please clean the working directory before generating the dataset.')
        return
    
    if verbose:
        print(f'Generating {number_of_files} files @ {payload_size} bytes...')
    peer_a_count = int(number_of_files / 2)
    peer_b_count = number_of_files - peer_a_count

    payload = b'\xFF' * payload_size 

    # Generate the files
    for i in range(peer_a_count):
        with (peer_a / f'file_{i:04}').open('wb') as f:
            f.write(payload)
    for i in range(peer_b_count):
        with (peer_b / f'file_{peer_a_count + i:04}').open('wb') as f:
            f.write(payload)

    if verbose:
        print('Done!')


scenarios = {
    'bidir_conflictless': bidir_conflictless
}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog='file_gen.py',
        description='This program generates test-datasets to test file-sync applications. (Written for fmerge_cpp)',
        epilog='Written by Leon Teichroeb'
    )
    parser.add_argument('path', help='Path to generate the test dataset in')
    parser.add_argument('scenario', choices=scenarios.keys(), help='The scenario to generate.')
    parser.add_argument('-n', '--count', type=int, default=500, help='Number of files to generate')
    parser.add_argument('--payload-size', type=int, default=32, help='Number of bytes per file to fill')
    args = parser.parse_args()

    scenarios[args.scenario](Path(args.path), args.count, args.payload_size)