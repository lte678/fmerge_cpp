#!/usr/bin/env python3
import argparse
from pathlib import Path


def _create_recursive_dirs(path, subdir_levels, subir_base_count, subdir_file_count, payload, suffix, only_last_leaf):
        if not only_last_leaf or subdir_levels == 0:
            for i in range(subdir_file_count):
                # Create files
                with (path / f'file_{i:04}{suffix}').open('wb') as f:
                    f.write(payload)
        if subdir_levels != 0:
            for i in range(subir_base_count):
                # Create subdirs
                child_path = path / f'child_dir_{i:04}'
                child_path.mkdir()
                _create_recursive_dirs(child_path, subdir_levels-1, subir_base_count, subdir_file_count, payload, suffix, only_last_leaf)


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


def bidir_conflictless_subdirs(path, subdir_levels, subir_base_count, subdir_file_count, payload_size, verbose=True, only_last_leaf=False):
    """
    Generate the following file tree:
    [base]
      [child1]
        [child1  ]
          file1 
          file...
          filek
        [child...]
          ...
        [childn  ]
      [child...]
        [child1  ]
        [child...]
        [childn  ]
      [childn  ]
        ...
      file1 
      file...
      filek 
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
        print(f'Generating files @ {payload_size} bytes...')

    payload = b'\xFF' * payload_size 

    # Generate the files
    _create_recursive_dirs(peer_a, subdir_levels, subir_base_count, subdir_file_count, payload, 'a', only_last_leaf)
    _create_recursive_dirs(peer_b, subdir_levels, subir_base_count, subdir_file_count, payload, 'b', only_last_leaf)

    if verbose:
        print('Done!')


def simplex_conflictless_subdirs(path, subdir_levels, subir_base_count, subdir_file_count, payload_size, verbose=True, only_last_leaf=False):
    """
    Generate the same file tree as the duplex_conflictless_subdirs case, but only for one peer
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
        print(f'Generating files @ {payload_size} bytes...')

    payload = b'\xFF' * payload_size 

    # Generate the files
    _create_recursive_dirs(peer_a, subdir_levels, subir_base_count, subdir_file_count, payload, 'a', only_last_leaf)

    if verbose:
        print('Done!')


scenarios = {
    'bidir_conflictless': bidir_conflictless,
    'bidir_conflictless_subdirs': bidir_conflictless_subdirs,
    'simplex_conflictless_subdirs': simplex_conflictless_subdirs,
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