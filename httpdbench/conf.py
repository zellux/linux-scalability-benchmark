#!/usr/bin/python

conf = r'''
server.document-root = "%(webdir)s" 

server.port = %(port)s
server.bind = "%(ip)s"

mimetype.assign = (
  ".html" => "text/html", 
  ".txt" => "text/plain",
  ".jpg" => "image/jpeg",
  ".png" => "image/png" 
)

index-file.names = ("index.html")
'''

iplist = ['10.131.1.133']
port = 8111

if __name__ == '__main__':
	for i, ip in enumerate(iplist):
		cfile = open('node%d.conf' % i, 'w')
		cfile.write(conf % locals())
	
