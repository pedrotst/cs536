#!/usr/bin/env python
import socket
import sys

serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# serversocket.bind((socket.gethostname(), 4445))
serversocket.bind((socket.gethostname(), int(sys.argv[1])))
serversocket.listen(5)

while(1):
    print "listening..."
    (client, address) = serversocket.accept()
    print "got a connection from", address
    msg = client.recv(4096)
    print "Received ", msg

    # ct = client_thread(clientsocket)
    # ct.run()
