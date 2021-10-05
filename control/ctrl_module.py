import time
import sys
import subprocess
from threading import Thread, currentThread, Lock

from utils import color_text_yellow, color_text_green, color_text_original


def ssh_cmd(pop, cmd):
        return "ssh {}@{} 'echo {} | {}'".format(pop.ssh_usr, pop.ssh_ip, pop.password, cmd)


def exec_cmd(cmd):
        print(cmd)
        try:
            subprocess.call(cmd, shell=True, stdout=sys.stdout)
        except Exception as e:
            print("execute error: {}".format(e))


class Scheduler:
    def __init__(self, pops, links, mcf_func):
        self.pops = pops
        self.links = links
        self.apps = []
        self.mcf_func = mcf_func

        self.classid_minor_space = list(range(10, 100))
        self.port_space = list(range(55000, 60000))

    def require_classid_minor(self):
        try:
            return self.classid_minor_space.pop()
        except Exception as e:
            print("require_classid_minor error: {}".format(e))

    def require_port_number(self):
        try:
            return self.port_space.pop()
        except Exception as e:
            print("require_port_number error: {}".format(e))

    def compute_mcf_optimal(self):
        ingress_egress_pairs = []
        for app in self.apps:
            ingress_egress_pairs.append([app.ingress, app.egress])

        try:
            _max_throughput, _flow_throughputs, _flows = self.mcf_func(
                self.pops, self.links, ingress_egress_pairs, min_flow=50)
        except Exception as e:
            print("MCF computation error: {}".format(e))

        max_throughput = int(_max_throughput)
        flow_throughputs = [int(i) for i in _flow_throughputs]
        flows = [[int(i) for i in _flow_set] for _flow_set in _flows]
        print(color_text_yellow("[MCF RESULT] max_throughput: {}, flow_throughputs: {}, "
                                "flows: {}".format(max_throughput, flow_throughputs, flows)))
        return max_throughput, flow_throughputs, flows

    def commit_new_apps(self, new_apps):
        for app in new_apps:
            if app in self.apps:
                continue
            self.apps.append(app)
        if len(self.apps) == 0:
            print(color_text_green("[SCHEDULER] no apps!"))
            return
        max_throughput, flow_throughputs, flows = self.compute_mcf_optimal()

        # multi-thread for app update_flow
        thread_list = []
        for i in range(len(self.apps)): 
            # self.apps[i].update_flows(flows[i], flow_throughputs[i])
            t = Thread(target=self.apps[i].update_flows, args=(flows[i], flow_throughputs[i]))
            t.start()
            thread_list.append(t)

        for t in thread_list:
            t.join()
        

    def terminate_old_apps(self, old_apps):
        thread_list = []
        null_flows = [0 for i in range(len(self.links))]

        for app in old_apps:
            if app in self.apps:
                # app.update_flows(null_flows, 0)
                t = Thread(target=app.update_flows, args=(null_flows, 0))
                t.start()
                thread_list.append(t)
                self.apps.remove(app)

        if len(self.apps) == 0:
            print(color_text_green("[SCHEDULER] no apps!"))
            for t in thread_list:
                t.join()
            return

        max_throughput, flow_throughputs, flows = self.compute_mcf_optimal()
        for i in range(len(self.apps)):
            t = Thread(target=self.apps[i].update_flows, args=(flows[i], flow_throughputs[i]))
            t.start()
            thread_list.append(t)

        for t in thread_list:
            t.join()

    @staticmethod
    def setup_network_on_pop(pop):
        delay = 120
        cmd = pop.reset_iptables()
        for link in pop.backend_links:
            cmd += link.tc_reset_qdisc()
            bw = link.bandwidth
            cmd += link.tc_qdisc_init("{}mbit".format(bw), "{}kbit".format(32 * bw), 
                                      "50ms", delay)
        with pop.lock:
            exec_cmd(ssh_cmd(pop, cmd))

    # @staticmethod
    # def change_network_on_pop(pop):
    #     cmd =""
    #     for link in pop.backend_links:
    #         bw = new_bws[link.idx]
    #         if link.bandwidth == bw:
    #             continue

    #         link.bandwidth == bw
    #         cmd += link.tc_change_tbf_bandwidth("{}mbit".format(bw),
    #                                             "{}kbit".format(32 * bw), "20ms")
    #     exec_cmd(ssh_cmd(pop, cmd))

    @staticmethod
    def destroy_network_on_pop(pop):
        cmd = pop.reset_iptables()
        for link in pop.backend_links:
            cmd += link.tc_reset_qdisc()
        with pop.lock:
            exec_cmd(ssh_cmd(pop, cmd))

    def network_operation(self, target_func):
        thread_list = []
        for pop in self.pops:
            t = Thread(target=target_func, args=(pop, ))
            t.start()
            thread_list.append(t)

        for t in thread_list:
            t.join()

    def setup_network(self):
        self.network_operation(Scheduler.setup_network_on_pop)

    # def change_network_bandwidth(self, new_bws):
    #     self.network_operation(Scheduler.change_network_on_pop)

    def destroy_network(self):
        self.network_operation(Scheduler.destroy_network_on_pop)

class App:
    def __init__(self, scheduler, ingress, egress, ingress_listen_port, egress_backend_port):
        self.scheduler = scheduler
        self.pops = scheduler.pops
        self.links = scheduler.links

        self.ingress = ingress
        self.egress = egress

        self.ingress_listen_port = ingress_listen_port
        self.egress_backend_port = egress_backend_port
        self.comm_port = scheduler.require_port_number() # for ingress and egress

        self.proxy_port = None
        self.proxy_comm_port = None

        self.flows = [0 for i in range(len(self.links))]
        self.throughput = 0

        self.classid_minor = scheduler.require_classid_minor()

        self.used_pops = set()
        self.used_links = set()
        self.mpp_progs = [None for i in range(len(self.pops))]

    def provision_network_on_pop(self, pop, new_used_links, new_flows, new_port):
        cmd = ""
        for link in pop.backend_links:
            if link.idx not in new_used_links:
                continue

            # whether it is a totally new link?
            # if link.idx not in self.used_links:
            #     cmd += link.tc_add_htb_rate(str(new_flows[link.idx]) + "mbit", self.classid_minor)
            # else:
            #     # it is an existing link, whether rate has changed?
            #     if new_flows[link.idx] != self.flows[link.idx]:
            #         cmd += link.tc_change_htb_rate(str(new_flows[link.idx]) + "mbit", self.classid_minor)

            # add class tag to this link
            if new_port != self.proxy_comm_port:
                cmd += link.iptables_add_rule(self.classid_minor, new_port)

        if cmd == "":
            return 

        with pop.lock:
            exec_cmd(ssh_cmd(pop, cmd))

    def destroy_network_on_pop(self, pop, new_used_links, new_flows):
        cmd = ""
        for link in pop.backend_links:
            if link.idx not in self.used_links:
                continue

            # whether it is a totally old link?
            if link.idx not in new_used_links:
                cmd += link.tc_del_htb_rate(str(self.flows[link.idx]) + "mbit", self.classid_minor)

            # add class tag to this link
            cmd += link.iptables_del_rule(self.classid_minor, self.proxy_port)

        # this can happen to sink node
        if cmd == "":
            return

        with pop.lock:
            exec_cmd(ssh_cmd(pop, cmd))

    # provision network only add rate limit to totally new links, change rate limit to existing links
    # and add class tags to all new_used_links
    def provision_logical_network(self, new_used_pops, new_used_links, new_flows, new_port):
        assert len(self.flows) == len(new_flows)

        print(color_text_yellow("[NETWORK] start provision"))
        thread_list = []
        for pop_idx in new_used_pops:
            pop = self.pops[pop_idx]
            t = Thread(target=self.provision_network_on_pop, args=(pop, new_used_links, new_flows, new_port))
            t.start()
            thread_list.append(t)

        for t in thread_list:
            t.join()
        print(color_text_yellow("[NETWORK] finish provision"))

    # destroy network only delete rate limit for totally old links
    # and delete class tags to all old links
    def destroy_logical_network(self, new_used_links, new_flows):
        assert len(self.flows) == len(new_flows)

        print(color_text_yellow("[NETWORK] start delete"))
        thread_list = []
        for pop_idx in self.used_pops:
            pop = self.pops[pop_idx]
            t = Thread(target=self.destroy_network_on_pop, args=(pop, new_used_links, new_flows))
            t.start()
            thread_list.append(t)

        for t in thread_list:
            t.join()
        print(color_text_yellow("[NETWORK] finish delete"))

    def update_flows(self, new_flows, throughput):
        assert len(self.flows) == len(new_flows)

        # no need to do any update if flows remain unchanged
        if self.flows == new_flows:
            print(color_text_yellow("flows are exactly the same!"))
            return

        # check whether we need to update MPP
        new_used_pops = set() 
        new_used_links = set()
        for i in range(len(new_flows)):
            if new_flows[i] > 0:
                new_used_links.add(i)
                for pop in self.links[i].pops:
                    new_used_pops.add(pop.idx)
        update_mpp = (self.used_links != new_used_links)

        # if no need to update mpp, we can just change the network by using old port
        if not update_mpp:
            self.provision_logical_network(new_used_pops, new_used_links, new_flows, self.proxy_port)
            self.flows = new_flows
            self.throughput = throughput
            return

        # if new_used_links is None, it means we need to close all the mpps
        if len(new_used_links) == 0:
            if self.proxy_port == None:
                return
            self.close()
            self.destroy_logical_network(new_used_links, new_flows)
            self.flows = new_flows
            self.throughput = throughput
            return

        # we need to update both mpp and network
        proxy_port = self.scheduler.require_port_number()
        proxy_comm_port = self.scheduler.require_port_number()

        # provision the network first
        self.provision_logical_network(new_used_pops, new_used_links, new_flows, proxy_port)

        # then configure mpp_progs
        print(color_text_yellow("[MPP] start config"))
        first_configuration = (self.proxy_port == None)
        new_mpp_progs = [None for i in range(len(self.pops))]

        pops = self.pops
        links = self.links
        ingress = self.ingress
        egress = self.egress

        # generate and deploy mpp_progs in parallel
        thread_list = []
        for pop_idx in new_used_pops:

            pop = pops[pop_idx]
            direct = "bin"
            prog_name = "mpp_dataplane_proxy"
            comm_ip = pop.ssh_ip
            comm_port = proxy_comm_port
            log_file = "mpp_{}.log".format(comm_port)

            listen_links = []
            listen_port = proxy_port
            listen_write_ratio = []
            backend_links = []
            backend_port = proxy_port
            backend_write_ratio = []

            for link in pop.listen_links:
                if link.idx in new_used_links:
                    listen_links.append(link)
                    listen_write_ratio.append(new_flows[link.idx])

            for link in pop.backend_links:
                if link.idx in new_used_links:
                    backend_links.append(link)
                    backend_write_ratio.append(new_flows[link.idx])

            # if not first time configuration, need to update ingress and egress
            # (while other programs need to be deployed)
            if pop.idx == ingress and not first_configuration:
                mpp_prog = self.mpp_progs[ingress]
                mpp_prog.update_backend_links(backend_links)
                mpp_prog.update_backend_port(proxy_port)
                new_mpp_progs[ingress] = mpp_prog
                continue

            if pop.idx == egress and not first_configuration:
                mpp_prog = self.mpp_progs[egress]
                mpp_prog.update_listen_links(listen_links)
                mpp_prog.update_listen_port(proxy_port)
                new_mpp_progs[egress] = mpp_prog
                continue

            if pop.idx == ingress:
                listen_links = [ Link(-1, 1000, "lo", [pops[ingress], pops[ingress]],
                                      ["127.0.0.1", "127.0.0.1"]) ]
                listen_write_ratio = [throughput]
                listen_port = self.ingress_listen_port
                comm_port = self.comm_port
                log_file = "mpp_{}.log".format(comm_port)
                prog_name = "mpp_adv_ingress"
            elif pop.idx == egress:
                backend_links = [ Link(-1, 1000, "lo", [pops[egress], pops[egress]],
                                      ["127.0.0.1", "127.0.0.1"]) ]
                backend_write_ratio = [throughput]
                backend_port = self.egress_backend_port
                comm_port = self.comm_port
                log_file = "mpp_{}.log".format(comm_port)
                prog_name = "mpp_adv_egress"
                
            mpp_prog = MppProg(direct, prog_name, log_file, pop, comm_ip, comm_port,
                               listen_links, backend_links, listen_port, backend_port,
                               listen_write_ratio, backend_write_ratio)
            new_mpp_progs[pop_idx] = mpp_prog

            t = Thread(target=mpp_prog.deploy)
            t.start()
            thread_list.append(t)

        for t in thread_list:
            t.join()

        print(color_text_yellow("[MPP] finish deploy new modules"))

        if not first_configuration:
            time.sleep(1)

            new_mpp_progs[ingress].update_topo_pre()
            new_mpp_progs[egress].update_topo_pre()

            t1 = Thread(target=new_mpp_progs[ingress].update_topo)
            t2 = Thread(target=new_mpp_progs[egress].update_topo) 
       
            t1.start()
            t2.start()
       
            t1.join()
            t2.join()

            for  pop_idx in self.used_pops:
                if pop_idx not in [ingress, egress]:
                    self.mpp_progs[pop_idx].close()

            self.destroy_logical_network(new_used_links, new_flows)

            print(color_text_yellow("[MPP] finish transition"))

        self.used_pops = new_used_pops
        self.used_links = new_used_links
        self.mpp_progs = new_mpp_progs

        self.proxy_port = proxy_port
        self.proxy_comm_port = proxy_comm_port

        self.flows = new_flows
        self.throughput = throughput

        print(color_text_yellow("[MPP] finish config"))

    def health_check(self):
        for  pop_idx in self.used_pops:
            self.mpp_progs[pop_idx].health_check()

    def reset(self):
        if self.ingress not in self.used_pops or self.egress not in self.used_pops:
            return

        self.mpp_progs[self.ingress].reset()
        self.mpp_progs[self.egress].reset()

    def close(self):
        thread_list = []
        for  pop_idx in self.used_pops:
            t = Thread(target=self.mpp_progs[pop_idx].close)
            t.start()
            thread_list.append(t)
        
        for t in thread_list:
            t.join()


class Link:
    def __init__(self, idx, bandwidth, interface, pops, ips):
        self.idx = idx
        self.bandwidth = bandwidth
        self.interface = interface

        self.pops = pops
        self.ips = ips

    def update_bandwidth(self, new_bandwidth):
        self.bandwidth = new_bandwidth
        return self.bandwidth

    def tc_qdisc_init(self, rate, burst, latency, delay):
        if delay == 0:
            cmd = (" sudo -S tc qdisc add dev {} root handle 1:0 tbf rate {} "
                   "burst {} latency {};".format(self.interface, rate, burst, latency))
            # cmd += (" sudo -S tc qdisc add dev {} parent 1:1 handle 20: htb default 2;"
            #     .format(self.interface))
            # cmd += (" sudo -S tc class add dev {} parent 20: classid 20:1 htb rate {};"
            #         .format(self.interface, rate))
            # cmd += (" sudo -S tc class add dev {} parent 20:1 classid 20:2 htb rate {};"
            #         .format(self.interface, rate))
            return cmd

        # add delay
        cmd = (" sudo -S tc qdisc add dev {} root handle 1:0 netem delay {}ms;"
               .format(self.interface, delay))

        # add tbf rate limit
        cmd += (" sudo -S tc qdisc add dev {} parent 1:1 handle 10: tbf rate {} "
                "burst {} latency {};".format(self.interface, rate, burst, latency))

        # add htb rate limit
        # cmd += (" sudo -S tc qdisc add dev {} parent 10:1 handle 20: htb default 2;"
        #         .format(self.interface))
        # cmd += (" sudo -S tc class add dev {} parent 20: classid 20:1 htb rate {};"
        #         .format(self.interface, rate))
        # cmd += (" sudo -S tc class add dev {} parent 20:1 classid 20:2 htb rate {};"
        #         .format(self.interface, rate))
        
        return cmd

    def tc_change_tbf_bandwidth(self, rate, burst, latency):
        return (" sudo -S tc qdisc change dev {} parent 1:1 handle 10: tbf rate {} "
                "burst {} latency {};".format(self.interface, rate, burst, latency))

    def tc_add_htb_rate(self, rate, classid_minor):
        return (" sudo -S tc class add dev {} parent 20:1 classid 20:{} htb rate {};"
                .format(self.interface, classid_minor, rate))

    def tc_change_htb_rate(self, rate, classid_minor):
        return (" sudo -S tc class change dev {} parent 20:1 classid 20:{} htb rate {};"
                .format(self.interface, classid_minor, rate))

    def tc_del_htb_rate(self, rate, classid_minor):
        return (" sudo -S tc class del dev {} parent 20:1 classid 20:{} htb rate {};"
                .format(self.interface, classid_minor, rate))

    def tc_reset_qdisc(self):
        return " sudo -S tc qdisc del dev {} root;".format(self.interface)

    def iptables_add_rule(self, classid_minor, dport):
        return (" sudo -S iptables -t mangle -A POSTROUTING -o {} -p tcp --dport {} -j CLASSIFY "
                "--set-class 20:{};".format(self.interface, dport, classid_minor))

    def iptables_del_rule(self, classid_minor, dport):
        return (" sudo -S iptables -t mangle -D POSTROUTING -o {} -p tcp --dport {} -j CLASSIFY "
                "--set-class 20:{};".format(self.interface, dport, classid_minor))


class Pop:
    def __init__(self, idx, ssh_usr, ssh_ip, password, 
                 listen_links=None, backend_links=None):
        self.idx = idx

        self.ssh_usr = ssh_usr
        self.ssh_ip = ssh_ip
        self.password = password

        self.listen_links = listen_links if listen_links != None else []
        self.backend_links = backend_links if backend_links != None else []

        self.lock = Lock()

    def reset_iptables(self):
        return " sudo -S iptables -t mangle -F;"


class MppProg:
    def __init__(self, direct, prog_name, log_file, pop, comm_ip, comm_port,
                 listen_links, backend_links, listen_port, backend_port,
                 listen_write_ratio, backend_write_ratio):
        self.direct = direct
        self.prog_name = prog_name
        self.log_file = log_file

        self.pop = pop
        self.comm_ip = comm_ip
        self.comm_port = comm_port

        self.listen_links = listen_links
        self.backend_links = backend_links

        self.listen_port = listen_port
        self.backend_port = backend_port

        self.listen_write_ratio = listen_write_ratio
        self.backend_write_ratio = backend_write_ratio

    def update_listen_links(self, listen_links):
        self.listen_links = listen_links

    def update_backend_links(self, backend_links):
        self.backend_links = backend_links

    def update_listen_port(self, listen_port):
        self.listen_port = listen_port

    def update_backend_port(self, backend_port):
        self.backend_port = backend_port

    def deploy(self):
        cmd = "cd Desktop/MPP; nohup {}/{} {} {} ".format(
            self.direct, self.prog_name, len(self.listen_links), len(self.backend_links))
        for i in range(len(self.listen_links)):
            listen_ip = self.listen_links[i].ips[1]
            listen_ratio = self.listen_write_ratio[i]
            cmd += "{} {} {} ".format(listen_ip, self.listen_port, listen_ratio)
        for i in range(len(self.backend_links)):
            backend_ip = self.backend_links[i].ips[1]
            backend_ratio = self.backend_write_ratio[i]
            cmd += "{} {} {} ".format(backend_ip, self.backend_port, backend_ratio)

        cmd += "{} {} ".format(self.comm_ip, self.comm_port)
        cmd += "> {} 2>&1 &".format(self.log_file)

        ssh_cmd = "ssh {}@{} '{}'".format(self.pop.ssh_usr, self.pop.ssh_ip, cmd)

        with self.pop.lock:
            exec_cmd(ssh_cmd)
                
    def health_check(self):
        cmd = "./bin/mpp_controller {} {} health_check".format(self.comm_ip, self.comm_port)
        exec_cmd(cmd)

    def update_topo_pre(self):
        if self.prog_name not in ["mpp_adv_ingress", "mpp_adv_egress"]:
            print("module does not support update!")
            return

        cmd = "./bin/mpp_controller {} {} update_topo_pre".format(self.comm_ip, self.comm_port)
        if self.prog_name == "mpp_adv_ingress":
            cmd += " {}".format(len(self.backend_links))
            for backend_link in self.backend_links:
                cmd += " {} {}".format(backend_link.ips[1], self.backend_port)
        else:
            cmd += " {}".format(len(self.listen_links))
            for listen_link in self.listen_links:
                cmd += " {} {}".format(listen_link.ips[1], self.listen_port)

        exec_cmd(cmd)

    def update_topo(self):
        cmd = "./bin/mpp_controller {} {} update_topo".format(self.comm_ip, self.comm_port)
        exec_cmd(cmd)

    def reset(self):
        if self.prog_name not in ["mpp_adv_ingress", "mpp_adv_egress"]:
            print("module does not support reset!")
            return
        cmd = "./bin/mpp_controller {} {} reset".format(self.comm_ip, self.comm_port)
        exec_cmd(cmd)

    def close(self):
        cmd = "./bin/mpp_controller {} {} close".format(self.comm_ip, self.comm_port)
        exec_cmd(cmd)
