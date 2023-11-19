import socket
import json
import matplotlib.pyplot as plt
import pandas as pd
from datetime import datetime
import os

def update_graphs(data_df, axs):
    # Converter a coluna 'timestamp' para um formato de data e hora adequado
    data_df['timestamp'] = pd.to_datetime(data_df['timestamp'])

    # Calcular as médias móveis para as últimas 10 linhas
    rolling_avg_temp = data_df['temperature'].rolling(window=10).mean()
    rolling_avg_pressure = data_df['pressure'].rolling(window=10).mean()
    rolling_avg_humidity = data_df['humidity'].rolling(window=10).mean()

    # Obter o último valor das médias móveis
    last_avg_temp = rolling_avg_temp.iloc[-1]
    last_avg_pressure = rolling_avg_pressure.iloc[-1]
    last_avg_humidity = rolling_avg_humidity.iloc[-1]

    axs[0].cla()
    axs[0].plot(data_df['timestamp'], data_df['temperature'], label='Temperature (°C)')
    axs[0].plot(data_df['timestamp'], rolling_avg_temp, linestyle='--', color='red', label='Last 10 Avg Temperature (°C)')
    axs[0].text(0.02, 0.95, f'Temperature Avg: {last_avg_temp:.2f} °C', transform=axs[0].transAxes, fontsize=9, verticalalignment='top', bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.5))
    axs[0].legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), shadow=True, ncol=2)
    # axs[0].set_xlabel('Timestamp')
    axs[0].set_ylabel('Temperature (°C)')

    axs[1].cla()
    axs[1].plot(data_df['timestamp'], data_df['pressure'], label='Pressure (hPa)')
    axs[1].plot(data_df['timestamp'], rolling_avg_pressure, linestyle='--', color='red', label='Last 10 Avg Pressure (hPa)')
    axs[1].text(0.02, 0.95, f'Pressure Avg: {last_avg_pressure:.2f} hPa', transform=axs[1].transAxes, fontsize=9, verticalalignment='top', bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.5))
    axs[1].legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), shadow=True, ncol=2)
    # axs[1].set_xlabel('Timestamp')
    axs[1].set_ylabel('Pressure (hPa)')

    axs[2].cla()
    axs[2].plot(data_df['timestamp'], data_df['humidity'], label='Humidity (%)')
    axs[2].plot(data_df['timestamp'], rolling_avg_humidity, linestyle='--', color='red', label='Last 10 Avg Humidity (%)')
    axs[2].text(0.02, 0.95, f'Humidity Avg: {last_avg_humidity:.2f}%', transform=axs[2].transAxes, fontsize=9, verticalalignment='top', bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.5))
    axs[2].legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), shadow=True, ncol=2)
    # axs[2].set_xlabel('Timestamp')
    axs[2].set_ylabel('Humidity (%)')


    plt.pause(0.1)


def save_to_csv(data_df):
    data_df.to_csv("sensor_data.csv", index=False)

def load_existing_data():
    if os.path.exists("sensor_data.csv"):
        return pd.read_csv("sensor_data.csv")
    else:
        return pd.DataFrame(columns=['temperature', 'pressure', 'humidity', 'timestamp'])

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

    # Create an empty DataFrame
    data_df = pd.DataFrame(columns=['temperature', 'pressure', 'humidity'])

    # Carregar dados existentes, se houver
    data_df = load_existing_data()    

    # Preparing for real-time plotting and data collection
    plt.ion()  # Turn on interactive mode
    fig, axs = plt.subplots(3, figsize=(10, 12))

    try:
        while True:
            # Get the host's response
            response_data = sock.recv(1024).decode("utf-8")

            if response_data == "end" or not response_data:
                break

            # Parse the JSON data
            data = json.loads(response_data)

            # Adicionando timestamp ao dicionário
            data['timestamp'] = datetime.now()
            new_row = pd.DataFrame([data])  # Convertendo o dicionário para DataFrame

            # Adicionando a nova linha ao DataFrame
            data_df = pd.concat([data_df, new_row], ignore_index=True)

            # Update graphs
            update_graphs(data_df, axs)

            # Salvar o DataFrame atualizado para CSV
            save_to_csv(data_df)
    finally:
        # Terminate
        sock.close()
        plt.ioff()  # Turn off interactive mode

        # Save the DataFrame to CSV
        data_df.to_csv("sensor_data.csv", index=False)
        print("Data saved to 'sensor_data.csv'")

if __name__ == "__main__":
    main("playback.laced.com.br", "50000", "202321234567")
