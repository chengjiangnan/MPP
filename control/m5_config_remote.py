from lp import mcf_max_flow
from ctrl_module_remote import Scheduler, App, Link, Pop, MppProg

# Topology:
# 0 ----- 1 ----- 3 ----- 4
# |         \   /         |
# |          \ /          |
#  ---------- 2 ----------

pops = [ Pop(0, "jc3377",   "52.247.214.127", "10.0.0.4", "jc3377"), 
         Pop(1, "jc3377",   "13.88.40.108", "10.0.1.4", "jc3377"),
         Pop(2, "jc3377",   "52.162.142.71", "10.0.2.4", "jc3377"),
         Pop(3, "jc3377",   "65.52.36.205",  "10.0.3.4", "jc3377"),
         Pop(4, "jc3377",   "13.90.78.11", "10.0.4.4", "jc3377") ]

two_h = 160

links = [ 
          Link(0, two_h, "eth1", [pops[0], pops[1]], ["10.0.0.5", "10.0.1.4"]), # mark
          Link(1, 200, "eth0", [pops[1], pops[0]], ["10.0.1.4", "10.0.0.5"]),

          Link(2, two_h, "eth0", [pops[0], pops[2]], ["10.0.0.4", "10.0.2.4"]), # mark
          Link(3, 200, "eth0", [pops[2], pops[0]], ["10.0.2.4", "10.0.0.4"]),

          Link(4, 200, "eth1", [pops[1], pops[2]], ["10.0.1.5", "10.0.2.5"]),
          Link(5, two_h, "eth1", [pops[2], pops[1]], ["10.0.2.5", "10.0.1.5"]), # mark

          Link(6, two_h, "eth2", [pops[1], pops[3]], ["10.0.1.6", "10.0.3.4"]), # mark
          Link(7, 200, "eth0", [pops[3], pops[1]], ["10.0.3.4", "10.0.1.6"]),

          Link(8, two_h, "eth2", [pops[2], pops[3]], ["10.0.2.6", "10.0.3.5"]), # mark
          Link(9, 200, "eth1", [pops[3], pops[2]], ["10.0.3.5", "10.0.2.6"]),

          Link(10, 200, "eth3", [pops[2], pops[4]], ["10.0.2.7", "10.0.4.4"]),
          Link(11, two_h, "eth0", [pops[4], pops[2]], ["10.0.4.4", "10.0.2.7"]), # mark

          Link(12, two_h, "eth2", [pops[3], pops[4]], ["10.0.3.6", "10.0.4.5"]), # mark
          Link(13, 200, "eth1", [pops[4], pops[3]], ["10.0.4.5", "10.0.3.6"])
        ]


# multi = 1.2 
# links = [ 
#           Link(0, 990 * multi, "eth0", [pops[0], pops[1]], ["10.0.0.4", "10.0.1.4"]), # mark
#           Link(1, 720 * multi, "eth0", [pops[1], pops[0]], ["10.0.1.4", "10.0.0.4"]),

#           Link(2, 540 * multi, "eth1", [pops[0], pops[2]], ["10.0.0.5", "10.0.2.4"]), # mark
#           Link(3, 430 * multi, "eth0", [pops[2], pops[0]], ["10.0.2.4", "10.0.0.5"]),

#           Link(4, 410 * multi, "eth1", [pops[1], pops[2]], ["10.0.1.5", "10.0.2.5"]),
#           Link(5, 450 * multi, "eth1", [pops[2], pops[1]], ["10.0.2.5", "10.0.1.5"]), # mark

#           Link(6, 680 * multi, "eth2", [pops[1], pops[3]], ["10.0.1.6", "10.0.3.4"]), # mark
#           Link(7, 670 * multi, "eth0", [pops[3], pops[1]], ["10.0.3.4", "10.0.1.6"]),

#           Link(8, 710 * multi, "eth2", [pops[2], pops[3]], ["10.0.2.6", "10.0.3.5"]), # mark
#           Link(9, 810 * multi, "eth1", [pops[3], pops[2]], ["10.0.3.5", "10.0.2.6"]),

#           Link(10, 990 * multi, "eth3", [pops[2], pops[4]], ["10.0.2.7", "10.0.4.4"]),
#           Link(11, 890 * multi, "eth0", [pops[4], pops[2]], ["10.0.4.4", "10.0.2.7"]), # mark

#           Link(12, 720 * multi, "eth2", [pops[3], pops[4]], ["10.0.3.6", "10.0.4.5"]), # mark
#           Link(13, 730 * multi, "eth1", [pops[4], pops[3]], ["10.0.4.5", "10.0.3.6"])
#         ]

new_bws = [ 150, 150, 50 ,50, 50, 50, 100, 100, 150, 150, 50, 50, 50, 50 ]

for link in links:
  # print("here!")
  # print(link.pops[0])
  # print(link.pops[0].backend_links)
  link.pops[0].backend_links.append(link)

  # print("here2!")
  # print(link.pops[1])
  # print(link.pops[1].listen_links)
  link.pops[1].listen_links.append(link)

scheduler = Scheduler(pops, links, mcf_max_flow)

apps = [
         App(scheduler, 0, 4, 50000, 50050),
         App(scheduler, 2, 3, 50001, 50051),
         App(scheduler, 2, 1, 50002, 50052),
         App(scheduler, 3, 0, 50003, 50053),
         App(scheduler, 4, 1, 50004, 50054)
       ]
