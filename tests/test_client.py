import socket
import struct

def test_dfs_list_command():
    """测试LIST命令"""
    # 连接到DFS1服务器
    host = 'localhost'
    port = 10001
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        print(f"Connected to {host}:{port}")
        
        # 发送包含认证信息的LIST命令
        # FLAG 0 USERNAME Bob PASSWORD ComplextPassword FOLDER / FILENAME NULL
        list_command = "FLAG 0 USERNAME Bob PASSWORD ComplextPassword FOLDER / FILENAME NULL"
        list_size = len(list_command)
        
        # 发送命令长度
        sock.send(struct.pack('!i', list_size))
        print(f"Sent command size: {list_size}")
        
        # 发送命令
        sock.send(list_command.encode('utf-8'))
        print(f"Sent command: {list_command}")
        
        # 接收命令响应
        try:
            # 首先接收状态码（int）
            status_bytes = sock.recv(4)
            if len(status_bytes) == 4:
                status = struct.unpack('!i', status_bytes)[0]
                print(f"Status code: {status}")
                
                if status == 1:
                    print("LIST command successful!")
                    # 接收文件信息大小
                    file_info_size_bytes = sock.recv(4)
                    if len(file_info_size_bytes) == 4:
                        file_info_size = struct.unpack('!i', file_info_size_bytes)[0]
                        print(f"File info size: {file_info_size}")
                        
                        if file_info_size > 0:
                            file_info = sock.recv(file_info_size)
                            print(f"Received file info ({len(file_info)} bytes)")
                    
                    # 接收文件夹信息大小
                    folder_info_size_bytes = sock.recv(4)
                    if len(folder_info_size_bytes) == 4:
                        folder_info_size = struct.unpack('!i', folder_info_size_bytes)[0]
                        print(f"Folder info size: {folder_info_size}")
                        
                        if folder_info_size > 0:
                            folder_info = sock.recv(folder_info_size)
                            print(f"Received folder info ({len(folder_info)} bytes)")
                else:
                    print("LIST command failed!")
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

def test_dfs_get_command():
    """测试GET命令"""
    # 连接到DFS1服务器
    host = 'localhost'
    port = 10001
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        print(f"Connected to {host}:{port}")
        
        # 发送包含认证信息的GET命令
        # FLAG 1 USERNAME Bob PASSWORD ComplextPassword FOLDER / FILENAME test.txt
        get_command = "FLAG 1 USERNAME Bob PASSWORD ComplextPassword FOLDER / FILENAME test.txt"
        get_size = len(get_command)
        
        # 发送命令长度
        sock.send(struct.pack('!i', get_size))
        print(f"Sent command size: {get_size}")
        
        # 发送命令
        sock.send(get_command.encode('utf-8'))
        print(f"Sent command: {get_command}")
        
        # 接收命令响应
        try:
            # 首先接收状态码（int）
            status_bytes = sock.recv(4)
            if len(status_bytes) == 4:
                status = struct.unpack('!i', status_bytes)[0]
                print(f"Status code: {status}")
                
                if status == 1:
                    print("GET command successful!")
                    # 接收文件信息大小
                    file_info_size_bytes = sock.recv(4)
                    if len(file_info_size_bytes) == 4:
                        file_info_size = struct.unpack('!i', file_info_size_bytes)[0]
                        print(f"File info size: {file_info_size}")
                        
                        if file_info_size > 0:
                            file_info = sock.recv(file_info_size)
                            print(f"Received file info ({len(file_info)} bytes)")
                            # 这里可以解析文件信息
                else:
                    print("GET command failed!")
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
    print("Testing LIST command...")
    test_dfs_list_command()
    print("\nTesting GET command...")
    test_dfs_get_command()