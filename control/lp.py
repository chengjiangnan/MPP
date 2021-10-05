import numpy as np
from scipy.optimize import linprog
from numpy.linalg import solve

from ctrl_module import Link, Pop


def isCyclicUtil(pop, links, link_throughputs, visited, recStack, recLinksIdx): 
  
        # Mark current node as visited and  
        # adds to recursion stack 
        visited[pop.idx] = True
        recStack[pop.idx] = True
  
        # Recur for all neighbours 
        # if any neighbour is visited and in  
        # recStack then graph is cyclic
        for i in range(len(links)):
            link = links[i]
            link_throughput = link_throughputs[i]
            if pop.idx == link.pops[0].idx and link_throughput > 1.0:
                neighbour = link.pops[1]
                if visited[neighbour.idx] == False:
                    cycle_idx, cycle_finish = isCyclicUtil(neighbour, links, link_throughputs,
                                                           visited, recStack, recLinksIdx)
                    if cycle_idx != None:
                        if cycle_finish == False:
                            recLinksIdx.append(i)
                        if pop.idx == cycle_idx:
                            cycle_finish = True
                        return cycle_idx, cycle_finish
                elif recStack[neighbour.idx] == True:
                    recLinksIdx.append(i)
                    return neighbour.idx, False
  
        # The node needs to be poped from  
        # recursion stack before function ends 
        recStack[pop.idx] = False
        return None, False
  
# Returns true and cycle if graph is cyclic; else false 
def isCyclic(pops, links, link_throughputs): 
    visited = [False] * len(pops) 
    recStack = [False] * len(pops)
    recLinksIdx = []
    for pop in pops:
        if visited[pop.idx] == False:
            cycle_idx, cycle_finish = isCyclicUtil(pop, links, link_throughputs,
                                                   visited, recStack, recLinksIdx)
            if cycle_idx != None:
                return True, recLinksIdx
    return False, None

def remove_cycle(pops, links, link_throughputs):
    while True:
        flag, cycle = isCyclic(pops, links, link_throughputs)
        if flag == False:
            return link_throughputs
        else:
            minimum_flow = float("inf")
            for link_idx in cycle:
                if minimum_flow > abs(link_throughputs[link_idx]):
                    minimum_flow = abs(link_throughputs[link_idx])

            for link_idx in cycle:
                if link_throughputs[link_idx] > 0:
                    link_throughputs[link_idx] -= minimum_flow
                else:
                    link_throughputs[link_idx] += minimum_flow


def mcf_max_flow(pops, links, ingress_egress_pairs, min_flow=0):

    num_pops = len(pops)
    num_links = len(links)

    num_flows = len(ingress_egress_pairs)

    # (num_flows * (num_links+1)) number of decision variables
    # (num_flows * num_pops) number of equal constraints
    # (num_links * (num_flows + 1)) number of unequal constraints

    A_eq = [[0] * (num_flows * (num_links+1)) for i in range(num_flows * num_pops)]
    b_eq = [0 for i in range(num_flows * num_pops)]

    A_ub = None
    b_ub = None

    if min_flow == 0:
        A_ub = [[0] * (num_flows * (num_links+1)) for i in range(num_links * (num_flows + 1))]
        b_ub = [0 for i in range(num_links * (num_flows + 1))]
    else:
        A_ub = [[0] * (num_flows * (num_links+1)) 
                for i in range(num_links * (num_flows + 1) + num_flows)]
        b_ub = [0 for i in range(num_links * (num_flows + 1) + num_flows)]

        k = 0
        for i in range(num_links * (num_flows + 1), num_links * (num_flows + 1) + num_flows):
            A_ub[i][k*(num_links+1)+num_links] = -1
            b_ub[i] = 0 - min_flow
            k += 1



    c = [0] * (num_flows * (num_links+1))

    for i in range(num_flows):
        ingress = ingress_egress_pairs[i][0]
        egress = ingress_egress_pairs[i][1]
        A_eq[i*num_pops+ingress][i*(num_links+1)+num_links] = -1
        A_eq[i*num_pops+egress][i*(num_links+1)+num_links] = 1
        c[i*(num_links+1)+num_links] = -1

        for j in range(num_links):
            link = links[j]
            A_eq[i*num_pops+link.pops[0].idx][i*(num_links+1)+j] = 1
            A_eq[i*num_pops+link.pops[1].idx][i*(num_links+1)+j] = -1

    for j in range(num_links):
        link = links[j]
        b_ub[j*(num_flows+1)+num_flows] = link.bandwidth
        for i in range(num_flows):
            A_ub[j*(num_flows+1)+i][i*(num_links+1)+j] = -1
            A_ub[j*(num_flows+1)+num_flows][i*(num_links+1)+j] = 1

    res = linprog(c, A_eq=A_eq, b_eq=b_eq, A_ub=A_ub, b_ub=b_ub,
    bounds=(None, None))

    # print("Max throughput: {}, Flows: {}".format(-res.fun, res.x))

    max_throughput = -res.fun
    link_throughputs = res.x
    
    flow_results = []
    flow_throughputs = []
    for i in range(num_flows):
        flow_results.append(remove_cycle(pops, links, 
            link_throughputs[i*(num_links+1):(i*(num_links+1)+num_links)]))
        flow_throughputs.append(link_throughputs[(i*(num_links+1)+num_links)])

    return max_throughput, flow_throughputs, flow_results 
    