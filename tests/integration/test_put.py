import socket
import struct
import os

def test_dfs_put_command():
    # 连接到DFS1服务器
    host = 'localhost'
    port = 10001
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        print(f"Connected to {host}:{port}")
        
        # 发送包含认证信息的PUT命令
        # FLAG 2 USERNAME Bob PASSWORD ComplextPassword FOLDER / FILENAME test_upload.txt
        put_command = "FLAG 2 USERNAME Bob PASSWORD ComplextPassword FOLDER / FILENAME test_upload.txt"
        put_size = len(put_command)
        
        # 发送命令长度
        sock.send(struct.pack('!i', put_size))
        print(f"Sent command size: {put_size}")
        
        # 发送命令
        sock.send(put_command.encode('utf-8'))
        print(f"Sent command: {put_command}")
        
        # 接收命令响应
        try:
            # 首先接收状态码（int）
            status_bytes = sock.recv(4)
            if len(status_bytes) == 4:
                status = struct.unpack('!i', status_bytes)[0]
                print(f"Status code: {status}")
                
                if status == 1:
                    print("PUT command successful! Ready to receive file splits.")
                    
                    # 模拟发送文件分片
                    # 这里需要模拟客户端发送分片的逻辑
                    # 但为了简化测试，我们先看看服务器是否正确响应
                    
                    # 发送一个简单的分片作为测试
                    # 分片格式: flag(1) + split_id(4) + content_length(4) + content
                    test_content = b"Hello DFS Upload Test!"
                    content_length = len(test_content)
                    split_id = 1
                    
                    # 发送分片
                    payload = bytes([0])  # INITIAL_WRITE_FLAG
                    payload += struct.pack('!i', split_id)
                    payload += struct.pack('!i', content_length)
                    payload += test_content
                    
                    sock.send(payload)
                    print(f"Sent test split (ID: {split_id}, length: {content_length})")
                    
                    # 等待服务器确认
                    ack = sock.recv(1024)
                    if ack:
                        print(f"Server response: {ack.decode('utf-8', errors='ignore')}")
                        
                else:
                    print("PUT command failed!")
                    # 接收错误信息
                    error_response = sock.recv(1024)
                    if error_response:
                        print(f"Error response: {error_response.decode('utf-8', errors='ignore')}")
            else:
                print("Failed to receive status code")
                
        except Exception as e:
            print(f"Error receiving response: {e}")
            
        sock.close()
        print("Connection closed")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_dfs_put_command()