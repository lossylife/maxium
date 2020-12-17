#!/bin/bash

if [ ! $# = 10 ]; then
    echo "usage: ./lb.sh add/del upstream_ip upstream_port lb_subnet lb_ip ipip_dev_name mark_output mark_input route_table_id_output route_table_id_input"
    exit
fi

op=$1
upstream_ip=$2
upstream_port=$3
lb_subnet=$4
lb_ip=$5
ipip_dev_name=$6
mark_output=$7
mark_input=$8
route_table_id_output=$9
route_table_id_input=${10}

ip link add name ${ipip_dev_name} type ipip local ${lb_ip} remote ${upstream_ip}
ip link set ${ipip_dev_name} up
ip addr add ${lb_subnet} dev ${ipip_dev_name}

ip route flush table ${route_table_id_output}
ip route add 0.0.0.0/0 dev ${ipip_dev_name} table ${route_table_id_output}
ip rule add fwmark ${mark_output} lookup ${route_table_id_output}

ip route flush table ${route_table_id_input}
ip route add local 0.0.0.0/0 dev lo table ${route_table_id_input}
ip rule add fwmark ${mark_input} lookup ${route_table_id_input}

iptables -t mangle -A OUTPUT -p tcp -d ${upstream_ip} --dport ${upstream_port} -j MARK --set-xmark ${mark_output}/0xffffffff
iptables -t mangle -A PREROUTING -p tcp -s ${upstream_ip} --sport ${upstream_port} -j MARK --set-xmark ${mark_input}/0xffffffff

