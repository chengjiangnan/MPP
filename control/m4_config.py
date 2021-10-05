from lp import mcf_max_flow
from ctrl_module import Scheduler, App, Link, Pop, MppProg


pops = [ Pop(0, "jc3377",   "127.0.0.1", "jc3377"), 
         Pop(1, "jc3377",   "10.0.0.2",  "jc3377"),
         Pop(2, "jc3377",   "10.0.2.1",  "jc3377"),
         Pop(3, "jc3377",   "10.0.5.2",  "jc3377") ]

links = [ 
          Link(0, 50, "enp1s0f1", [pops[0], pops[3]], ["10.0.5.1", "10.0.5.2"]),
          Link(1, 50, "enp3s0f2", [pops[3], pops[0]], ["10.0.5.2", "10.0.5.1"]),

          Link(2, 150, "enp3s0f1", [pops[2], pops[3]], ["10.0.4.1", "10.0.4.2"]),
          Link(3, 150, "enp3s0f3", [pops[3], pops[2]], ["10.0.4.2", "10.0.4.1"]),

          Link(4, 100, "enp3s0f0", [pops[2], pops[1]], ["10.0.1.1", "10.0.1.2"]),
          Link(5, 100, "enp1s0f1", [pops[1], pops[2]], ["10.0.1.2", "10.0.1.1"]),

          Link(6, 50, "enp1s0f0", [pops[2], pops[0]], ["10.0.2.1", "10.0.2.2"]),
          Link(7, 50, "enp3s0f0", [pops[0], pops[2]], ["10.0.2.2", "10.0.2.1"]),

          Link(8, 50, "enp1s0f0", [pops[0], pops[1]], ["10.0.0.1", "10.0.0.2"]),
          Link(9, 50, "enp1s0f0", [pops[1], pops[0]], ["10.0.0.2", "10.0.0.1"])
           ]

for link in links:
  link.pops[0].backend_links.append(link)
  link.pops[1].listen_links.append(link)

scheduler = Scheduler(pops, links, mcf_max_flow)

apps = [
         App(scheduler, 3, 1, 50000, 50050)
       ]
