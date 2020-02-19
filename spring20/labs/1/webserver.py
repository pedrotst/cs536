#!/usr/bin/env python
import socket
import sys
import os
import re

def get_content(msg):
    lines = re.split("\r\n\r\n", msg)

    body = lines[1]
    headers = re.split("\r\n", lines[0])

    headers.append("body: " + lines[1])

    content_dict = {re.split(":", l)[0].strip():re.split(":", l)[1].strip() for l in headers[1:] if l != ""}

    return headers[0], content_dict

def get_filename(request):
    filename = re.match("/(.*)", request)

    return filename

serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

hostname = socket.gethostname()
serversocket.bind((hostname, int(sys.argv[1])))
serversocket.listen(5)

while(1):
    print "listening at", hostname, "..."
    (client, address) = serversocket.accept()
    child_pid = os.fork()

    if child_pid == 0:
        print "got a connection from", address
        msg = client.recv(4096)
        request, headers_dict = get_content(msg)

        print "Received ", request, headers_dict
        break;

    else:
        continue;
