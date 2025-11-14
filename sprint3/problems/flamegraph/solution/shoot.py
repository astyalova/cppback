import argparse
import subprocess
import time
import random
import shlex
import os
import signal

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1

PERF_DATA_FILE = 'perf.data'
GRAPH_FILE = 'graph.svg'


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    else:
        process.terminate()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def run_perf_record(pid):
    return subprocess.Popen(
        ['perf', 'record', '-F', '99', '-g', '-o', PERF_DATA_FILE, '-p', str(pid)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )


def build_flamegraph():
    perf_script = subprocess.Popen(['perf', 'script', '-i', PERF_DATA_FILE], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    stackcollapse = subprocess.Popen(['./FlameGraph/stackcollapse-perf.pl'], stdin=perf_script.stdout, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    with open(GRAPH_FILE, 'w') as svg: subprocess.run(['./FlameGraph/flamegraph.pl'], stdin=stackcollapse.stdout, stdout=svg, check=True)


def main():
    server_cmd = start_server()
    server_proc = run(server_cmd)

    time.sleep(1) 

    server_pid = server_proc.pid

    perf_proc = run_perf_record(server_pid)

    make_shots()

    perf_proc.send_signal(signal.SIGINT)
    perf_proc.wait()

    stop(server_proc)
    time.sleep(1)

    build_flamegraph()
    print('Job done')

if __name__ == '__main__':
    main()