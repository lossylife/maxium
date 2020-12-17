#!/bin/bash

if [ ! $# = 8 ]; then
    echo "usage: ./upstream.sh add/del upstream_ip upstream_subnet lb_ip lb_mac ipip_dev_name mark route_table_id"
    exit
fi

op=$1
upstream_ip=$2
upstream_subnet=$3
lb_ip=$4
lb_mac=$5
ipip_dev_name=$6
mark=$7
route_table_id=$8

ip link add name ${ipip_dev_name} type ipip local ${upstream_ip} remote ${lb_ip}
ip link set ${ipip_dev_name} up
ip addr add ${upstream_subnet} dev ${ipip_dev_name}

ip route flush table ${route_table_id}
ip route add 0.0.0.0/0 dev ${ipip_dev_name} table ${route_table_id}
ip rule add fwmark ${mark} lookup ${route_table_id}

iptables -A PREROUTING -t mangle -m mac --mac-source ${lb_mac}  -j MARK --set-mark ${mark}
iptables -A PREROUTING -t mangle -m mark --mark ${mark} -j CONNMARK --save-mark
iptables -A OUTPUT -t mangle -j CONNMARK --restore-mark


