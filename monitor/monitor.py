import socket
import sys
import thread
import time

import json
import pingparsing
from multiprocessing import Process, Queue

ping_dest_ip = "127.0.0.1"

count = 500
interval = 0.02
num_of_threads = 4

curr_ts = 0.0
curr_loss_rate = 0.0
curr_rtt = 0.0


def ping_thread(count, interval, queue):
    ping_parser = pingparsing.PingParsing()
    transmitter = pingparsing.PingTransmitter()
    transmitter.destination = ping_dest_ip
    transmitter.count = count
    transmitter.ping_option = "-i" + str(interval)
    result = transmitter.ping()

    queue.put(ping_parser.parse(result).as_dict())


def ping_master(count, interval, num_of_threads):
    global curr_ts
    global curr_loss_rate
    global curr_rtt

    while True:
        q = Queue()
        process_list = []

        for num in range(num_of_threads):
            p = Process(target=ping_thread, args=(count, interval, q))
            process_list.append(p)
            p.start()

        for p in process_list:
            p.join()

        pkt_total = 0
        pkt_receive = 0
        rtt_total = 0
        for num in range(num_of_threads):
            json_entry = q.get()
            pkt_total += json_entry["packet_transmit"]
            pkt_receive += json_entry["packet_receive"]
            rtt_total += json_entry["packet_receive"] * json_entry["rtt_avg"]

        curr_ts = time.time()
        curr_loss_rate = (pkt_total - pkt_receive) / (pkt_total + 0.0)
        curr_rtt = rtt_total / pkt_receive

        output = "[{}] pkt_total:{}\npkt_receive:{}\nloss_rate:{}\nrtt_avg:{}".format(
            curr_ts, pkt_total, pkt_receive, curr_loss_rate, curr_rtt)

        print(output)


if __name__ == "__main__":

    if len(sys.argv) < 7:
        print(("[MONITOR] required arguments: ping_dest_ip comm_ip comm_port"
               " count interval num_of_threads"))
        sys.exit()

    ping_dest_ip = sys.argv[1]
    comm_ip = sys.argv[2]
    comm_port = int(sys.argv[3])
    count = int(sys.argv[4])
    interval = float(sys.argv[5])
    num_of_threads = int(sys.argv[6])

    try:
        thread.start_new_thread(ping_master, (count, interval, num_of_threads))
        print("[MONITOR] start ping_master thread")
    except:
        print("[MONITOR] unable to start ping_master thread")
        sys.exit()

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (comm_ip, comm_port)
        sock.bind(server_address)
        sock.listen(1)
        print("[MONITOR] bind to and listen on {}".format(server_address))
    except:
        print("[MONITOR] unable to bind to and listen on {}".format(server_address))
        sys.exit()


    while True:
        connection, client_address = sock.accept()

        try:
            print("[MONITOR] recv connection from {}".format(client_address))

            cmd = connection.recv(4)

            print("[MONITOR] command is: {}".format(cmd))

            if cmd == "stop":
                connection.close()
                sock.close()
                sys.exit()
            elif cmd == "chek":
                connection.sendall("success")
            elif cmd == "stat":
                data = "{} {} {}".format(curr_ts, curr_loss_rate, curr_rtt)
                print("[MONITOR] sending stat [{}] back to the client".format(data))
                connection.sendall(data)
                
        finally:
            connection.close()
