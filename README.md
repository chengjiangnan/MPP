# Multipath Protocol

An implementation of multipath protocol (MPP), written in C language.

Note: the OS we used is Linux (Ubuntu).

## Compilation

```
make
```
will compile all the required modules into `./bin` folder.

### key modules: 

We have three key modules. 1) `mpp_adv_ingress`, MPP client side module; 2)`mpp_adv_egress`: MPP server side module; 3)`mpp_dataplane_proxy`: MPP proxy at each middle node. They have the common usage:
```
<program_name> <listen_num> <backend_num> <listen_ip1> <listen_port1> <listen_write_ratio1> ... <backend_ip1> <backend_port1> <backend_write_ratio1> ... <comm_ip> <comm_port>
```
Explanations:
* `listen_num` and `backend_num` specifies the number of incoming and outgoing TCP connections, respectively. For `mpp_adv_ingress`, `listen_num` must be 1; and for `mpp_adv_egress`, `backend_num` must be 1.
* `listen_ip1`, `listen_port1`, and `listen_write_ratio1` specify the server-side ip address, port number and the expected throughput for incoming TCP 1. Similarly, we need to specify these three values for other incoming TCPs. `listen_write_ratio1` is currently an obsolete field.
* `backend_ip1`, `backend_port1`, and `backend_write_ratio1` specify the server-side ip address, port number and the expected throughput for outgoing TCP 1. Similarly, we need to specify these three values for other outgoing TCPs. `backend_write_ratio1` is currently an obsolete field. 
* `comm_ip` and `comm_port` are the ip address and port number associated with a communication server that a module needs to listen on. We use `mpp_controller` to send commands to a module. 

### Controll
`mpp_controller` is used to send commands to the above three module:
```
<program_name> <remote_ip> <remote_port> <operation>
```
where `remote_ip` and `remote_port` are the `comm_ip` and `comm_port` of a module.

Supported operations:
* `health_check`: check whether a module is healthy or not;
* `update_topo_pre`: tell a module to make preparations for topology update;
* `update_topo`: tell a module to perform topology update;
* `reset`: reset a module after the MPP connection is terminated (don't call it if the old flow is still ongoing);
* `close`: terminate a module.

## Python Controller

To simply the control process, we also provide a Python controller, which can not only spawn/terminate MPP modules, but also build/destroy networks. 
Usage:
```
python3 cli.py <config_file>
```

An example `config_file` is `m5_config.py`.

