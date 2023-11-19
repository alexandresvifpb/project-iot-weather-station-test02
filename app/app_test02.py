import socket

def main(address, port, device_id):
    HOST = address
    PORT = int(port)

    print(f"Address: {address}, Port: {port}, Device ID: {device_id}")

    # Create a socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # Connect to the remote host and port
    sock.connect((HOST, PORT))

    print("Connected to server")
    print(f"Device ID: {device_id}")

    # Send a request to the host
    sock.send("app\n".encode()[:-1])

    # Get the host's response, no more than, say, 1,024 bytes
    response_data = sock.recv(1024)
    sresponse = response_data.decode("utf-8")
    print(sresponse)

    if sresponse == "fail":
        sock.close()
        exit()

    # Send a request to the host
    sock.send((device_id + "\n").encode()[:-1])

    # Get the host's response, no more than, say, 1,024 bytes
    response_data = sock.recv(1024)
    sresponse = response_data.decode("utf-8")
    print(sresponse)

    if sresponse == "fail":
        sock.close()
        exit()

    while True:
        # Get the host's response, no more than, say, 1,024 bytes
        response_data = sock.recv(1024).decode("utf-8")
        print(f"Data received: {response_data}")

        if response_data == "end":
            break

    # Terminate
    sock.close()

if __name__ == "__main__":
    main("playback.laced.com.br", "50000", "202321234567")