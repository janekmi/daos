#!/bin/bash
PREFIX=/opt/daos
BIN=$PREFIX/bin
WD=$(pwd)
# Prerequisite:
# SL_PREFIX=/opt/daos/ sudo -E ./utils/setup_daos_server_helper.sh
case "$1" in
prep)
        sudo mkdir /var/run/daos_server
        sudo chown michalsk:michalsk /var/run/daos_server
        ;;
cleanup)
        rm -rdf /var/daos/config/*
        rm /var/tmp/spdk_pci_lock_*
        rm -rdf /mnt/daos/*
        sudo umount /mnt/daos
        rm /tmp/daos_server.log
        rm /tmp/daos_engine.0.log
        ;;
server)
        rm /var/tmp/spdk_pci_lock_*
        # $BIN/daos_server start --config=$WD/nlt-server-config-mdonssd.yaml --insecure
        $BIN/daos_server start --config=$WD/daos_server_mdonssd.yml --insecure
        # $BIN/daos_server start --config=$WD/nlt-server-config.yaml --insecure
        ;;
agent)
        sudo mkdir -p /var/run/daos_agent
        sudo chown michalsk:michalsk /var/run/daos_agent
        $BIN/daos_agent \
                --config-path $WD/dnt_agent/nlt_agent.yaml \
                --insecure \
                --logfile /tmp/dnt_agent.log
        # --runtime_dir /var/dnt_agent \
        ;;
dmg)
        shift 1
        $BIN/dmg $* --insecure
        # e.g.
        # system stop
        # system erase
        # storage format
        # pool list
        # pool create NLT --scm-size 128M
        # system query
        ;;
daos)
        shift 1
        $BIN/daos $*
        # --insecure
        # --config-path $WD/dnt_agent/nlt_agent.yaml
        ;;
*)
        echo "Unknown command: $1"
        exit 1
        ;;
esac
