import socket
import time
import random

HOST = '127.0.0.1'
PORT = 9000

batch_size = 5000
data_block = ""
for i in range(batch_size):
    data_block += f"{i},1700000000,{random.uniform(20.0, 90.0):.2f}\n"

encoded_block = data_block.encode()

print(f"🔥 INICIANDO FLOOD TCP: {len(encoded_block) / 1024:.2f} KB por pacote")

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    try:
        s.connect((HOST, PORT))
        print("✅ CONECTADO AO APEXSTREAM CORE")
        
        counter = 0
        start = time.time()
        
        while True:
            s.sendall(encoded_block)
            counter += batch_size
            
            if counter % 1000000 == 0:
                print(f"🚀 Enviados: {counter / 1000000:.1f} Milhões de linhas...")
    except ConnectionRefusedError:
        print("❌ Conexão recusada. O ApexStream Core está rodando?")
    except KeyboardInterrupt:
        print("🛑 Parando flood.")
    except Exception as e:
        print(f"❌ Erro: {e}")
