from lp import mcf_max_flow
from ctrl_module import Scheduler, App, Link, Pop, MppProg

# Topology:
# 3 ----- 2 ----- 1 ----- 4
# |         \   /         |
# |          \ /          |
#  ---------- 0 ----------

pops = [ Pop(0, "jc3377",   "132.236.59.101", "jc3377"), 
         Pop(1, "jc3377",   "132.236.59.102",  "jc3377"),
         Pop(2, "jc3377",   "132.236.59.57",  "jc3377"),
         Pop(3, "jc3377",   "132.236.59.103",  "jc3377"),
         Pop(4, "jc3377",   "132.236.59.118",  "jc3377") ]

fifty = 50
hundred = 100
hundred_fifty = 150

links = [ 
          Link(0, 50, "enp1s0f1", [pops[0], pops[3]], ["10.0.5.1", "10.0.5.2"]),
          Link(1, fifty, "enp3s0f2", [pops[3], pops[0]], ["10.0.5.2", "10.0.5.1"]),

          Link(2, 150, "enp1s0f3", [pops[0], pops[4]], ["10.0.7.1", "10.0.7.2"]),
          Link(3, hundred_fifty, "enp1s0f1", [pops[4], pops[0]], ["10.0.7.2", "10.0.7.1"]),

          Link(4, 150, "enp3s0f1", [pops[2], pops[3]], ["10.0.4.1", "10.0.4.2"]),
          Link(5, hundred_fifty, "enp3s0f3", [pops[3], pops[2]], ["10.0.4.2", "10.0.4.1"]),

          Link(6, hundred, "enp3s0f0", [pops[2], pops[1]], ["10.0.1.1", "10.0.1.2"]),
          Link(7, 100, "enp1s0f1", [pops[1], pops[2]], ["10.0.1.2", "10.0.1.1"]),

          Link(8, fifty, "enp1s0f0", [pops[1], pops[4]], ["10.0.6.1", "10.0.6.2"]),
          Link(9, 50, "enp1s0f0", [pops[4], pops[1]], ["10.0.6.2", "10.0.6.1"]),

          Link(10, 50, "enp1s0f1", [pops[2], pops[0]], ["10.0.3.1", "10.0.3.2"]),
          Link(11, fifty, "enp1s0f2", [pops[0], pops[2]], ["10.0.3.2", "10.0.3.1"]),

          Link(12, fifty, "enp3s0f0", [pops[0], pops[1]], ["10.0.10.1", "10.0.10.2"]),
          Link(13, 50, "enp3s0f0", [pops[1], pops[0]], ["10.0.10.2", "10.0.10.1"])
        ]

# links = [ 
#           Link(0, 50, "enp1s0f1", [pops[0], pops[3]], ["10.0.5.1", "10.0.5.2"]),
#           Link(1, 50, "enp3s0f2", [pops[3], pops[0]], ["10.0.5.2", "10.0.5.1"]),

#           Link(2, 150, "enp1s0f3", [pops[0], pops[4]], ["10.0.7.1", "10.0.7.2"]),
#           Link(3, 150, "enp1s0f1", [pops[4], pops[0]], ["10.0.7.2", "10.0.7.1"]),

#           Link(4, 150, "enp3s0f1", [pops[2], pops[3]], ["10.0.4.1", "10.0.4.2"]),
#           Link(5, 150, "enp3s0f3", [pops[3], pops[2]], ["10.0.4.2", "10.0.4.1"]),

#           Link(6, 100, "enp3s0f0", [pops[2], pops[1]], ["10.0.1.1", "10.0.1.2"]),
#           Link(7, 100, "enp1s0f1", [pops[1], pops[2]], ["10.0.1.2", "10.0.1.1"]),

#           Link(8, 50, "enp3s0f0", [pops[1], pops[4]], ["10.0.6.1", "10.0.6.2"]),
#           Link(9, 50, "enp1s0f0", [pops[4], pops[1]], ["10.0.6.2", "10.0.6.1"]),

#           Link(10, 50, "enp1s0f0", [pops[2], pops[0]], ["10.0.2.1", "10.0.2.2"]),
#           Link(11, 50, "enp3s0f0", [pops[0], pops[2]], ["10.0.2.2", "10.0.2.1"]),

#           Link(12, 50, "enp1s0f0", [pops[0], pops[1]], ["10.0.0.1", "10.0.0.2"]),
#           Link(13, 50, "enp1s0f0", [pops[1], pops[0]], ["10.0.0.2", "10.0.0.1"])
#         ]

new_bws = [ 150, 150, 50 ,50, 50, 50, 100, 100, 150, 150, 50, 50, 50, 50 ]

for link in links:
  link.pops[0].backend_links.append(link)
  link.pops[1].listen_links.append(link)

scheduler = Scheduler(pops, links, mcf_max_flow)

apps = [
         App(scheduler, 3, 4, 50000, 50050),
         # App(scheduler, 0, 1, 50001, 50051),
         # App(scheduler, 0, 2, 50002, 50052),
         # App(scheduler, 1, 3, 50003, 50053),
         # App(scheduler, 4, 2, 50004, 50054),

         # App(scheduler, 3, 4, 50005, 50055),
         # App(scheduler, 3, 4, 50006, 50056),
         # App(scheduler, 3, 4, 50007, 50057),
         # App(scheduler, 3, 4, 50008, 50058)
       ]
