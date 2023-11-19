import socket
import json
import matplotlib.pyplot as plt
import pandas as pd
from datetime import datetime
import os
import sys

def update_graphs(data_df, axs):
    # Converter a coluna 'timestamp' para um formato de data e hora adequado
    data_df['timestamp'] = pd.to_datetime(data_df['timestamp'])
    
    # Obter o último valor das médias móveis
    last_avg_temp = data_df['avg_temp'].iloc[-1]
    last_avg_pressure = data_df['avg_pressure'].iloc[-1]
    last_avg_humidity = data_df['avg_humidity'].iloc[-1]
    last_rain_probability = data_df['rain_probability'].iloc[-1]
    # print(f"Last Avg Temp: {last_avg_temp:.2f} °C; Last Avg Pressure: {last_avg_pressure:.2f} hPa; Last Avg Humidity: {last_avg_humidity:.2f}%; Last Rain Probability: {last_rain_probability:.2f}%")

    # Grafico 1: Temperatura e média móvel 10 amostras
    axs[0].cla()
    axs[0].plot(data_df['timestamp'], data_df['temperature'], label='Temperature (°C)')
    axs[0].plot(data_df['timestamp'], data_df['avg_temp'], linestyle='--', color='red', label='Avg Temperature (°C)')
    axs[0].text(0.03, 0.95, f'Temp. Avg: {last_avg_temp:.2f} °C', transform=axs[0].transAxes, fontsize=9, verticalalignment='top', bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.5))
    axs[0].legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), shadow=True, ncol=2)
    axs[0].set_ylabel('Temperature (°C)')    

    # Grafico 2: Pressão e média móvel 10 amostras
    axs[1].cla()
    axs[1].plot(data_df['timestamp'], data_df['pressure'], label='Pressure (hPa)')
    axs[1].plot(data_df['timestamp'], data_df['avg_pressure'], linestyle='--', color='red', label='Avg Pressure (hPa)')
    axs[1].text(0.03, 0.95, f'Pressure Avg: {last_avg_pressure:.2f} hPa', transform=axs[1].transAxes, fontsize=9, verticalalignment='top', bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.5))
    axs[1].legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), shadow=True, ncol=2)
    axs[1].set_ylabel('Pressure (hPa)')

    # Grafico 3: Umidade e média móvel 10 amostras
    axs[2].cla()
    axs[2].plot(data_df['timestamp'], data_df['humidity'], label='Humidity (%)')
    axs[2].plot(data_df['timestamp'], data_df['avg_humidity'], linestyle='--', color='red', label='Avg Humidity (%)')
    axs[2].text(0.03, 0.95, f'Humidity Avg: {last_avg_humidity:.2f}%', transform=axs[2].transAxes, fontsize=9, verticalalignment='top', bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.5))
    axs[2].legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), shadow=True, ncol=2)
    axs[2].set_ylabel('Humidity (%)')

    # Grafico 4: Probabilidade de chuva
    axs[3].cla()
    axs[3].plot(data_df['timestamp'], data_df['rain_probability'], label='Rain Probability (%)', color='green')
    axs[3].text(0.03, 0.95, f'Probability: {last_rain_probability:.2f}%', transform=axs[3].transAxes, fontsize=9, verticalalignment='top', bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.5))
    axs[3].legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), shadow=True, ncol=2)
    axs[3].set_ylabel('Rain Probability (%)')

    # # Adicionar um título geral na parte superior da janela do gráfico
    # fig.suptitle('Climate Parameters Monitor', fontsize=16)
    
    plt.pause(0.1)

def save_to_csv(data_df):
    data_df.to_csv("sensor_data.csv", index=False)

def load_existing_data():
    if os.path.exists("sensor_data.csv"):
        return pd.read_csv("sensor_data.csv")
    else:
        return pd.DataFrame(columns=['temperature', 'pressure', 'humidity', 'timestamp'])

# def calculate_rain_probability(avg_temp, avg_pressure, avg_humidity):
#     # Exemplo de um modelo simples para calcular a probabilidade de chuva
#     # NOTA: Esta fórmula é fictícia e para propósitos de demonstração
#     if avg_humidity > 50 and avg_temp < 30:
#         rain_probability = (avg_humidity - 60) + (20 - avg_temp)
#         # print(f"Rain Probability: {rain_probability:.2f}%")
#     else:
#         rain_probability = 0
    

#     # Limitar a probabilidade entre 0 e 100
#     # return min(max(rain_probability, 0), 100)
#     return rain_probability
def calculate_rain_probability(avg_temp, avg_pressure, avg_humidity):
    # Exemplo revisado de um modelo simples para calcular a probabilidade de chuva

    # Ajustar a lógica para evitar valores negativos
    if avg_humidity > 50 and avg_temp < 30:
        humidity_factor = avg_humidity - 50  # Fator baseado na umidade
        temperature_factor = 30 - avg_temp    # Fator baseado na temperatura
        rain_probability = humidity_factor + temperature_factor
    else:
        rain_probability = 0

    # Limitar a probabilidade entre 0 e 100
    return min(max(rain_probability, 0), 100)

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
    sys.stdout.flush()  # Limpar a saída padrão após enviar

    # Get the host's response, no more than, say, 1,024 bytes
    response_data = sock.recv(1024)
    sys.stdout.flush()  # Limpar a saída padrão após receber
    sresponse = response_data.decode("utf-8")
    print(sresponse)

    if sresponse == "fail":
        sock.close()
        exit()

    # Send a request to the host
    sock.send((device_id + "\n").encode()[:-1])
    sys.stdout.flush()  # Limpar a saída padrão após enviar

    # Get the host's response, no more than, say, 1,024 bytes
    response_data = sock.recv(1024)
    sys.stdout.flush()  # Limpar a saída padrão após receber
    sresponse = response_data.decode("utf-8")
    print(sresponse)

    if sresponse == "fail":
        sock.close()
        exit()

    # Create an empty DataFrame
    data_df = pd.DataFrame(columns=['timestamp', 'temperature', 'avg_temp', 'pressure', 'avg_pressure', 'humidity', 'avg_humidity', 'rain_probability'])

    # Carregar dados existentes, se houver
    data_df = load_existing_data()    

    # Preparing for real-time plotting and data collection
    plt.ion()  # Turn on interactive mode
    fig, axs = plt.subplots(2, 2, figsize=(20, 12))  # Criar 4 subplots em uma grade 2x2
    axs = axs.flatten()  # Converter axs para um array 1D para facilitar o acesso

    # Ajustar o espaçamento entre os subplots
    fig.subplots_adjust(hspace=0.4, wspace=0.4)

    # Adicionar um título geral na parte superior da janela do gráfico
    fig.suptitle('Climate Parameters Monitor', fontsize=16)
    

    try:
        while True:
            # Get the host's response
            response_data = sock.recv(1024).decode("utf-8")
            sys.stdout.flush()  # Limpar a saída padrão após receber
            print(response_data)            

            if response_data == "end" or not response_data:
                break

            try:
                # Parse the JSON data
                data = json.loads(response_data)

                # Adicionando timestamp ao dicionário
                data['timestamp'] = datetime.now()
                new_row = pd.DataFrame([data])  # Convertendo o dicionário para DataFrame

                # Adicionando a nova linha ao DataFrame
                data_df = pd.concat([data_df, new_row], ignore_index=True)

                # Calcular as médias móveis e adicionar ao DataFrame
                data_df['avg_temp'] = data_df['temperature'].rolling(window=10).mean()
                data_df['avg_pressure'] = data_df['pressure'].rolling(window=10).mean()
                data_df['avg_humidity'] = data_df['humidity'].rolling(window=10).mean()

                # Calcular a probabilidade de chuva e adicionar ao DataFrame
                data_df['rain_probability'] = data_df.apply(lambda row: calculate_rain_probability(row['avg_temp'], row['avg_pressure'], row['avg_humidity']), axis=1)

                # Update graphs
                update_graphs(data_df, axs)

                # Salvar o DataFrame atualizado para CSV
                save_to_csv(data_df)
            except json.decoder.JSONDecodeError:
                print("Invalid JSON data")
                sys.stdout.flush()  # Limpar a saída padrão após imprimir
                continue
    finally:
        # Terminate
        sock.close()
        plt.ioff()  # Turn off interactive mode

        # Save the DataFrame to CSV
        data_df.to_csv("sensor_data.csv", index=False)
        print("Data saved to 'sensor_data.csv'")

if __name__ == "__main__":
    main("playback.laced.com.br", "50000", "202321234567")
