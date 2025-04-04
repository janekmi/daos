#!/bin/bash
PREFIX=/opt/daos
BIN=$PREFIX/bin
WD=$(pwd)
# Prerequisite:
# SL_PREFIX=/opt/daos/ sudo -E ./utils/setup_daos_server_helper.sh
case "$1" in
server)
        $BIN/daos_server start --config=$WD/nlt-server-config.yaml --insecure
        ;;
agent)
        $BIN/daos_agent \
                --config-path $WD/dnt_agent/nlt_agent.yaml \
                --insecure \
                --runtime_dir $WD/dnt_agent \
                --logfile /tmp/dnt_agent.log
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
*)
        echo "Unknown command: $1"
        exit 1
        ;;
esac
