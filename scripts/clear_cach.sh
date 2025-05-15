#!/bin/bash

for i in {1..3}; do
  dir="${i}ps"
  (
    mkdir "$dir" && rm -rf $dir/*
  )
  cp ps_main "$dir" && cp config.toml $dir 
done

rm -rf router_storage ps_raft_data ps_storage logs

