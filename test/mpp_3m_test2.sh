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

            nohup ./bin/mpp_dataplane_proxy 1 2 10.0.0.1 50020 10.0.2.1 50020 10.0.3.1 50020 127.0.0.1 51020 > /dev/null 2>&1 &

            ssh jiangnan@10.0.2.1 "
                cd Desktop/MPP
                nohup ./bin/mpp_egress 3 1 10.0.1.1 50020 10.0.2.1 50020 10.0.3.1 50020 127.0.0.1 50050 10.0.2.1 51000 > /dev/null 2>&1 &
            "
            ssh jc3377@10.0.0.2 "
                cd Desktop/MPP
                nohup ./bin/mpp_ingress 1 2 127.0.0.1 50000 10.0.0.1 50020 10.0.1.1 50020 10.0.0.2 51000 > /dev/null 2>&1 &
            "

            echo_red_text "finish spawn basic components"

            sleep 2
            
            echo_red_text "start health check"
            
            ./bin/mpp_controller 127.0.0.1 51020 health_check
            ./bin/mpp_controller 10.0.2.1 51000 health_check
            ./bin/mpp_controller 10.0.0.2 51000 health_check
            ;;
        close)
            echo_red_text "start close"

            # actually only need to close dataplane proxies that are still using
            ./bin/mpp_controller 10.0.0.2 51000 close
            ./bin/mpp_controller 10.0.2.1 51000 close
            ./bin/mpp_controller 127.0.0.1 51020 close

            echo_red_text "finish close"
            ;;
        *)
            echo_red_text $"Usage: {start|close}"
    esac
done
