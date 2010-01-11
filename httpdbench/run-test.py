#!/usr/bin/python

import os, time
from conf import *
from subprocess import *

startcmd = 'lighttpd -f node%(id)s.conf -D &'
affinitycmd = 'taskset -pc %(id)s %(pid)s'
abcmd = 'ab -n 10000 http://%(ip)s:%(port)s/index.html 2>&1 | grep Requests'

servers = []
clients = []

def start():
	print('Starting servers...')
	for id, ip in enumerate(iplist):
		print startcmd % locals()
		p = Popen(startcmd % locals(), shell=True)
		pid = p.pid
		Popen(affinitycmd % locals(), shell=True)
		servers.append(p)

def test():
	print('Waiting 3 seconds for server starting...')
	time.sleep(3)
	print('Testing...')
	port = 8111
	for id, ip in enumerate(iplist):
		p = Popen(abcmd % locals(), shell=True)
		clients.append(p)
	for c in clients:
		os.waitpid(p.pid, 0)

def cleanup():
	print('Cleaning up...')
	for s in servers:
		os.system('killall lighttpd')

start()
test()
cleanup()
