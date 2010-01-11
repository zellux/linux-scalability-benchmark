#!/usr/bin/python

from conf import *
from subprocess import *

startcmd = 'lighttpd -f node%(id)s.conf -D &'

servers = []
def start():
	for id, ip in enumerate(iplist):
		print startcmd % locals()
		p = Popen(startcmd % locals(), shell=True)
		servers.append(p)

def test():
	pass
	
start()
