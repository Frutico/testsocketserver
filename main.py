import struct
from socket import *
import sys
import random

CTRLPORT = 8000
DATAPORT = 8001

random.seed()

# create set
try:
    s1 = socket(AF_INET, SOCK_STREAM)
    s2 = socket(AF_INET, SOCK_STREAM)
except error as err:
    print("set creation failed: %s" %err)
    sys.exit()

    # connect to the host
try:
    s1.connect(("localhost", CTRLPORT))
    s2.connect(("localhost", DATAPORT))
except error as err:
    print("set connection failed: %s" %err)
    sys.exit()

id = random.randint(1, 1000000)
s1.send(str(id).encode())
key = int(s1.recv(128).decode())
s2.send((str(id) + " " + str(key) + " " + 'Message to write').encode())
s2.send((str(id) + " " + str(key+1) + " " + 'Erroneous key').encode())
message = s2.recv(128).decode()
print(message)
s1.close()
s2.close()
