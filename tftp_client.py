import socket
import struct
import os
import sys

BUF_SIZE = 516
DATA_SIZE = 512

OP_RRQ = 1
OP_WRQ = 2
OP_DATA = 3
OP_ACK = 4
OP_ERROR = 5


def send_rrq(sockfd, server_addr, filename, mode):
    buffer = struct.pack(
        f"!H{len(filename)}sx{len(mode)}sx", OP_RRQ, filename.encode(), mode.encode()
    )
    sockfd.sendto(buffer, server_addr)


def send_wrq(sockfd, server_addr, filename, mode):
    buffer = struct.pack(
        f"!H{len(filename)}sx{len(mode)}sx", OP_WRQ, filename.encode(), mode.encode()
    )
    sockfd.sendto(buffer, server_addr)


def send_ack(sockfd, server_addr, block_num):
    ack_packet = struct.pack("!HH", OP_ACK, block_num)
    sockfd.sendto(ack_packet, server_addr)


def receive_file(sockfd, server_addr, filename):
    fd = os.open(filename, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o666)
    if fd < 0:
        print("Failed to open file:", filename)
        sys.exit(1)

    block_num = 0

    while True:
        buffer, _ = sockfd.recvfrom(BUF_SIZE)
        opcode, recv_block_num = struct.unpack("!HH", buffer[:4])

        if opcode == OP_DATA and recv_block_num == block_num + 1:
            os.write(fd, buffer[4:])
            send_ack(sockfd, server_addr, recv_block_num)
            block_num += 1

            if len(buffer) < BUF_SIZE:
                # Last packet received
                break
        elif opcode == OP_ERROR:
            print("Error from server:", buffer[4:].decode())
            os.close(fd)
            sys.exit(1)
        else:
            print("Unexpected packet received")
            os.close(fd)
            sys.exit(1)

    os.close(fd)


def send_file(sockfd, server_addr, filename):
    fd = os.open(filename, os.O_RDONLY)
    if fd < 0:
        print("Failed to open file:", filename)
        sys.exit(1)

    block_num = 0

    # Wait for initial ACK from server
    while True:
        buffer, _ = sockfd.recvfrom(BUF_SIZE)
        opcode, recv_block_num = struct.unpack("!HH", buffer[:4])

        if opcode == OP_ACK and recv_block_num == block_num:
            print("Initial ACK received")
            break

    block_num = 1
    while True:
        data_block = os.read(fd, DATA_SIZE)
        if not data_block:
            break

        buffer = struct.pack(f"!HH{len(data_block)}s", OP_DATA, block_num, data_block)

        while True:
            sockfd.sendto(buffer, server_addr)
            print(f"Block {block_num} sent, waiting for ACK")

            sockfd.settimeout(1)  # 1 second timeout
            try:
                buffer, _ = sockfd.recvfrom(BUF_SIZE)
            except socket.timeout:
                print(f"Retrying block {block_num}")
                continue

            opcode, recv_block_num = struct.unpack("!HH", buffer[:4])

            if opcode == OP_ACK and recv_block_num == block_num:
                print(f"Valid ACK for block {block_num} received")
                block_num += 1
                break
            elif opcode == OP_ERROR:
                print("Error from server:", buffer[4:].decode())
                os.close(fd)
                sys.exit(1)
            else:
                print("Unexpected packet received")
                os.close(fd)
                sys.exit(1)

    os.close(fd)


if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python3 script.py <server_ip> <server_port> <filename> <mode>")
        sys.exit(1)

    server_ip = sys.argv[1]
    server_port = int(sys.argv[2])
    filename = sys.argv[3]
    mode = sys.argv[4]

    sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_addr = (server_ip, server_port)

    if mode == "r":
        send_rrq(sockfd, server_addr, filename, "octet")
        receive_file(sockfd, server_addr, filename)
    elif mode == "w":
        send_wrq(sockfd, server_addr, filename, "octet")
        send_file(sockfd, server_addr, filename)
    else:
        print("Invalid mode. Use 'r' for read or 'w' for write.")
        sys.exit(1)

    sockfd.close()
