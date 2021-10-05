#!/bin/bash

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

            echo_red_text "./bin/mpp_ingress 1 1 127.0.0.1 50000 127.0.0.1 50020 127.0.0.1 51000 > /dev/null 2>&1 &"
            ./bin/mpp_ingress 1 1 127.0.0.1 50000 127.0.0.1 50020 127.0.0.1 51000 > /dev/null 2>&1 &

            echo_red_text "./bin/mpp_dataplane_proxy 1 1 127.0.0.1 50020 127.0.0.1 50040 127.0.0.1 51020 > /dev/null 2>&1 &"
            ./bin/mpp_dataplane_proxy 1 1 127.0.0.1 50020 127.0.0.1 50040 127.0.0.1 51020 > /dev/null 2>&1 &

            echo_red_text "./bin/mpp_egress 1 1 127.0.0.1 50040 127.0.0.1 50050 127.0.0.1 51040 > /dev/null 2>&1 &"
            ./bin/mpp_egress 1 1 127.0.0.1 50040 127.0.0.1 50050 127.0.0.1 51040 > /dev/null 2>&1 &

            echo_red_text "finish spawn basic components"
            
            sleep 2

            echo_red_text "start health_check"
            ./bin/mpp_controller 127.0.0.1 51000 health_check
            ./bin/mpp_controller 127.0.0.1 51020 health_check
            ./bin/mpp_controller 127.0.0.1 51040 health_check
            ;;
        update_topo)
            echo_red_text "start updating topology"
            echo_red_text "./bin/mpp_dataplane_proxy  1 1 127.0.0.1 50021 127.0.0.1 50041 127.0.0.1 51021 > /dev/null 2>&1 &"
            ./bin/mpp_dataplane_proxy  1 1 127.0.0.1 50021 127.0.0.1 50041 127.0.0.1 51021 > /dev/null 2>&1 &

            sleep 1

            echo_red_text "./bin/mpp_controller 127.0.0.1 51000 update_topo_pre" 
            ./bin/mpp_controller 127.0.0.1 51000 update_topo_pre 1 127.0.0.1 50021

            echo_red_text "./bin/mpp_controller 127.0.0.1 51040 update_topo_pre"
            ./bin/mpp_controller 127.0.0.1 51040 update_topo_pre 1 127.0.0.1 50041

            echo_red_text "./bin/mpp_controller 127.0.0.1 51040 update_topo 1 127.0.0.1 50041 &"
            ./bin/mpp_controller 127.0.0.1 51040 update_topo &
            server_pid=$!

            echo_red_text "./bin/mpp_controller 127.0.0.1 51000 update_topo 1 127.0.0.1 50021" 
            ./bin/mpp_controller 127.0.0.1 51000 update_topo
            wait server_pid
            echo_red_text "finish updating topology"
            ;;
        reset)
            echo_red_text "start reset"
            echo_red_text "./bin/mpp_controller 127.0.0.1 51000 reset" 
            ./bin/mpp_controller 127.0.0.1 51000 reset

            echo_red_text "./bin/mpp_controller 127.0.0.1 51040 reset"
            ./bin/mpp_controller 127.0.0.1 51040 reset
            echo_red_text "finish reset"
            ;;
        close)
            echo_red_text "start close"
            echo_red_text "./bin/mpp_controller 127.0.0.1 51000 close"
            ./bin/mpp_controller 127.0.0.1 51000 close

            echo_red_text "./bin/mpp_controller 127.0.0.1 51020 close"
            ./bin/mpp_controller 127.0.0.1 51020 close

            echo_red_text "./bin/mpp_controller 127.0.0.1 51021 close"
            ./bin/mpp_controller 127.0.0.1 51021 close

            echo_red_text "./bin/mpp_controller 127.0.0.1 51040 close"
            ./bin/mpp_controller 127.0.0.1 51040 close

            echo_red_text "finish close"
            ;;
        *)
            echo_red_text $"Usage: {start|change_topo|reset|close}"
    esac
done
