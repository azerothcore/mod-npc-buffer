#!/usr/bin/env bash

MOD_NPCBUFFER_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/" && pwd )"

source "$MOD_NPCBUFFER_ROOT/conf/conf.sh.dist"

if [ -f "$MOD_NPCBUFFER_ROOT/conf/conf.sh" ]; then
    source "$MOD_NPCBUFFER_ROOT/conf/conf.sh"
fi 