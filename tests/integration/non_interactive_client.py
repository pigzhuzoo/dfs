#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
非交互式DFS客户端
用于性能测试，直接执行PUT和GET命令
"""

import subprocess
import os
import sys
import time
import socket
import struct
import hashlib
from pathlib import Path

class NonInteractiveDFSClient:
    def __init__(self, config_file="conf/dfc.conf"):
        # 确保配置文件路径是绝对路径
        if not os.path.isabs(config_file):
            self.config_file = os.path.join(os.path.dirname(os.path.dirname(__file__)), config_file)
        else:
            self.config_file = config_file
        self.config = self._parse_config()
        
    def _parse_config(self):
        """解析配置文件"""
        config = {
            'servers': [],
            'username': '',
            'password': '',
            'encryption_type': 'AES_256_GCM'
        }
        
        with open(self.config_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line.startswith('Server'):
                    parts = line.split()
                    if len(parts) >= 3:
                        server_name = parts[1]
                        address = parts[2]
                        host, port = address.split(':')
                        config['servers'].append({
                            'name': server_name,
                            'host': host,
                            'port': int(port)
                        })
                elif line.startswith('Username:'):
                    config['username'] = line.split(':', 1)[1].strip()
                elif line.startswith('Password:'):
                    config['password'] = line.split(':', 1)[1].strip()
                elif line.startswith('EncryptionType:'):
                    config['encryption_type'] = line.split(':', 1)[1].strip()
        
        return config
    
    def _get_mod_value(self, file_path):
        """计算文件的mod值"""
        # 使用MD5哈希计算mod值
        with open(file_path, 'rb') as f:
            content = f.read()
            md5_hash = hashlib.md5(content).hexdigest()
            mod = int(md5_hash, 16) % 4
        return mod
    
    def _send_command(self, server_host, server_port, command):
        """发送命令到服务器"""
        try:
            # 创建socket连接
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(30)  # 30秒超时
            sock.connect((server_host, server_port))
            
            # 发送命令
            sock.sendall(command.encode())
            
            # 接收响应
            response = sock.recv(4096)
            
            sock.close()
            return response.decode()
        except Exception as e:
            print(f"Error communicating with server {server_host}:{server_port}: {e}")
            return None
    
    def put_file(self, local_file_path, remote_file_name):
        """上传文件到DFS"""
        start_time = time.time()
        
        try:
            # 确保本地文件路径是绝对路径
            if not os.path.isabs(local_file_path):
                local_file_path = os.path.abspath(local_file_path)
            
            # 检查本地文件是否存在
            if not os.path.exists(local_file_path):
                return False, 0, 0, f"Local file not found: {local_file_path}"
            
            # 使用dfc客户端进行PUT操作
            # 创建临时脚本文件
            script_content = f"PUT {local_file_path} {remote_file_name}\nEXIT\n"
            script_path = "/tmp/dfc_script.txt"
            
            with open(script_path, 'w') as f:
                f.write(script_content)
            
            # 执行dfc命令
            dfc_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), "bin", "dfc")
            config_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), self.config_file)
            
            result = subprocess.run(
                [dfc_path, config_path],
                input=script_content,
                text=True,
                capture_output=True,
                timeout=120,
                cwd=os.path.dirname(os.path.dirname(__file__))
            )
            
            end_time = time.time()
            latency = end_time - start_time
            
            success = result.returncode == 0 and ("File uploaded successfully" in result.stdout or "successfully" in result.stdout.lower())
            
            if success:
                file_size = os.path.getsize(local_file_path)
                throughput = file_size / latency / (1024 * 1024)  # MB/s
                return True, latency, throughput, ""
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                return False, latency, 0, error_msg
                
        except subprocess.TimeoutExpired:
            return False, 0, 0, "PUT operation timed out"
        except Exception as e:
            return False, 0, 0, f"PUT operation error: {str(e)}"
        finally:
            # 清理临时脚本文件
            if os.path.exists(script_path):
                os.remove(script_path)
    
    def get_file(self, remote_file_name, local_file_path):
        """从DFS下载文件"""
        start_time = time.time()
        
        try:
            # 确保本地文件路径是绝对路径
            if not os.path.isabs(local_file_path):
                local_file_path = os.path.abspath(local_file_path)
            
            # 确保本地目录存在
            local_dir = os.path.dirname(local_file_path)
            if not os.path.exists(local_dir):
                os.makedirs(local_dir, exist_ok=True)
            
            # 使用dfc客户端进行GET操作
            # 创建临时脚本文件
            script_content = f"GET {remote_file_name} {local_file_path}\nEXIT\n"
            script_path = "/tmp/dfc_script.txt"
            
            with open(script_path, 'w') as f:
                f.write(script_content)
            
            # 执行dfc命令
            dfc_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), "bin", "dfc")
            config_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), self.config_file)
            
            result = subprocess.run(
                [dfc_path, config_path],
                input=script_content,
                text=True,
                capture_output=True,
                timeout=120,
                cwd=os.path.dirname(os.path.dirname(__file__))
            )
            
            end_time = time.time()
            latency = end_time - start_time
            
            success = result.returncode == 0 and os.path.exists(local_file_path) and os.path.getsize(local_file_path) > 0
            
            if success:
                file_size = os.path.getsize(local_file_path)
                throughput = file_size / latency / (1024 * 1024)  # MB/s
                return True, latency, throughput, file_size, ""
            else:
                error_msg = result.stderr if result.stderr else "File not downloaded"
                return False, latency, 0, 0, error_msg
                
        except subprocess.TimeoutExpired:
            return False, 0, 0, 0, "GET operation timed out"
        except Exception as e:
            return False, 0, 0, 0, f"GET operation error: {str(e)}"
        finally:
            # 清理临时脚本文件
            if os.path.exists(script_path):
                os.remove(script_path)


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python3 non_interactive_client.py <command> <local_path> <remote_path>")
        print("Commands: PUT, GET")
        sys.exit(1)
    
    command = sys.argv[1].upper()
    local_path = sys.argv[2]
    remote_path = sys.argv[3]
    
    client = NonInteractiveDFSClient()
    
    if command == "PUT":
        success, latency, throughput, error = client.put_file(local_path, remote_path)
        if success:
            print(f"PUT successful: {throughput:.2f} MB/s, {latency:.3f}s")
        else:
            print(f"PUT failed: {error}")
    elif command == "GET":
        success, latency, throughput, file_size, error = client.get_file(remote_path, local_path)
        if success:
            print(f"GET successful: {throughput:.2f} MB/s, {latency:.3f}s, {file_size} bytes")
        else:
            print(f"GET failed: {error}")
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)