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

SHOOT_COUNT = 1000
COOLDOWN = 0.01


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
    process.terminate()


def shoot(ammo):
    result = subprocess.run(f'curl -s -o /dev/null -w "%{{http_code}}" {ammo}', 
                          shell=True, capture_output=True, text=True)
    time.sleep(COOLDOWN)


def make_shots():
    for i in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')

print("Starting server...")
server = run(start_server())
time.sleep(2)

perf_cmd = run(f'timeout 30s sudo perf record -g -p {server.pid} -o perf.data') 
time.sleep(1)

make_shots()

perf_cmd.wait()

print("Perf recording finished")

if os.path.exists('perf.data'):
    subprocess.run('perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > graph.svg', shell=True)
    
    if os.path.exists('graph.svg'):
        svg_size = os.path.getsize('graph.svg')
        print(f"graph.svg size: {svg_size} bytes")
        print("Flamegraph created successfully!")
    else:
        print("Failed to create flamegraph")
        
else:
    print("perf.data file not found!")

stop(server)
print('Job done')
