#!/usr/bin/env python
import socket
import sys
import os
import re
import filetype
import datetime

today = datetime.datetime.now()

def get_content(msg):
    lines = re.split("\r\n\r\n", msg)

    body = lines[1]
    headers = re.split("\r\n", lines[0])

    headers.append("body: " + lines[1])

    content_dict = {re.split(":", l)[0].strip().lower():re.split(":", l)[1].strip() for l in headers[1:] if l != ""}

    return headers[0], content_dict

def get_filename(request):
    m = re.match("GET /(.*) ", request)

    if len(m.groups()) < 1:
        print "Get request ill formed"
        return None

    filename = m.group(1)

    if filename == "":
        return "index.html"

    return filename

def get_filetype(filename):
    kind = filetype.guess(filename)
    if kind is None:
        return "text"
    return kind.mime

forbiddenfiles = ['webserver.py', 'Download/webclient.py', 'Upload', 'Download']

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

        print "\nGot Request:\n", msg
        request, headers_dict = get_content(msg)

        print "debug: ", request, headers_dict
        filename = get_filename(request)


        date = "Date: " + today.ctime() + "\r\n"
        server = "Server: pedroserver35.2 (Python 2.7)\r\n"
        common_headers = date + server

        print "debug: requested file: ", get_filename(request)
        if(filename == None):
            print("Bad Request")
            cont = "Bad Request"
            client.sendall("HTTP/1.1 400 Bad Request\r\n" + common_headers + "content-type: html/text\r\ncontent-length: " + str(len(cont)) + "\r\n\r\n" + cont)
            # client.close()
            break;

        if filename in forbiddenfiles:
            print("Forbidden File")
            cont = "<header>Forbidden!!<\header>"
            client.sendall("HTTP/1.1 403 Forbidden\r\n" + common_headers + "content-type: html/text\r\ncontent-length: " + str(len(cont)) + "\r\n\r\n" + cont)
            # client.close()
            break;

        try:
            fname = "Upload/" + filename
            print "Fname: ", fname
            f = open(fname, "r")
            fcontent = f.read()
            body = "HTTP/1.1 200 OK\r\n"
            clen = "Content-Length: " + str(len(fcontent)) + "\r\n"
            cty = "Content-Type: " + get_filetype(fname) + "\r\n"

            if headers_dict["connection"] == None:
                connection = ""
            else:
                connection = "Connection: " + headers_dict["connection"] + "\r\n"

            ans = body + clen + cty + connection + common_headers + "\r\n" + fcontent
            print "Sending answer: \n", ans
            client.sendall(ans)
            break;

        except IOError:
            print("Could not open file")
            cont = "<header>File not found<\header>"
            client.sendall("HTTP/1.1 404 Not Found\r\n" + "content-type: html/text\r\ncontent-length: " + str(len(cont)) + "\r\n\r\n" + cont)
            # client.close()
            break;


    else:
        continue;
