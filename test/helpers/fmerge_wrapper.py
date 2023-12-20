import subprocess
import time
from helpers import TestException
from helpers.util import get_process_threads


def save_thread_log(log_prefix, pid_a, pid_b):
    with open(f'{log_prefix}_a_zombies.log', 'w') as log1, open(f'{log_prefix}_b_zombies.log', 'w') as log2:
        log1.write(get_process_threads(pid_a))
        log2.write(get_process_threads(pid_b))


def fmerge(fmerge_path, test_path, log_prefix, server_readiness_wait=5, timeout=60, probe_interval=0.1):
    """
    Return without any exceptions if execution was successfull (according to the fmerge exit code and timeout limits).
    Writes logs to file.
    """

    def check_exit_code(r1, r2):
        if r1 != 0 and r2 != 0:
            raise TestException(f'Fmerge client and server failed with error codes {r1} and {r2}.')
        if r1 != 0:
            raise TestException(f'Fmerge server failed with error codes {r1}.')
        if r2 != 0:
            raise TestException(f'Fmerge client failed with error codes {r2}.')

    with open(f'{log_prefix}_a.log', 'w') as log1, open(f'{log_prefix}_b.log', 'w') as log2:
        # Start the processes
        p1 = subprocess.Popen(
            [fmerge_path, '-y', '-d', '-s', (test_path / 'peer_a').as_posix()],
            stdout=log1,
            stderr=log1
        )
        # Wait for p1 to start listening for clients
        time.sleep(server_readiness_wait)
        p2 = subprocess.Popen(
            [fmerge_path, '-y', '-d', '-c', 'localhost', (test_path / 'peer_b').as_posix()],
            stdout=log2,
            stderr=log2
        )
        # Allow both processes to fail now before we check for a timeout
        time.sleep(1)

        # Wait for timeout
        start_time = time.time()
        while time.time() < start_time + timeout:
            res1 = p1.poll()
            res2 = p2.poll()
            if res1 is not None and res2 is not None:
                # Both processes terminated.
                check_exit_code(res1, res2)
                return
            time.sleep(probe_interval)
        
        # At least one process timed out. Kill it.
        save_thread_log(log_prefix, p1.pid, p2.pid)
        if res1 is None and res2 is None:
            p1.terminate()
            p2.terminate()
            raise TestException('Fmerge client and server timed out.')
        elif res1 is None:
            p1.terminate()
            if res2 == 0:
                raise  TestException('Fmerge server timed out.')
            else:
                raise TestException(f'Fmerge server timed out and client failed with code {res1}')
        elif res2 is None:
            p2.terminate()
            if res1 == 0:
                raise TestException('Fmerge client timed out.')
            else:
                raise TestException(f'Fmerge client timed out and server failed with code {res1}')

        # This should never be reached :)
        raise Exception("Contact Leon Teichroeb and tell him he f***ed up.")