#!/usr/bin/python

import os, re, math
from subprocess import *

# corelist = [1]
corelist = [1, 2, 3, 4, 5, 6, 7, 8]
repeat = 10

log = open('stat.log', 'w')

def stddev(num):
    l = map(float, num)
    a = avg(num)
    s = 0.0
    for x in l:
        s += (a - x) * (a - x)
    s /= len(l) - 1
    return math.sqrt(s)

def avg(num):
    l = map(float, num)
    return sum(l) / len(l)

def warmup():
    print('Warming up...')
    p = Popen('./sockbench 1', shell=True, stdout=PIPE)
    os.waitpid(p.pid, 0)

def test():
    print('Begin testing...')
    pattern = re.compile(r'usec: (\d+)')
    for n in corelist:
        stats = []
        for i in xrange(repeat):
            p = Popen('./sockbench %d' % n, shell=True, stdout=PIPE)
            os.waitpid(p.pid, 0)
            output = p.stdout.read().strip()
            usec = int(pattern.search(output).group(1))
            stats.append(usec)
            log.write('%d: %d\n' % (n, usec))
            print('%d: %d' % (n, usec))
        print('Core #%d: averge %f, std deviation %f' % (n, avg(stats), stddev(stats)))

warmup()
test()

log.close()

