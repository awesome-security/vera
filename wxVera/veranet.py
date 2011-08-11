# TCP server for VERA

import socket

port = 2005

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(("", port))
s.listen(5)

print "Server listening on port %d" % port

while 1:
    client, addr = s.accept()
    print "Connection from ", addr[0]
    client.send("VERAnet_0.0001\n")

    # Read the data

    while 1:
        data = ""
        try:
            data = client.recv(512).strip()
        except socket.error:
            client.close()
            print "Client closed connection"
            break

        if len(data) == 0:
            continue

        if ( data == 'BYE' ):
            print "Disconnecting"
            client.send("Bye!\n")
            client.close()
            break
        elif ( "NAV" in data ):
            print "Got navigate command"
            parms = data.split(" ")

            if len(parms) != 2:
                client.send("Invalid format (only 1 parameter expected)\n")
                continue

            try:
                addr = int(parms[1])
            except ValueError:
                client.send("Invalid address format\n")
                continue

            print "Got navigate command to 0x%8.8x" % addr
        else:
            client.send("Unsupported\n");
