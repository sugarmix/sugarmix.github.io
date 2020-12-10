#!/bin/bash
read -p "输入同时启动的客户端个数:" client_num
for((i=1;i<=$client_num;i++));  
do   
    echo 正在打开第$i个客户端
    `./rpc_client $i > /dev/null 2>&1 &`;  
done
wait
