#!/usr/bin/env python
import socket
import sys
import os
import re

def get_content(msg):
    lines = re.split("\r\n\r\n", msg)

    return lines[1]

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

serveraddr = sys.argv[1]
serverport = sys.argv[2]
print "connecting to", serveraddr, serverport
sock.connect((serveraddr, int(serverport)))
print "connection successful!"

req_file = "index.html" if len(sys.argv) < 4 else sys.argv[3]

sep = "\r\n"
header = "GET /" + req_file + " HTTP/1.1"
host = "Host: " + serveraddr
conn = "Connection: close"
accpt = "Accept: text/html"
body = ""
msg = header + sep + host + sep + conn + sep + accpt + sep + sep + body

sock.sendall(msg)
ans = sock.recv(2147483647)
content = get_content(ans)

print "Answer:\n", content
# Save file at the Downloads Folder

filehandler = open(req_file, "w")
filehandler.write(content)

