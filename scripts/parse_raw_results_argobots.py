from collections import defaultdict
import os
import re

from statistics import mean

directory_path = './raw_results_argobots'

xstreams_pattern = re.compile(r'Number of available xstreams:\s+(\d+)')
threads_pattern = re.compile(r'Number of available threads:\s+(\d+)')
initialization_time_pattern = re.compile(r'Initialization time =\s+([\d.]+) seconds')
verification_pattern = re.compile(r'VERIFICATION SUCCESSFUL')
class_pattern = re.compile(r'Class\s+=\s+(\w+)')
time_pattern = re.compile(r'Time in seconds =\s+([\d.]+)')

time_dict = defaultdict(list)

for filename in os.listdir(directory_path):
    if not filename.endswith('.txt'):
        continue

    with open(os.path.join(directory_path, filename), 'r') as file:
        content = file.read()

        xstreams = xstreams_pattern.search(content)
        threads = threads_pattern.search(content)
        init_time = initialization_time_pattern.search(content)
        verification = verification_pattern.search(content)
        class_p = class_pattern.search(content)
        time_seconds = time_pattern.search(content)

        xstreams_value = int(xstreams.group(1)) if xstreams else None
        threads_value = int(threads.group(1)) if threads else None
        init_time_value = init_time.group(1) if init_time else None
        verification_status = True if verification else False
        class_value = class_p.group(1) if class_p else None
        time_seconds_value = time_seconds.group(1) if time_seconds else None

        if not verification_status:
            print(f'Verification failed on {filename}')
            continue

        key = (class_value,xstreams_value, threads_value)
        time_dict[key].append(float(time_seconds_value))

for key in sorted(time_dict):
    value = time_dict[key]
    mean_value = mean(value)
    rounded_mean = round(mean_value, 2)
    print(f'{key}: {rounded_mean}')