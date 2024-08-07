#!/usr/bin/env python


from subprocess import call
from concurrent.futures import ThreadPoolExecutor as Pool
import os
import sys
from random import *

def count_files(directory):
    # List all files in the directory
    files = [f for f in os.listdir(directory) if os.path.isfile(os.path.join(directory, f))]
    return len(files)

# Example usage
# directory_path = 'dataset/training'
# directory_path = 'dataset/evaluation'
directory_path = 'dataset/test'
file_count = count_files(directory_path)
print(f"There are {file_count} files in the directory '{directory_path}'")

run_list = range(0,file_count)
run_depth = 2


version = str(randint(0,10**9))
if len(sys.argv) == 2: version = sys.argv[1]
print("Updating to version", version)

parallel = 6

os.system('mkdir -p store/version/')
os.system('mkdir -p store/tmp/')

def outdated(i):
    fn = 'store/version/%d.txt'%i
    os.system('touch '+fn)
    f = open(fn, 'r')
    t = f.read()
    f.close()
    return t != version

def update(i):
    fn = 'store/version/%d.txt'%i
    os.system('touch '+fn)
    f = open(fn, 'w')
    f.write(version)
    f.close()

run_list = [i for i in run_list if outdated(i)]

global done
done = 0

n = len(run_list)
def run(i):
    if call(['/usr/bin/time', './run', str(i), str(run_depth)], stdout=open('store/tmp/%d_out.txt'%i,'w'), stderr=open('store/tmp/%d_err.txt'%i,'w')):
        print(i, "Crashed")
        return i
    os.system('cp store/tmp/%d_out.txt store/%d_out.txt'%(i,i))
    update(i)
    global done
    done += 1
    print("%d / %d     \r"%(done, n), end = "")
    sys.stdout.flush()
    return i

call(['make','-j'])
scores = []
with Pool(max_workers=parallel) as executor:
    for i in executor.map(run, run_list):
        pass
print("Done!       ")
