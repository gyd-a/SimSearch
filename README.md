
rocksdb  支持mvcc


TODO:

1、cpp yaml解析；                          --完成
2、snapshot恢复扩副本问题；                 --完成
3、master并发建表，删表问题，                --完成
4、多表测试；
5、单副本写入失败问题；
6、节点清除扩副本问题；
7、进程退出保存etcd，
8、监控问题、健康借口
9、ps有space启动注册master失败后还可以启动
10、master创建表分布式锁, 当有create/delete space失败时，不可进行space操作


介入gamma问题:  
1、接入问题，
2、add、delete、get、search开发，
3、rocksdb全算量dump问题；


不紧急的特性：
1、create/delete space 发生错误时，保存状态锁表
2、ps退出摘掉etcd
3、鉴权问题
4、部署问题
5、数据备份问题
6、申请库管理平台
7、网页工具，类似es的kibana网页小工具


SimSearch


- build
    - 编译gflag tag:v2.2.2
        mkdir build && cd build && cmake .. -DBUILD_SHARED_LIBS=ON && make -j && make install
    - 编译glog， tag:v0.3.5, 
        mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DWITH_GFLAGS=ON && make -j && sudo make install
    - 编译protobufer, tag:v3.20.3
    - 编译brpc
        export BRPC_HOME=/usr/home/guoyande/guoyande/lib/brpc
        cmake -B build -DCMAKE_INSTALL_PREFIX=$BRPC_HOME -DBRPC_WITH_GLOG=ON && && cmake --build build -j && make install
    - 编译braft
        export BRAFT_HOME=/usr/home/guoyande/guoyande/lib/braft
        mkdir bld && cd bld && cmake .. -DCMAKE_INSTALL_PREFIX=$BRAFT_HOME -DBRPC_WITH_GLOG=ON && make -j && make install
    - 编译router和ps
        mkdir build && cd build && cmake .. && make -j
    - 编译master
        cd master && go build main.go