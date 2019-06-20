# 开发手册

## 目录

<!-- TOC -->

- [测试网](#测试网)
    - [A链环境](#a链环境)
        - [区块信息](#区块信息)
        - [通讯合约地址](#通讯合约地址)
        - [资产合约地址](#资产合约地址)
    - [B链环境](#b链环境)
        - [区块信息](#区块信息)
        - [通讯合约地址](#通讯合约地址)
        - [资产合约地址](#资产合约地址)
- [体验跨链](#体验跨链)
    - [A链发行资产](#a链发行资产)
    - [A链转移资产至通讯合约](#a链转移资产至通讯合约)
    - [A链触发通讯合约](#a链触发通讯合约)
    - [查看B链通讯合约状态](#查看b链通讯合约状态)
    - [查看B链资产合约状态](#查看b链资产合约状态)
    - [查看A链最终状态](#查看a链最终状态)

- [开发说明](#开发说明)
    - [公证人程序](#公证人程序)
    - [通讯合约](#通讯合约)
    - [资产合约](#资产合约)
    - [部署说明](#部署说明)
        - [部署链](#部署链)
        - [部署公证人](#部署公证人)
        - [部署合约](#部署合约)
        - [部署链](#部署链)

<!-- /TOC -->

单条区块链的基础设施正在蓬勃发展，但各条链之间却是价值孤岛，我们致力于创建一种轻量、安全的跨链资产转移协议，让资产自由流通起来。

目前我们已经设计并实现了跨链资产转移协议，并在两条BUChain测试网上线，现邀请各位开发者体验和使用，并提出宝贵意见。

## 测试网
目前我们搭建了两条链，A链和B链，他们内部分别有对应的资产合约和通讯合约。通讯合约负责两条链的消息交互，保证交互的原子性，有效性；资产合约是实际发生资产转移的对应业务形态的实现，用户可以自定义任何方式的业务，如资产1:1转移，资产1:N兑换等。


| | A链环境 | B链环境 
|:--- | ---  | --- 
| 区块信息 | http://52.80.81.176:10002/getLedger | http://52.80.81.176:20002/getLedger
|通讯合约地址| [a00168ba...](http://52.80.81.176:10002/getAccount?address=a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60 "a00168ba...")  |[a0010cc417...](http://52.80.81.176:20002/getAccount?address=a0010cc417e4dfa7a952347980842d2d37f99a3ae190b0 "a0010cc417...") 
| 资产合约地址 | [a0023006...](http://52.80.81.176:10002/getAccount?address=a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e "a0023006...")  | [a0021217...](http://52.80.81.176:20002/getAccount?address=a002121795274745cfa2d56577b15781c5fb5627458bc2 "a0021217...") 



## 体验跨链
使用我们搭建的测试链，可以快速体验跨链资产转移，下面我们看如何进行操作，为了编译开发和调试，我们使用Postman进行发送消息，里面的私钥都是明文形式，在实际使用过程中，不可用Postman发交易，以防私钥丢失。
### A链发行资产
跨链第一步需要在A链发行资产。我们在A链的资产合约`a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e`里发行10000000数量的资产。
```
{
  "items": [
    {
      "transaction_json": {
        "source_address": "a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec", 
        "nonce": 1, 
        "operations": [
          {
            "type": 3, 
            "payment": {
              "dest_address": "a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e", 
              "input": "{\"action\":\"issue\",\"from\":\"a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec\",\"to\":\"a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec\",\"amount\":10000000}"
            }
          }
        ]
      }, 
      "private_keys": [
        "c001ab8accaa32d24e36cbab6fbc2abce67f59c65f3dcff87c7ec58fb7b8bc710ad4b8"
      ]
    }
  ]
}	

可以通过 http://52.80.81.176:10002/getAccount?address=a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e 接口查看交易资产的结果。

```

例如：
```
{
    key: "tx_1",
    value: "{"
    from ":"
    a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec ","
    to ":"
    a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec ","
    amount ":10000000,"
    seq ":1,"
    actionTime ":1561011118108}",
    version: 1
},
```

### A链转移资产至通讯合约
跨链第二步需要在先给通讯合约转移资产，让合约`a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e`锁定。
```
{
  "items": [
    {
      "transaction_json": {
        "source_address": "a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec", 
        "nonce": 2, 
        "operations": [
          {
            "type": 3, 
            "payment": {
              "dest_address": "a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e", 
              "input": "{\"action\":\"transfer\",\"from\":\"a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec\",\"to\":\"a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60\",\"amount\":50}"
            }
          }
        ]
      }, 
      "private_keys": [
        "c001ab8accaa32d24e36cbab6fbc2abce67f59c65f3dcff87c7ec58fb7b8bc710ad4b8"
      ]
    }
  ]
}

```

可以通过 http://52.80.81.176:10002/getAccount?address=a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e 接口查看交易资产的结果。

例如：
```
{
    key: "tx_2",
    value: "{"
    from ":"
    a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec ","
    to ":"
    a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60 ","
    amount ":50,"
    seq ":2,"
    actionTime ":1561011198494}",
    version: 1
}
```

### A链触发通讯合约
跨链第三步，需要通知通讯合约`a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e`，形成提案。
```
{
  "items": [
    {
      "transaction_json": {
        "source_address": "a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec", 
        "nonce": 6, 
        "operations": [
          {
            "type": 3, 
            "payment": {
              "dest_address": "a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60", 
              "input": "{\"function\":\"sendCrossChain\",\"f_assets_addr\":\"a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e\",\"from\":\"a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec\",\"to\":\"a002b8467e03f6771c7e9dda09f9a7027b33fdf9a62386\",\"amount\":50,\"t_assets_addr\":\"a002121795274745cfa2d56577b15781c5fb5627458bc2\",\"seq\":2}"
            }
          }
        ]
      }, 
      "private_keys": [
        "c001ab8accaa32d24e36cbab6fbc2abce67f59c65f3dcff87c7ec58fb7b8bc710ad4b8"
      ]
    }
  ]
}	

```
可以通过 http://52.80.81.176:10002/getAccount?address=a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60 接口查看交易资产的结果。

例如：

```
{
    key: "send_proposal_1",
    value: "{"
    proposal ":{"
    seq ":1,"
    amount ":50,"
    from ":"
    a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec ","
    to ":"
    a002b8467e03f6771c7e9dda09f9a7027b33fdf9a62386 ","
    f_assets_addr ":"
    a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e ","
    t_assets_addr ":"
    a002121795274745cfa2d56577b15781c5fb5627458bc2 ","
    f_comm_addr ":"
    a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60 ","
    t_comm_addr ":"
    a0010cc417e4dfa7a952347980842d2d37f99a3ae190b0 ","
    f_chain_id ":"
    CHAIN_20190528_A ","
    t_chain_id ":"
    CHAIN_20190528_B "},"
    state ":1," //提案状态。1为初始，2为处理中，3为失败，4为成功。
    vote ""  //提案反馈记录
}
```

### 查看B链通讯合约状态
交易完成后，在B链通讯合约查看状态 http://52.80.81.176:20002/getAccount?address=a0010cc417e4dfa7a952347980842d2d37f99a3ae190b0 

结果如下：
```
{
    key: "receive_proposal_1",
    value: "{"
    proposals ":[["
    a001e5c2e5142c2b57e72dd5783b383eabfdf4f4e4f6bb ",1,50,"
    a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec ","
    a002b8467e03f6771c7e9dda09f9a7027b33fdf9a62386 ","
    a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e ","
    a002121795274745cfa2d56577b15781c5fb5627458bc2 ","
    a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60 ","
    a0010cc417e4dfa7a952347980842d2d37f99a3ae190b0 ","
    CHAIN_20190528_A ","
    CHAIN_20190528_B "],["
    a001ce7acd9c7cb6b9bd6acaeecaaa0d7ff240241a9d15 ",1,50,"
    a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec ","
    a002b8467e03f6771c7e9dda09f9a7027b33fdf9a62386 ","
    a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e ","
    a002121795274745cfa2d56577b15781c5fb5627458bc2 ","
    a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60 ","
    a0010cc417e4dfa7a952347980842d2d37f99a3ae190b0 ","
    CHAIN_20190528_A ","
    CHAIN_20190528_B "],["
    a001ce7acd9c7cb6b9bd6acaeecaaa0d7ff240241a9d15 ",1,50,"
    a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec ","
    a002b8467e03f6771c7e9dda09f9a7027b33fdf9a62386 ","
    a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e ","
    a002121795274745cfa2d56577b15781c5fb5627458bc2 ","
    a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60 ","
    a0010cc417e4dfa7a952347980842d2d37f99a3ae190b0 ","
    CHAIN_20190528_A ","
    CHAIN_20190528_B "]],"
    state ":4,"
    vote_count ":3}",
    version: 2
}
```

### 查看B链资产合约状态

同时，在B链资产合约查看跨链的新资产 http://52.80.81.176:20002/getAccount?address=a002121795274745cfa2d56577b15781c5fb5627458bc2

结果如下：

```
{
    key: "tx_1",
    value: "{"
    from ":"
    a0010cc417e4dfa7a952347980842d2d37f99a3ae190b0 ","
    to ":"
    a002b8467e03f6771c7e9dda09f9a7027b33fdf9a62386 ","
    amount ":50,"
    seq ":1,"
    actionTime ":1561012460941,"
    cpcSeq ":1}",
    version: 1
}
```

### 查看A链最终状态
在A链里能看到本次资产的通讯提案，处于完成状态。
可以通过 http://52.80.81.176:10002/getAccount?address=a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60 接口查看。

结果如下：
```
{
    key: "send_proposal_1",
    value: "{"
    proposal ":{"
    seq ":1,"
    amount ":50,"
    from ":"
    a00138e97b736f16d1342d67fe5cbcceb1638ffc52d6ec ","
    to ":"
    a002b8467e03f6771c7e9dda09f9a7027b33fdf9a62386 ","
    f_assets_addr ":"
    a00230068c4eab8c26dd1cd140390fd09f9ffa9706845e ","
    t_assets_addr ":"
    a002121795274745cfa2d56577b15781c5fb5627458bc2 ","
    f_comm_addr ":"
    a00168babf35f0feac4854bb1fcc79d0235edfa87d0b60 ","
    t_comm_addr ":"
    a0010cc417e4dfa7a952347980842d2d37f99a3ae190b0 ","
    f_chain_id ":"
    CHAIN_20190528_A ","
    t_chain_id ":"
    CHAIN_20190528_B "},"
    state ":4," //反馈成功，标识为4
    vote ":[["
    a001e5c2e5142c2b57e72dd5783b383eabfdf4f4e4f6bb ",4]],"
    vote_count ":1}",
    version: 2
}
```

## 开发说明
公证人机制的实现分为两部分，一是公证人程序，二是通讯合约和资产合约。

### 公证人程序
公证人程序主要跟A链和B链的合约打交道，并根据协议规则，实现交互流程。使用C++实现，[代码目录](https://github.com/zjg555543/bubichain-v3/tree/feature/crosschain/src/notary "代码目录")

### 通讯合约
https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_comm.js

### 资产合约
https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_asset.js

## 部署说明
A链和B链的部署参考 buchain的部署。[部署文档](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/manual.md "部署文档")，需要注意的是要配置与公证人程序的配置文件，[配置文件](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/build/win32/config/bubi.json "配置文件")
### 部署链
```
"cross": {
    "comm_unique": "CHAIN_20190528_A",  //链的ID，与对应通訊合约里的ID保持一致
    "comm_contract": "a0010cc417e4dfa7a952347980842d2d37f99a3ae190b0", //通讯合约地址
    "target_addr": "127.0.0.1:30000", //公证人程序的监听端口
    "enabled": true
}
```
### 部署公证人
编译完成后，运行`notary`程序即可，需要注意的是其[配置文件](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/build/win32/config/notary.json "配置文件")

```
{
    "logger": {
        "path": "log/notary.log",
        "dest": "FILE|STDOUT|STDERR",
        "level": "TRACE|INFO|WARNING|ERROR|FATAL",
        "time_capacity": 1,
        "size_capacity": 10,
        "expire_days": 10
    },
    "notary": {
        "notary_address": "a001c3bbfce78bba8bfcb37113a84e95a3fd441a5622e3",
        "private_key": "c0018d7939ec4085db3db6a2c698a6b30345301d81b6b53a700f4865a8c51f479149ee",
        "listen_addr": "127.0.0.1:30000"//公证人监听端口
    },
    "pair_chain_1": {
        "comm_unique": "CHAIN_20190528_A",//A链通讯合约ID
        "comm_contract": "a00200c3a5b881e0c729ae167fa43cf7778eb18eda3754"//A链通讯合约地址
    },
    "pair_chain_2": {
        "comm_unique": "CHAIN_20190528_B",//B链通讯合约ID
        "comm_contract": "a00136bc18029f74548c1a1b9ae8a2449a43d93ce19184"//B链通讯合约地址
    }
}
```

### 部署合约
- [创建A链通讯合约](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_step/0-CreateAChainCom.txt "创建A链通讯合约")
- [创建A链资产合约](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_step/0-CreateAChainAsset.txt "创建A链资产合约")
- [创建B链通讯合约](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_step/0-CreateBChainComm.txt "创建B链通讯合约")
- [创建B链资产合约](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_step/0-CreateBChainAsset.txt "创建B链资产合约")
- [初始化A链通讯合约](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_step/1-InitAChainComm.txt "初始化A链通讯合约")
- [初始化B链通讯合约](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_step/1-InitBChainComm.txt "初始化B链通讯合约")
- [在A链发行资产](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_step/2-CrossIssueAChainAsset.txt "在A链发行资产")
- [转移A链资产给通讯合约](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_step/2-CrossTransAChainAsset.txt "转移A链资产给通讯合约")
- [触发通讯合约](https://github.com/zjg555543/bubichain-v3/blob/feature/crosschain/docs/cross_chain/cross_step/2-CrossTransAChainComm.txt "触发通讯合约")
