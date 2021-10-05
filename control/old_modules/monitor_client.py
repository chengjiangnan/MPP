import socket
import sys

def monitor_ctrl(remote_ip, remote_port, cmd):

    if cmd not in ['stop', 'stat', 'chek']:
        print("[MONITOR_CTRL] invalid cmd!")
        sys.exit()

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (remote_ip, remote_port)
        sock.connect(server_address)
    except:
        print("[MONITOR_CTRL] unable to connect to {}".format(server_address))
        return None

    try:
        sock.sendall(cmd)
        # print("[MONITOR_CTRL] send cmd [{}] to server".format(cmd))

        if cmd == 'stop':
            print("[MONITOR_CTRL] ({}) stop".format(server_address))
            sock.close()
            return True

        if cmd == 'chek':
            data = sock.recv(100)
            result = data == 'success'
            print("[MONITOR_CTRL] ({}) health_check result: [{}]".format(server_address, result))
            return result
        
        data = sock.recv(100)
        data = data.split(' ')

        ts = float(data[0])
        loss_rate = float(data[1])
        rtt = float(data[2])
        # print("[MONITOR_CTRL] ts:{}, loss_rate:{}, rtt:{}".format(ts, loss_rate, rtt))

        sock.close()

        return ts, loss_rate, rtt

    except Exception as e:
        raise Exception(e)
