import socket

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

server = '/tmp/FFsockeT'

sock.connect(server)

while True:
    data = sock.recv(80)
    data = [ord(x) for x in data]
    print(data)
