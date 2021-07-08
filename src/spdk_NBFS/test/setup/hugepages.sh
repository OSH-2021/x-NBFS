#!/usr/bin/env bash
testdir=$(readlink -f "$(dirname "$0")")
rootdir=$(readlink -f "$testdir/../../")
source "$testdir/common.sh"

shopt -s extglob nullglob

declare -a nodes_sys=()

declare -i default_huges=0
declare -i no_nodes=0
declare -i nr_hugepages=0

default_huges=$(get_meminfo Hugepagesize)
default_huge_nr=/sys/kernel/mm/hugepages/hugepages-${default_huges}kB/nr_hugepages
global_huge_nr=/proc/sys/vm/nr_hugepages

get_nodes() {
	local node

	for node in /sys/devices/system/node/node+([0-9]); do
		nodes_sys[${node##*node}]=$(< "$node/hugepages/hugepages-${default_huges}kB/nr_hugepages")
	done
	no_nodes=${#nodes_sys[@]}
	((no_nodes > 0))
}

clear_hp() {
	local node hp

	for node in "${!nodes_sys[@]}"; do
		for hp in "/sys/devices/system/node/node$node/hugepages/hugepages-"*; do
			echo 0 > "$hp/nr_hugepages"
		done
	done

	export CLEAR_HUGE=yes
}

get_test_nr_hugepages() {
	local size=$1 # kB
	if (($# > 1)); then
		shift
		local node_ids=("$@")
	fi

	((size >= default_huges))

	nr_hugepages=$(((size + default_huges - 1) / default_huges))
	get_test_nr_hugepages_per_node "${node_ids[@]}"
}

get_test_nr_hugepages_per_node() {
	local user_nodes=("$@")

	local _nr_hugepages=$nr_hugepages
	local _no_nodes=$no_nodes

	local -g nodes_test=()

	if ((${#user_nodes[@]} > 0)); then
		for _no_nodes in "${user_nodes[@]}"; do
			nodes_test[_no_nodes]=$nr_hugepages
		done
		return 0
	elif ((${#nodes_hp[@]} > 0)); then
		for _no_nodes in "${!nodes_hp[@]}"; do
			nodes_test[_no_nodes]=${nodes_hp[_no_nodes]}
		done
		return 0
	fi

	while ((_no_nodes > 0)); do
		nodes_test[_no_nodes - 1]=$((_nr_hugepages / _no_nodes))
		: $((_nr_hugepages -= nodes_test[_no_nodes - 1]))
		: $((--_no_nodes))
	done
}

verify_nr_hugepages() {
	local node
	local sorted_t
	local sorted_s

	echo "nr_hugepages=$nr_hugepages"
	(($(< "$default_huge_nr") == nr_hugepages))
	(($(< "$global_huge_nr") == nr_hugepages))
	(($(get_meminfo HugePages_Total) == nr_hugepages))

	get_nodes

	# There's no obvious way of determining which NUMA node is going to end
	# up with an odd number of hugepages in case such number was actually
	# allocated by the kernel. Considering that, let's simply check if our
	# expaction is met by sorting and comparing it with nr of hugepages that
	# was actually allocated on each node.

	for node in "${!nodes_test[@]}"; do
		sorted_t[nodes_test[node]]=1 sorted_s[nodes_sys[node]]=1
		echo "node$node=${nodes_sys[node]}"
	done
	[[ ${!sorted_s[*]} == "${!sorted_t[*]}" ]]
}

# Test cases
default_setup() {
	# Default HUGEMEM (8G) alloc on node0
	get_test_nr_hugepages $((HUGEMEM * 1024)) 0
	setup
	verify_nr_hugepages
}

per_node_2G_alloc() {
	# 2G alloc per node, total N*2G pages
	local IFS=","

	get_test_nr_hugepages $((2048 * 1024)) "${!nodes_sys[@]}"
	NRHUGE=$nr_hugepages HUGENODE="${!nodes_sys[*]}" setup
	nr_hugepages=$((nr_hugepages * ${#nodes_sys[@]})) verify_nr_hugepages
}

even_2G_alloc() {
	# 2G alloc spread across N nodes
	get_test_nr_hugepages $((2048 * 1024))
	NRHUGE=$nr_hugepages HUGE_EVEN_ALLOC=yes setup
	verify_nr_hugepages
}

odd_alloc() {
	# Odd 2049MB alloc across N nodes
	get_test_nr_hugepages $((2049 * 1024))
	HUGEMEM=2049 HUGE_EVEN_ALLOC=yes setup
	verify_nr_hugepages
}

custom_alloc() {
	# Custom alloc: node0 == 512 pages [node1 == 1024 pages]

	local IFS=","

	local node
	local nodes_hp=()

	local nr_hugepages=0

	nodes_hp[0]=512
	if ((${#nodes_sys[@]} > 1)); then
		nodes_hp[1]=1024
	fi

	for node in "${!nodes_hp[@]}"; do
		HUGENODE+=("nodes_hp[$node]=${nodes_hp[node]}")
		((nr_hugepages += nodes_hp[node]))
	done

	get_test_nr_hugepages_per_node
	HUGENODE="${HUGENODE[*]}" setup
	verify_nr_hugepages
}

hp_status() {
	# Parse status from last verification

	local node
	local size free total

	((${#nodes_sys[@]} > 0))

	while read -r node size free _ total; do
		size=${size/kB/} node=${node#node}
		((size == default_huges)) || continue
		((free == nodes_test[node]))
		((total == nodes_test[node]))
	done < <(setup output status |& grep "node[0-9]")
}

get_nodes
clear_hp

run_test "default_setup" default_setup
run_test "per_node_2G_alloc" per_node_2G_alloc
run_test "even_2G_alloc" even_2G_alloc
run_test "odd_alloc" odd_alloc
run_test "custom_alloc" custom_alloc
run_test "hp_status" hp_status

clear_hp
