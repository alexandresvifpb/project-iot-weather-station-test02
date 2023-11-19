import socket
import json
import matplotlib.pyplot as plt

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

    # Preparing for real-time plotting
    plt.ion()  # Turn on interactive mode
    fig, axs = plt.subplots(3, figsize=(10, 8))
    temperatures, pressures, humidities = [], [], []

    try:
        while True:
            # Get the host's response
            response_data = sock.recv(1024).decode("utf-8")

            if response_data == "end" or not response_data:
                break

            # Parse the JSON data
            data = json.loads(response_data)
            temperatures.append(data["temperature"])
            pressures.append(data["pressure"])
            humidities.append(data["humidity"])

            # Update Temperature plot
            axs[0].cla()
            axs[0].plot(temperatures, label='Temperature (°C)')
            axs[0].legend()

            # Update Pressure plot
            axs[1].cla()
            axs[1].plot(pressures, label='Pressure (hPa)')
            axs[1].legend()

            # Update Humidity plot
            axs[2].cla()
            axs[2].plot(humidities, label='Humidity (%)')
            axs[2].legend()

            # Draw the plots
            plt.pause(0.1)

    finally:
        # Terminate
        sock.close()
        plt.ioff()  # Turn off interactive mode

if __name__ == "__main__":
    main("playback.laced.com.br", "50000", "202321234567")