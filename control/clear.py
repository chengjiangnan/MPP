import sys
import subprocess

def ssh_cmd(address, cmd):
        return "ssh {}@{} 'echo {} | {}'".format("jc3377", address, "jc3377", cmd)

def exec_cmd(cmd):
        print(cmd)
        try:
            subprocess.call(cmd, shell=True, stdout=sys.stdout)
        except Exception as e:
            print("execute error: {}".format(e))

for address in ["127.0.0.1", "10.0.0.2", "10.0.2.1", "10.0.5.2", "10.0.7.2"]:
    ssh_command = ""
    for port in range(59985, 60000):
        exec_cmd("./bin/mpp_controller {} {} close".format(address, port))
        ssh_command += " sudo -S kill $(lsof -t -i:{});".format(port)
    exec_cmd(ssh_cmd(address, ssh_command))