import subprocess


def get_process_threads(pid):
    res = subprocess.run(['ps', '-T', '-p', str(pid)], capture_output=True)
    return res.stdout.decode('utf-8')