from lp import mcf_max_flow
from ctrl_module import Config, Topology, Dag, LinkMonitor, Link, Pop, MppProg

pops = [ Pop(0, "jc3377", "10.0.0.2", "jc3377"),
         Pop(1, "jc3377", "10.0.2.1", "jc3377"),
         Pop(2, "jc3377", "127.0.0.1", "jc3377"),
         Pop(3, "jc3377", "127.0.0.1", "jc3377") ]

ingress_egress_pairs = [[0, 3], [2, 1]]

links = [ Link(102, pops[0], "10.0.1.2", "enp1s0f1", pops[1], "10.0.1.1", "enp1s0f1"),
          Link(102, pops[1], "10.0.1.2", "enp1s0f1", pops[0], "10.0.1.1", "enp1s0f1"),
          Link(100, pops[0], "10.0.0.2", "enp1s0f1", pops[2], "10.0.0.1", "enp1s0f1"),
          Link(100, pops[2], "10.0.0.2", "enp1s0f1", pops[0], "10.0.0.1", "enp1s0f1"),
          Link(100, pops[1], "10.0.1.2", "enp1s0f1", pops[3], "10.0.1.1", "enp1s0f1"),
          Link(100, pops[3], "10.0.1.2", "enp1s0f1", pops[1], "10.0.1.1", "enp1s0f1"),
          Link(102, pops[2], "10.0.0.2", "enp1s0f1", pops[3], "10.0.0.1", "enp1s0f1"),
          Link(102, pops[3], "10.0.0.2", "enp1s0f1", pops[2], "10.0.0.1", "enp1s0f1"),
          Link(100, pops[1], "10.0.0.2", "enp1s0f1", pops[2], "10.0.0.1", "enp1s0f1"),
          Link(100, pops[2], "10.0.0.2", "enp1s0f1", pops[1], "10.0.0.1", "enp1s0f1") ]

max_throughput, flow_throughputs, flow_results = mcf_max_flow(
    pops, links, ingress_egress_pairs)

print(max_throughput)
print(flow_throughputs)
print(flow_results)

# should be [200, 0, 0, -200]

# pops = [ Pop(0, "jc3377", "10.0.0.2", "jc3377"),
#          Pop(1, "jiangnan", "10.0.2.1", "jc3377"),
#          Pop(2, "jc3377", "127.0.0.1", "jc3377"),
#          Pop(3, "jc3377", "127.0.0.1", "jc3377"),
#          Pop(4, "jc3377", "127.0.0.1", "jc3377")]



# links = [ Link(100, pops[0], "10.0.1.2", pops[1], "10.0.1.1"),
#           Link(300, pops[0], "10.0.0.2", pops[2], "10.0.0.1"),
#           Link(100, pops[1], "10.0.2.2", pops[2], "10.0.2.1"),
#           Link(100, pops[1], "10.0.3.2", pops[3], "10.0.3.1"),
#           Link(100, pops[2], "10.0.3.2", pops[3], "10.0.3.1"),
#           Link(100, pops[2], "10.0.3.2", pops[4], "10.0.3.1"),
#           Link(100, pops[3], "10.0.3.2", pops[4], "10.0.3.1")]

# print(remove_cycle(pops, links, [150, 100, 50, -100, 50, 50, 150]))
# should be [150, 100, 0, -50, 0, 50, 150]
