#!/usr/bin/python3

'''
    Script for updating the bootloader or the application of our Mbed OS based
    firmwares. The target application has to have a TCPFwUpdateModule object
    instantiated.

    Usage:
        ./update_fw.py IP PORT FW_TYPE BIN_PATH

   Author: Guilherme Ricioli <guilherme.ricioli@lnls.br>
'''

import os
import socket
import sys

# Defined by the firmware
max_cmd_len = 32
terminator = '\n'
chunk_max_size = 1024   # LPC1768's flash page size

def recv_cmd_res(sock):
    cmd_res_recvd = False
    while not cmd_res_recvd:
        cmd_res = sock.recv(max_cmd_len).decode('UTF-8').replace(terminator, '')
        if cmd_res in ["ACK", "NACK", "NALL"]:
            cmd_res_recvd = True
    return cmd_res

if len(sys.argv) != 8:
    print(f'Wrong usage! Usage: {sys.argv[0]} IP PORT FW_TYPE BIN_PATH MAJOR MINOR PATCH')
    sys.exit(-1)

ip = sys.argv[1]
port = int(sys.argv[2])
fw_type = sys.argv[3]
bin_path = sys.argv[4]
major, minor, patch = sys.argv[5], sys.argv[6], sys.argv[7]

print('# Opening socket... ', end='')
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(5.0)
sock.connect((ip, port))
print('done')

size = os.path.getsize(bin_path)
f = open(bin_path, 'rb')
byte = f.read(1)
checksum = 0
while byte:
    checksum = checksum + int.from_bytes(byte, 'big')

    byte = f.read(1)
print(f'size, checksum: {size}, {checksum}')
cmd_req = ''.join([f'@UPD {fw_type} {size} {checksum} {major} {minor} {patch}', terminator])
cmd_req = cmd_req.encode('UTF-8')
sock.sendall(cmd_req)
cmd_res = recv_cmd_res(sock)
assert cmd_res == "ACK"


f.seek(0)
chunk = f.read(chunk_max_size)
sent_count = 0
while chunk:
    chunk_size = len(chunk)
    print(f'# Sending {chunk_size} bytes... ', end='')
    cmd_req = ''.join([f'@BIN {chunk_size}',terminator]).encode('UTF-8')
    cmd_req = bytearray(cmd_req)
    cmd_req.extend(chunk)
    sock.sendall(cmd_req)
    cmd_res = recv_cmd_res(sock)
    assert cmd_res == "ACK"
    sent_count = sent_count + chunk_size
    print(f'done ({sent_count} of {size} bytes sent)')

    chunk = f.read(chunk_max_size)
f.close()

print('# Validating image... ', end='')
cmd_req = ''.join([f'@VAL', terminator]).encode('UTF-8')
sock.sendall(cmd_req)
cmd_res = ''
cmd_res = recv_cmd_res(sock)
assert cmd_res == "ACK"
print('done')

print('# Closing socket... ', end='')
sock.close()
print('done')

print('# Target successfully updated!')
