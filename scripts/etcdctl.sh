
export ETCDCTL_API=3
export ETCDCTL_ENDPOINTS="http://172.24.131.15:2370"

etcdctl get "" --prefix

etcdctl lease grant 30

etcdctl put ttt tttt  --lease=0b429631df80e41d

etcdctl lease  keep-alive 0b429631df80e41d
