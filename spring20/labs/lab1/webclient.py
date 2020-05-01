#!/usr/bin/env python
import socket
import sys

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# clientsocket.connect(("128.10.3.52", 4445))
serveraddr = sys.argv[1]
serverport = sys.argv[2]
print "connecting to", serveraddr, serverport
sock.connect((serveraddr, int(serverport)))
print "connection successful!"

msg = b"GET / HTTP/1.1\r\nHost: webcode.me\r\nAccept: text/html\r\nConnection: close\r\n\r\n"
sock.sendall(msg)


print "program is over :("
