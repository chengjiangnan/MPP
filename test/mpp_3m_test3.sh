#!/bin/bash

num_pops=3

# 0 is ingress; 1 is egress; others are
ssh_addrs[0]="jc3377@10.0.0.2"
ssh_addrs[1]="jiangnan@10.0.2.1"
ssh_addrs[2]="jc3377@127.0.0.1"

comm_addrs[0]="10.0.0.2 51000"
comm_addrs[1]="10.0.2.1 51000"
comm_addrs[2]="127.0.0.1 51020"

num_listen[0]="1"
num_listen[1]="2"
num_listen[2]="1"

listen_addrs[0]="127.0.0.1 50000"
listen_addrs[1]="10.0.1.1 50020 10.0.2.1 50020"
listen_addrs[2]="10.0.0.1 50020"

num_back[0]="2"
num_back[1]="1"
num_back[2]="1"

back_addrs[0]="10.0.0.1 50020 10.0.1.1 50020"
back_addrs[1]="127.0.0.1 50050"
back_addrs[2]="10.0.2.1 50020"

# new_configuration
n_comm_addrs[0]="10.0.0.2 51000"
n_comm_addrs[1]="10.0.2.1 51000"
n_comm_addrs[2]="127.0.0.1 51021"

n_num_listen[0]="1"
n_num_listen[1]="3"
n_num_listen[2]="1"

n_listen_addrs[0]="127.0.0.1 50000"
n_listen_addrs[1]="10.0.1.1 50021 10.0.2.1 50021 10.0.3.1 50021"
n_listen_addrs[2]="10.0.0.1 50021"

n_num_back[0]="2"
n_num_back[1]="1"
n_num_back[2]="2"

n_back_addrs[0]="10.0.0.1 50021 10.0.1.1 50021"
n_back_addrs[1]="127.0.0.1 50050"
n_back_addrs[2]="10.0.2.1 50021 10.0.3.1 50021"

echo_red_text ()
{
    echo -e "\e[31m$1\e[0m"
}

while true
do
    echo_red_text "Enter command:"
    read cmd
    case $cmd in
        start)
            echo_red_text "start spawn basic components"

            ssh "${ssh_addrs[0]}" "
                cd Desktop/MPP
                nohup ./bin/mpp_adv_ingress ${num_listen[0]} ${num_back[0]} ${listen_addrs[0]} ${back_addrs[0]} ${comm_addrs[0]} > ingress.log 2>&1 &
            "

            ssh "${ssh_addrs[1]}" "
                cd Desktop/MPP
                nohup ./bin/mpp_adv_egress ${num_listen[1]} ${num_back[1]} ${listen_addrs[1]} ${back_addrs[1]} ${comm_addrs[1]} > egress.log 2>&1 &
            "

            for i in $(seq 2 $(expr $num_pops - 1))
            do 
                ssh "${ssh_addrs[${i}]}" "
                cd Desktop/MPP
                nohup ./bin/mpp_dataplane_proxy ${num_listen[${i}]} ${num_back[${i}]} ${listen_addrs[${i}]} ${back_addrs[${i}]} ${comm_addrs[${i}]} > dp.log 2>&1 &
                "
            done

            echo_red_text "finish spawn basic components"

            sleep 2
            
            echo_red_text "start health check"

            for i in $(seq 0 $(expr $num_pops - 1))
            do
                eval "./bin/mpp_controller ${comm_addrs[${i}]} health_check"
            done

            ;;
        update_topo)
            echo_red_text "start updating topology"

            for i in $(seq 2 $(expr $num_pops - 1))
            do 
                ssh "${ssh_addrs[${i}]}" "
                cd Desktop/MPP
                nohup ./bin/mpp_dataplane_proxy ${n_num_listen[${i}]} ${n_num_back[${i}]} ${n_listen_addrs[${i}]} ${n_back_addrs[${i}]} ${n_comm_addrs[${i}]} > n_dp.log 2>&1 &
                "
            done
            echo_red_text "spawn new mpp_dataplane_proxy(s)"

            sleep 1

            echo_red_text "start health check"
            for i in $(seq 2 $(expr $num_pops - 1))
            do
                eval "./bin/mpp_controller ${n_comm_addrs[${i}]} health_check"
            done
            sleep 1

            echo_red_text "start updating topology"
            eval "./bin/mpp_controller ${comm_addrs[0]} update_topo_pre ${n_num_back[0]} ${n_back_addrs[0]}"
            eval "./bin/mpp_controller ${comm_addrs[1]} update_topo_pre ${n_num_listen[1]} ${n_listen_addrs[1]}"

            eval "./bin/mpp_controller ${comm_addrs[1]} update_topo &"
            egress_pid=$!
            eval "./bin/mpp_controller ${comm_addrs[0]} update_topo"
            wait $egress_pid
            echo_red_text "finish updating topology"

            echo_red_text "start terminating old proxy"
            for i in $(seq 2 $(expr $num_pops - 1))
            do
                eval "./bin/mpp_controller ${comm_addrs[${i}]} close"
            done
            echo_red_text "finish terminating old proxy"

            echo_red_text "start updating variables"
            for i in $(seq 0 $(expr $num_pops - 1))
            do
                comm_addrs[${i}]=${n_comm_addrs[${i}]}
                num_listen[${i}]=${n_num_listen[${i}]}
                listen_addrs[${i}]=${n_listen_addrs[${i}]}
                num_back[${i}]=${n_num_back[${i}]}
                back_addrs[${i}]=${n_back_addrs[${i}]}
            done
            echo_red_text "finish updating variables"

            ;;
        reset)
            echo_red_text "start reset"
            eval "./bin/mpp_controller ${comm_addrs[0]} reset"
            eval "./bin/mpp_controller ${comm_addrs[1]} reset"
            echo_red_text "finish reset"
            ;;
        close)
            echo_red_text "start close"

            for i in $(seq 0 $(expr $num_pops - 1))
            do
                eval "./bin/mpp_controller ${comm_addrs[${i}]} close"
            done

            echo_red_text "finish close"
            ;;
        *)
            echo_red_text $"Usage: {start|change_topo|reset|close}"
    esac
done
