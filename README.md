
rocksdb  支持mvcc


TODO:

1、cpp yaml解析；                                                    --完成
2、snapshot恢复扩副本问题；                                            --完成
3、master并发建表，删表问题，                                           --完成
4、master创建表分布式锁, 当有create/delete space失败时，不可进行space操作  --完成
5、多表测试, 增加表id，方式同名表aba问题；                                --完成
6、单副本写入失败问题；
7、节点清除扩副本问题；
8、进程退出保存etcd，
9、监控问题、健康借口
10、ps有space启动注册master失败后还可以启动
11、ps注册master ip/port重复检测，防止一个机器中起了多个相同的端口


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




分布式表管理问题：
1、删表的时候有ps节点挂了，一段时间又上线了；          解决方案：增加一个记录删表失败的list, ps上线时如果有表，比较表是否在删除的清单中；etcd中需要增加一个表id字段；
2、ps节点迁移时，表启动时带上迁移命令，然后节点启动时先check, 本地节点信息是否和etcd中能对得上，如果对得上，ps请求master,执行迁移命令；

3、space迁移方案，避免了aba问题
- 同集群
1）master发起数据迁移zhi ling：
    a) create new_ps_space；
    b) 让old_ps 向 new_ps 同步数据；
    c) router space下增加new_ps connection
2) master发起切流量指令；
    router write/get时check old_ps是否完成数据同步，没完成阻塞等等，完成则后续流量全部转入new_ps
3) master发起删除old_ps, router删除old_ps连接；
- 跨集群
1）新集群创建space；
2) 旧集群ps 将数据同步到新集群 ps；
3) 让业务方切换 地址；

4、迁移master
1）将老集权etcd数据dump下来；
2) 将数据写入到新集权etcd;
3) 对ps修改 配置
4）对router修改 配置


