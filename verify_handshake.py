import socket
import base64
import hashlib
import struct

def verify_handshake():
    host = '127.0.0.1'
    port = 8080
    
   
    key = base64.b64encode(b'1234567890123456').decode('utf-8')
    
  
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((host, port))
        print(f"🔌 Connected to {host}:{port}")
        
       
        request = (
            f"GET / HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            f"Sec-WebSocket-Version: 13\r\n"
            f"\r\n"
        )
        s.sendall(request.encode('utf-8'))
        
       
        response = s.recv(4096).decode('utf-8')
        print("📩 Received response:")
        print(response)
        
        
        if "101 Switching Protocols" in response:
            print("✅ Handshake successful (Status 101)")
        else:
            print("❌ Handshake failed")
            return

       
        headers = response.split('\r\n')
        accept_key = None
        for h in headers:
            if h.lower().startswith('sec-websocket-accept:'):
                accept_key = h.split(':')[1].strip()
                break
        
        magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
        expected_key = base64.b64encode(hashlib.sha1((key + magic).encode('utf-8')).digest()).decode('utf-8')
        
        if accept_key == expected_key:
            print(f"✅ Sec-WebSocket-Accept verified: {accept_key}")
        else:
            print(f"❌ Sec-WebSocket-Accept mismatch! Expected {expected_key}, got {accept_key}")

    except ConnectionRefusedError:
        print("❌ Connection refused. Is the server running?")
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        s.close()

if __name__ == "__main__":
    verify_handshake()
