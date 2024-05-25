# File: tcp_chat_client.py
# Redes

import socket
import threading
import sys
import signal

# SERVER_IP = "127.0.0.1"
# SERVER_PORT = 8080
BUFFER_SIZE = 1024


def signal_handler(sig, frame):
    sys.exit(0)


# Register the signal handler for SIGINT (Ctrl+C)
signal.signal(signal.SIGINT, signal_handler)


def send_messages(client_socket, name):
    while True:
        print(f"> ", end="")  # Print name and ">" with no newline
        message = input()
        if message.lower() == "exit":
            client_socket.sendall(message.encode("utf-8"))
            print("Disconnected from chat server")
            client_socket.close()
            sys.exit()
        n = "[" + name + "]: "
        message = n + message
        client_socket.sendall(message.encode("utf-8"))


def receive_messages(client_socket):
    while True:
        try:
            message = client_socket.recv(BUFFER_SIZE).decode("utf-8")
            if not message:
                print("Disconnected from chat server")
                client_socket.close()
                sys.exit()
            print(message)
        except Exception as e:
            print(f"Error: {str(e)}")
            client_socket.close()
            sys.exit()


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

    # Send the client's name
    name = input("Enter your name: ")
    client_socket.sendall(name.encode("utf-8"))

    # Create threads for sending and receiving messages
    send_thread = threading.Thread(target=send_messages, args=(client_socket, name))
    receive_thread = threading.Thread(target=receive_messages, args=(client_socket,))

    send_thread.start()
    receive_thread.start()

    send_thread.join()
    receive_thread.join()


if __name__ == "__main__":
    main()
