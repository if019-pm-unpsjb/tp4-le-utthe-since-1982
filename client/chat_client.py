import socket
import threading
import sys
import signal
import os
import time

BUFFER_SIZE = 1024
DELIMITER = "#"


def signal_handler(sig, frame):
    sys.exit(0)


signal.signal(signal.SIGINT, signal_handler)


def send_messages(client_socket, name):
    while True:
        message = input()
        if message.lower() == "exit":
            client_socket.sendall(message.encode("utf-8"))
            print("Disconnected from chat server")
            client_socket.close()
            sys.exit()
        if message.lower().startswith("file: "):
            filename = message[6:]
            send_file = "file: " + filename
            # try:
            # Get the file size and send it
            file_size = os.path.getsize(filename)
            send_file += DELIMITER + str(file_size)
            print(f"Sending: {filename} {file_size}")
            client_socket.sendall(send_file.encode("utf-8"))
            # Wait for acknowledgment before sending file data
            ack = client_socket.recv(len(b"sr")).decode("utf-8")
            print("+++++ack received++++")
            # if ack != "sr":
            #    print("Failed to receive acknowledgment from server")
            #    continue
            if ack == "sr":
                send_file_func(filename, client_socket)
            else:
                client_socket.close()
                sys.exit()
            # except Exception as e:
            #    print(f"Failed to send file {filename}: {str(e)}")
        else:
            n = "[" + name + "]: "
            message = n + message
            client_socket.sendall(message.encode("utf-8"))


def send_file_func(filename, client_socket):
    # START_RECEIVING = "ack"
    print("ENTERING SEND FILE FUNC---------")
    # client_socket.sendall(b'ack')
    with open(filename, "rb") as file:
        while True:
            bytes_read = file.read(BUFFER_SIZE)
            if not bytes_read:
                break
            client_socket.sendall(bytes_read)
    print(f"File {filename} sent successfully")


def receive_messages(client_socket):
    while True:
        try:
            message = client_socket.recv(BUFFER_SIZE).decode("utf-8")
            if not message:
                print("Disconnected from chat server")
                client_socket.close()
                sys.exit()
            if message.startswith("SENDING_FILE"):
                receive_file(client_socket, message)
            else:
                print(message)
        except Exception as e:
            print(f"Error: {str(e)}")
            client_socket.close()
            sys.exit()


def receive_file(client_socket, message):
    file_size = int(message.split(DELIMITER)[1])
    file_name = message[len("SENDING_FILE") : message.find(DELIMITER)]
    client_socket.sendall(b"ready")
    with open(file_name, "wb") as f:
        total_received = 0
        while total_received < file_size:
            data = client_socket.recv(BUFFER_SIZE)
            if not data:
                break
            f.write(data)
            total_received += len(data)
        print(f"Received {total_received} of {file_size} bytes")
    print("File received.")


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 script.py <server_ip> <server_port>")
        sys.exit(1)
    server_ip = sys.argv[1]
    server_port = int(sys.argv[2])

    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        client_socket.connect((server_ip, server_port))
        print("Connected to chat server")
    except Exception as e:
        print(f"Unable to connect to server: {str(e)}")
        return

    print("Type 'exit' then Ctrl+C to exit")

    name = input("Enter your name: ")
    client_socket.sendall(name.encode("utf-8"))

    send_thread = threading.Thread(target=send_messages, args=(client_socket, name))
    receive_thread = threading.Thread(target=receive_messages, args=(client_socket,))

    send_thread.start()
    receive_thread.start()

    send_thread.join()
    receive_thread.join()


if __name__ == "__main__":
    main()
