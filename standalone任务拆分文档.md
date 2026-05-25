# Standalone分支任务拆分文档

## 一、项目概述

### 1.1 项目背景
UBS-IO（UBS-IO BoostIO）是一个基于计算侧高性能分布式读写缓存的系统，旨在解决存算分离架构下的性能瓶颈问题。当前分支（codex/standalone）的功能是实现**应用程序调用API接口**，让应用程序能够通过统一的SDK接口访问缓存服务。

### 1.2 核心架构
系统采用三层架构设计：
- **客户端SDK层**：`bio.h` 定义的API接口
- **Agent接口层**：`bio_client_agent` 负责客户端与服务端的通信
- **服务端实现层**：包含Cache、Flow、Net、CM等核心模块

### 1.3 关键技术点
- 使用动态链接库（dlopen）实现客户端与服务端的解耦
- 支持standalone和convergence两种部署模式
- 通过函数指针实现跨进程调用
- 使用共享内存进行数据交互

---

## 二、模块接口设计

### 2.1 接口定义文件

| 接口文件 | 职责 | 关键类型 |
|---------|------|---------|
| `bio.h` | 客户端SDK公共接口 | `Bio`, `BioService` |
| `bio_client_agent.h` | Agent层函数指针定义 | `BioClientAgent` |
| `message.h` | 通信消息结构定义 | `RequestComm`, 各类Request/Response |
| `bio_c.h` | C语言接口定义 | C风格API |

### 2.2 核心接口列表

#### 2.2.1 BioService接口（服务端初始化）
```cpp
class BioService {
    static CResult Initialize(WorkerMode mode, const ClientOptionsConfig &optConf);
    static void Exit();
    static CResult BioShowCacheResource(std::vector<CacheResourcesDesc> &nodeDesc);
    static CResult BioShowCacheHitRatio(std::unordered_map<uint16_t, CacheHitDesc> &nodeDesc);
    static std::shared_ptr<Bio> CreateCache(const CacheDescriptor &desc);
    static CacheDescriptor GetCache(uint64_t tenantId);
    static std::vector<CacheDescriptor> ListCache();
    static void DestroyCache(uint64_t tenantId);
};
```

#### 2.2.2 Bio缓存实例接口
```cpp
class Bio {
    CResult CalculateLocation(uint64_t objectId, ObjLocation &location);
    CResult Put(const char *key, const char *value, uint64_t length, const ObjLocation &location);
    CResult Get(const char *key, uint64_t offset, uint64_t length, const ObjLocation &location, char *value, uint64_t &realLength);
    CResult Delete(const char *key, const ObjLocation &location);
    CResult Load(LoadPara &para, const ObjLocation location, const BioLoadCallback callback, void *context);
    CResult ListAll(const char *prefix, std::unordered_map<std::string, ObjStat> &objs);
    CResult Stat(const char *key, const ObjLocation &location, ObjStat &stat);
    CResult NotifyUpdateFinish();
    CResult NotifyUpdatePrepare();
    CResult CheckUpdateReady();
    CResult AllocSpace(uint64_t objectId, uint64_t length, CacheSpaceDesc &spaceInfo);
    CResult Put(const char *key, CacheSpaceDesc &spaceInfo);
};
```

#### 2.2.3 Agent层函数指针接口
```cpp
class BioClientAgent {
    // 初始化与退出
    BioServerStartFuncPtr startOp;           // 融合部署启动
    BioServerStartFuncPtr standaloneStartOp; // 独立部署启动
    BioServerExitFuncPtr exitOp;             // 服务退出

    // 配置获取
    GetRuntimeConfigFuncPtr getRuntimeConfigOp;
    GetBioServerCrcFlagFuncPtr getCrcFlag;
    GetBioServerCliFlagFuncPtr getCliFlag;
    GetBioServerPromethuesToggleFuncPtr getPrometheusToggle;
    GetBioServerListenAddressFuncPtr getListenAddress;
    GetBioServertimeOutFuncPtr getTimeOut;
    GetBioServerScrapeIntervalSecFuncPtr getScrapeIntervalSec;
    GetBioServerNetEngineFuncPtr getNetEngineOp;

    // 节点信息
    GetLocalNidFuncPtr getLocalNidOp;

    // 配额管理
    GetQuotaInfoFuncPtr getQuotaInfoOp;
    AllocQuotaFuncPtr allocQuotaOp;
    FreeQuotaFuncPtr freeQuotaOp;

    // 视图查询
    GetNodeViewFuncPtr getNodeViewOp;
    GetPtViewFuncPtr getPtViewOp;

    // 流管理
    CreateFlowMasterFuncPtr createFlowMasterOp;
    CreateFlowSlaveFuncPtr createFlowSlaveOp;
    DestroyFlowFuncPtr destroyFlowOp;

    // 升级相关
    NotifyUpdateFuncPtr notifyUpdateOp;
    CheckUpdateReadyFuncPtr checkUpdateReadyOp;

    // 数据操作
    GetSliceFuncPtr getSliceOp;
    PutFuncPtr putOp;
    GetFuncPtr getOp;
    DeleteFuncPtr deleteOp;
    AddDiskFuncPtr addDiskOp;
    StatFuncPtr statOp;
    ListFuncPtr listOp;
    LoadFuncPtr loadOp;

    // 监控
    GetCacheHitLocalFuncPtr cacheHitOp;
    CalcCacheResourceLocalFuncPtr cacheResourceOp;
    GetTracePointsLocalFuncPtr getTracePointsOp;
};
```

---

## 三、任务拆分方案

### 任务分组说明

根据用户需求，将工作拆分为**三个主要模块**，工作量大致分布为：
- **模块1（Agent接口层）**：约35%工作量
- **模块2（服务端实现层）**：约40%工作量
- **模块3（客户端SDK层）**：约25%工作量

---

### 模块1：Agent接口层（程序员1）

#### 1.1 职责概述
负责客户端Agent的初始化、函数指针加载、跨进程调用逻辑。这是客户端与服务端的桥梁，需要理解dlopen机制和函数指针调用。

#### 1.2 详细工作内容

##### 1.2.1 Agent初始化与退出
**文件**：`bio_client_agent.cpp`

**工作项**：
- [ ] 实现 `Initialize(WorkerMode mode)` 函数
  - 根据模式（CONVERGENCE/STANDALONE）加载不同的so文件
  - 路径：`/usr/lib64/libbio_server.so`
  - 处理dlopen失败的情况
- [ ] 实现 `Exit()` 函数
  - 调用服务端的退出函数
  - 清理资源

##### 1.2.2 函数指针加载
**文件**：`bio_client_agent.cpp` - `InitOperation()`, `InitUpgradeOperation()`

**工作项**：
- [ ] 实现基础函数指针加载（30个函数指针）
  - 启动/退出相关
  - 配置获取相关
  - 配额管理相关
  - 视图查询相关
  - 流管理相关
  - 数据操作相关
  - 监控相关
- [ ] 实现升级相关函数指针加载（2个）
- [ ] 实现错误处理和日志记录

##### 1.2.3 客户端网络通信
**文件**：`bio_client_net.cpp`, `bio_client_net.h`

**工作项**：
- [ ] 实现网络连接管理
- [ ] 实现请求发送和响应接收
- [ ] 实现超时处理
- [ ] 实现重连机制

##### 1.2.4 客户端SDK接口调用
**文件**：`bio_client.cpp`

**工作项**：
- [ ] 实现 `BioService::Initialize()` 的客户端部分
- [ ] 实现 `BioService::CreateCache()` 的客户端调用
- [ ] 实现 `BioService::DestroyCache()` 的客户端调用
- [ ] 实现 `BioService::BioShowCacheResource()` 的客户端调用
- [ ] 实现 `BioService::BioShowCacheHitRatio()` 的客户端调用

##### 1.2.5 缓存实例操作封装
**文件**：`bio_client.cpp`

**工作项**：
- [ ] 实现 `Bio::CalculateLocation()` 的客户端调用
- [ ] 实现 `Bio::Put()` 的客户端调用
- [ ] 实现 `Bio::Get()` 的客户端调用
- [ ] 实现 `Bio::Delete()` 的客户端调用
- [ ] 实现 `Bio::Load()` 的异步调用
- [ ] 实现 `Bio::ListAll()` 的客户端调用
- [ ] 实现 `Bio::Stat()` 的客户端调用
- [ ] 实现配额管理相关调用

#### 1.3 关键接口定义

```cpp
// bio_client_agent.h
namespace ock {
namespace bio {
namespace agent {
class BioClientAgent {
public:
    BResult Initialize(WorkerMode mode);
    void Exit();
    BResult GetLocalNodeInfo(uint16_t &protocol, CmNodeId &localNid);
    BResult GetRuntimeConfig(ShmInitResponse &rsp);
    BResult GetLocalQuotaInfo(uint32_t scene, bool &enable, uint64_t &preloadSize);
    BResult AllocQuota(AllocQuotaRequest &req, uint64_t &expectPreloadSize);
    BResult FreeQuota(FreeQuotaRequest &req);
    BResult GetClusterNodeView(uint64_t &curNodeTimes, std::map<CmNodeId, CmNodeInfo, CmNodeIdCmp> &nodeView);
    BResult GetPtView(uint64_t &curPtTimes, std::map<uint16_t, CmPtInfo> &ptView);
    BResult CreateFlowLocal(pid_t procId, CmPtInfo &ptEntry, FlowInfo &flowInfo);
    BResult DestroyFlowLocal(pid_t procId, CmPtInfo &ptEntry, uint16_t ptId, uint64_t flowId);
    BResult PrepareResource(CmPtInfo &ptEntry, uint64_t flowId, uint64_t offset, uint64_t index, uint64_t length,
        GetSliceResponse **rsp);
    void PutLocal(PutRequest *req, Callback &callback);
    BResult GetLocal(GetRequest &req, char *value, uint64_t &realLen);
    void DeleteLocal(DeleteRequest &req, Callback &callback);
    BResult AddDisk(AddDiskRequest &req, AddDiskResponse &rsp);
    BResult ListLocal(ListRequest &req, std::unordered_map<std::string, ObjStat> &objs);
    BResult StatLocal(StatRequest &req, ObjStat &objInfo);
    BResult NotifyUpdate(bool &flag);
    BResult CheckUpdateReady();
    BResult LoadLocal(LoadRequest &req);
    BResult GetCacheHitLocal(CacheHitRequest &req, std::unordered_map<uint16_t, CacheHitDesc> &nodeDesc);
    BResult CalcCacheResourceLocal(CacheResourceRequest &req, std::vector<CacheResourcesDesc> &nodeDesc);
    BResult GetTracePointsLocal(GetTracePointsRequest &req, GetTracePointsResponse &rsp);
};
}
}
}
```

#### 1.4 依赖关系
- 依赖服务端提供的函数导出
- 依赖 `message.h` 中的消息结构定义
- 依赖 `net_engine.h` 的网络通信接口

#### 1.5 交付物
- `bio_client_agent.cpp/h` - Agent核心实现
- `bio_client_net.cpp/h` - 网络通信实现
- `bio_client.cpp/h` - SDK接口封装

---

### 模块2：服务端实现层（程序员2）

#### 2.1 职责概述
负责服务端的核心模块实现，包括缓存管理、流管理、网络通信、集群管理等。这是系统的主体功能实现，工作量最大。

#### 2.2 详细工作内容

##### 2.2.1 服务端初始化流程
**文件**：`bio_server.cpp`, `bio_server.h`

**工作项**：
- [ ] 实现 `BioServer::Start(bool standaloneMode)` 主入口
  - 日志初始化
  - 配置初始化
  - 模块构建和初始化
  - 服务启动
- [ ] 实现模块注册机制（`BuildModules`）
  - 根据standalone模式选择不同模块
- [ ] 实现服务退出流程（`Exit`）
- [ ] 实现错误回滚机制

##### 2.2.2 配置文件解析
**文件**：`bio_config_instance.cpp/h`, `bio_config.h`, `bio_config_validator.h`

**工作项**：
- [ ] 实现配置加载（`Initialize`）
  - 路径：`/etc/boostio/`
- [ ] 实现各类配置项解析
  - DaemonConfig - 守护进程配置
  - NetConfig - 网络配置
  - CacheConfig - 缓存配置
  - ClusterConfig - 集群配置
  - UnderFsConfig - 底层文件系统配置
- [ ] 实现配置验证器
- [ ] 实现配置热更新机制

##### 2.2.3 缓存模块实现
**文件**：
- `cache/cache.h` - 缓存主模块
- `cache/cache_slice.h` - 缓存片管理
- `cache/cache_slice_operator.cpp/h` - 缓存片操作
- `cache/read/rcache*.cpp/h` - 读缓存实现
- `cache/write/wcache*.cpp/h` - 写缓存实现
- `cache/overloadctrl/cache_overload_ctrl.cpp/h` - 负载控制

**工作项**：
- [ ] 实现缓存初始化和退出
- [ ] 实现读缓存管理（RCache）
  - 缓存淘汰策略（LRU、LFU等）
  - 缓存预取
  - 缓存统计
- [ ] 实现写缓存管理（WCache）
  - 多层缓存（内存+磁盘）
  - 写回策略
  - 数据持久化
- [ ] 实现缓存片管理
- [ ] 实现负载控制
- [ ] 实现缓存统计接口

##### 2.2.4 流管理模块实现
**文件**：`flow/flow*.cpp/h`

**工作项**：
- [ ] 实现流创建（CreateFlow）
- [ ] 实现流销毁（DestroyFlow）
- [ ] 实现流调度
- [ ] 实现流状态管理
- [ ] 实现流ID分配

##### 2.2.5 网络模块实现
**文件**：`net/net_engine.cpp/h`, `net/net_connector.cpp/h`, `net/net_channel_mgr.cpp/h`

**工作项**：
- [ ] 实现网络引擎初始化
- [ ] 实现RDMA支持（可选）
- [ ] 实现连接管理
- [ ] 实现通道管理
- [ ] 实现数据传输

##### 2.2.6 集群管理模块（CM）
**文件**：
- `cluster/cm.cpp/h` - 主模块
- `cluster/client/*.c/h` - 客户端实现
- `cluster/server/*.c/h` - 服务端实现
- `cluster/common/*.c/h` - 公共组件
- `cluster/common/cm_zkadapter.c/h` - ZooKeeper适配器

**工作项**：
- [ ] 实现节点管理
- [ ] 实现主从选举
- [ ] 实现配置同步
- [ ] 实现故障检测
- [ ] 实现ZooKeeper集成

##### 2.2.7 Mirror服务器实现
**文件**：`server/mirror_server.cpp/h`, `server/mirror_server_crb.cpp/h`

**工作项**：
- [ ] 实现MirrorServer初始化
- [ ] 实现CRB（Callback Request Base）处理
- [ ] 实现数据同步
- [ ] 实现镜像操作

##### 2.2.8 底层存储模块
**文件**：`underfs/*.cpp/h`, `disk/common/bdm*.c/h`

**工作项**：
- [ ] 实现本地文件系统支持
- [ ] 实现HDFS支持
- [ ] 实现Ceph支持
- [ ] 实现块设备管理

##### 2.2.9 Standalone模式特殊实现
**文件**：`server/standalone_view.cpp/h`

**工作项**：
- [ ] 实现单机视图构建
- [ ] 实现单机模式下的节点模拟
- [ ] 实现单机模式下的流管理

#### 2.3 关键接口定义

```cpp
// bio_server.h
class BioServer {
public:
    BResult Start(bool standaloneMode = false);
    void Exit();
    static BioServerPtr &Instance();

    // 模块管理
    std::vector<ModuleDesc> BuildModules(bool standaloneMode);
    BResult BioConfigInit();
    BResult BioLoggerInit(std::string pathName);
    BResult BioTraceInit();
    BResult BioUnderFsInit();
    BResult BioBdmInit();
    BResult BioNetInit();
    BResult BioCmInit();
    BResult BioStandaloneViewInit();
    BResult BioMirrorServerInit();
    BResult BioCacheInit();
    BResult BioFlowInit();

    // 配置获取
    BioConfigPtr GetConfig();
    CmNodeId GetLocalNid();
    bool GetCrbProcessing();
    CmPtr GetCm();
    NetEnginePtr GetNetEngine();
    MirrorServerPtr GetMirrorServer();

    // 视图管理
    std::map<CmNodeId, CmNodeInfo, CmNodeIdCmp> GetNodeView(uint64_t *curNodeTimes);
    std::map<uint16_t, CmPtInfo> GetPtView(uint64_t *curPtTimes);
    CmPtInfo GetPtEntry(uint16_t ptId);

    // 服务状态
    bool GetServiceState();
    bool IsStandaloneMode() const;
    BResult ReportServiceState(bool isUpgrade);
};
```

#### 2.4 依赖关系
- 依赖配置文件解析
- 依赖底层存储
- 依赖网络库
- 依赖ZooKeeper客户端库

#### 2.5 交付物
- `bio_server.cpp/h` - 服务端主入口
- `bio_config_instance.cpp/h` - 配置管理
- `cache/*.cpp/h` - 缓存模块完整实现
- `flow/*.cpp/h` - 流管理模块
- `net/*.cpp/h` - 网络模块
- `cluster/*.c/h` - 集群管理模块
- `mirror_server.cpp/h` - Mirror服务器
- `standalone_view.cpp/h` - Standalone模式支持

---

### 模块3：客户端SDK层（程序员3）

#### 3.1 职责概述
负责定义和实现面向应用程序的SDK接口，封装底层细节，提供简洁易用的API。同时负责C语言接口和示例代码。

#### 3.2 详细工作内容

##### 3.2.1 公共API定义
**文件**：`sdk/bio.h`

**工作项**：
- [ ] 定义 `Bio` 类 - 缓存实例操作
  - 构造函数参数：`id`, `affinity`, `strategy`
  - 公共方法：Put, Get, Delete, Load, ListAll, Stat等
- [ ] 定义 `BioService` 类 - 服务管理
  - 静态方法：Initialize, Exit, CreateCache, DestroyCache等
- [ ] 定义数据结构
  - `ObjLocation` - 对象位置信息
  - `CacheDescriptor` - 缓存描述符
  - `LoadPara` - 加载参数
  - `CacheSpaceDesc` - 空间描述符

##### 3.2.2 C语言接口定义
**文件**：`sdk/bio_c.h`

**工作项**：
- [ ] 定义C风格API函数
  - `BioC_Init`
  - `BioC_Exit`
  - `BioC_CreateCache`
  - `BioC_DestroyCache`
  - `BioC_Put`
  - `BioC_Get`
  - `BioC_Delete`
  - `BioC_ListAll`
  - `BioC_Stat`
- [ ] 定义C风格数据结构
- [ ] 添加extern "C"声明

##### 3.2.3 SDK实现
**文件**：`sdk/bio.cpp`

**工作项**：
- [ ] 实现 `BioService::Initialize()`
  - 调用Agent初始化
  - 配置验证
- [ ] 实现 `BioService::Exit()`
- [ ] 实现 `BioService::CreateCache()`
  - 参数验证
  - 创建缓存描述符
  - 注册到管理器
- [ ] 实现 `BioService::DestroyCache()`
- [ ] 实现 `BioService::BioShowCacheResource()`
- [ ] 实现 `BioService::BioShowCacheHitRatio()`

##### 3.2.4 Bio实例方法实现
**文件**：`sdk/bio.cpp`

**工作项**：
- [ ] 实现 `Bio::CalculateLocation()`
- [ ] 实现 `Bio::Put()` - 两版本
- [ ] 实现 `Bio::Get()`
- [ ] 实现 `Bio::Delete()`
- [ ] 实现 `Bio::Load()`
- [ ] 实现 `Bio::ListAll()`
- [ ] 实现 `Bio::Stat()`
- [ ] 实现 `Bio::NotifyUpdateFinish()`
- [ ] 实现 `Bio::NotifyUpdatePrepare()`
- [ ] 实现 `Bio::CheckUpdateReady()`
- [ ] 实现 `Bio::AllocSpace()`

##### 3.2.5 QoS管理
**文件**：`sdk/bio_qos.cpp/h`

**工作项**：
- [ ] 实现QoS策略定义
- [ ] 实现带宽控制
- [ ] 实现优先级调度
- [ ] 实现配额管理

##### 3.2.6 镜像客户端
**文件**：`sdk/mirror_client.cpp/h`

**工作项**：
- [ ] 实现镜像操作接口
- [ ] 实现镜像同步
- [ ] 实现故障切换

##### 3.2.7 示例代码和文档
**文件**：`test/benchmarks/console.cpp`

**工作项**：
- [ ] 编写基础使用示例
- [ ] 编写高级功能示例
- [ ] 编写性能测试代码

#### 3.3 关键接口定义

```cpp
// sdk/bio.h
namespace ock {
namespace bio {

class Bio {
public:
    using LoadCallback = std::function<void(void *context, int32_t result)>;

    CResult CalculateLocation(uint64_t objectId, ObjLocation &location);
    CResult Put(const char *key, const char *value, uint64_t length, const ObjLocation &location);
    CResult Get(const char *key, uint64_t offset, uint64_t length, const ObjLocation &location,
                char *value, uint64_t &realLength);
    CResult Delete(const char *key, const ObjLocation &location);
    CResult Load(LoadPara &para, const ObjLocation location, const BioLoadCallback callback, void *context);
    CResult ListAll(const char *prefix, std::unordered_map<std::string, ObjStat> &objs);
    CResult Stat(const char *key, const ObjLocation &location, ObjStat &stat);
    CResult NotifyUpdateFinish();
    CResult NotifyUpdatePrepare();
    CResult CheckUpdateReady();
    CResult AllocSpace(uint64_t objectId, uint64_t length, CacheSpaceDesc &spaceInfo);
    CResult Put(const char *key, CacheSpaceDesc &spaceInfo);

    Bio(uint64_t id, AffinityStrategy affinity, WriteStrategy strategy);
};

class BioService {
public:
    static CResult Initialize(WorkerMode mode, const ClientOptionsConfig &optConf);
    static void Exit();
    static CResult BioShowCacheResource(std::vector<CacheResourcesDesc> &nodeDesc);
    static CResult BioShowCacheHitRatio(std::unordered_map<uint16_t, CacheHitDesc> &nodeDesc);
    static std::shared_ptr<Bio> CreateCache(const CacheDescriptor &desc);
    static CacheDescriptor GetCache(uint64_t tenantId);
    static std::vector<CacheDescriptor> ListCache();
    static void DestroyCache(uint64_t tenantId);
};

}
}
```

```c
// sdk/bio_c.h
#ifdef __cplusplus
extern "C" {
#endif

CResult BioC_Init(int mode, const ClientOptionsConfigC *optConf);
void BioC_Exit();
CResult BioC_CreateCache(uint64_t tenantId, AffinityStrategy affinity, WriteStrategy strategy);
void BioC_DestroyCache(uint64_t tenantId);
CResult BioC_Put(uint64_t tenantId, const char *key, const char *value, uint64_t length);
CResult BioC_Get(uint64_t tenantId, const char *key, char *value, uint64_t *length);
CResult BioC_Delete(uint64_t tenantId, const char *key);

#ifdef __cplusplus
}
#endif
```

#### 3.4 依赖关系
- 依赖Agent层接口
- 依赖消息结构定义
- 依赖错误码定义

#### 3.5 交付物
- `sdk/bio.h` - C++公共接口定义
- `sdk/bio_c.h` - C语言接口定义
- `sdk/bio.cpp` - SDK实现
- `sdk/bio_qos.cpp/h` - QoS管理
- `sdk/mirror_client.cpp/h` - 镜像客户端
- `test/benchmarks/console.cpp` - 示例代码

---

## 四、接口对接规范

### 4.1 Agent层导出函数（服务端提供）

```cpp
// 函数声明
extern "C" {
    int32_t BioServerInit();
    int32_t BioServerStandaloneInit();
    void BioServerExit();
    int32_t GetRuntimeConfig(ShmInitResponse *rsp);
    bool GetCrcFlag();
    bool GetCliFlag();
    bool GetPrometheusToggle();
    const char* GetPrometheusListenAddress();
    uint32_t GetNegoWorkIoTimeOut();
    uint32_t GetPrometheusScrapeIntervalSec();
    uintptr_t GetBioServerNet();
    int32_t GetLocalNid(GetLocalNidResponse *rsp);
    int32_t GetQuotaInfo(QueryQuotaRequest *req, QueryQuotaResponse *rsp);
    int32_t AllocQuota(AllocQuotaRequest *req, AllocQuotaResponse *rsp);
    int32_t FreeQuota(FreeQuotaRequest *req);
    int32_t GetNodeView(QueryNodeViewRequest *req, QueryNodeViewResponse *rsp);
    int32_t GetPtView(QueryPtViewRequest *req, QueryPtViewResponse *rsp);
    int32_t CreateFlowMaster(CreateFlowRequest *req, CreateFlowResponse *rsp);
    int32_t CreateFlowSlave(CreateFlowRequest *req);
    int32_t DestroyFlow(DestroyFlowRequest *req);
    int32_t NotifyUpdate(NotifyUpdateRequest *req);
    int32_t CheckUpdateReady(CheckUpdateReadyRequest *req, CheckUpdateReadyResponse *rsp);
    int32_t GetSlice(GetSliceRequest *req, GetSliceResponse **rsp);
    int32_t Put(PutRequest *req, PutResponse *rsp);
    int32_t Get(GetRequest *req, GetResponse *rsp);
    int32_t Delete(DeleteRequest *req);
    int32_t AddDisk(AddDiskRequest *req, AddDiskResponse *rsp);
    int32_t Stat(StatRequest *req, StatResponse *rsp);
    int32_t List(ListRequest *req, ListResponse **rsp);
    int32_t Load(LoadRequest *req);
    int32_t GetCacheHitLocal(CacheHitResponse *rsp);
    int32_t CalcCacheResourceLocal(CacheResourceResponse *rsp);
    int32_t GetTracePointsLocal(GetTracePointsResponse *rsp);
}
```

### 4.2 错误码定义

```cpp
// bio_err.h
enum BResult {
    BIO_OK = 0,
    BIO_ERR = -1,
    BIO_INNER_ERR = -2,
    BIO_NOT_READY = -3,
    BIO_NOT_EXISTS = -4,
    BIO_ALLOC_FAIL = -5,
    BIO_INVALID_PARAM = -6,
    BIO_TIMEOUT = -7,
    BIO_NETWORK_ERR = -8,
    BIO_PERMISSION_DENIED = -9,
    // ... 更多错误码
};
```

### 4.3 消息类型定义

```cpp
// message.h
enum MessageType {
    MSG_SHM_INIT = 1,
    MSG_QUERY_QUOTA = 2,
    MSG_ALLOC_QUOTA = 3,
    MSG_FREE_QUOTA = 4,
    MSG_GET_NODE_VIEW = 5,
    MSG_GET_PT_VIEW = 6,
    MSG_CREATE_FLOW = 7,
    MSG_DESTROY_FLOW = 8,
    MSG_NOTIFY_UPDATE = 9,
    MSG_CHECK_UPDATE_READY = 10,
    MSG_GET_SLICE = 11,
    MSG_PUT = 12,
    MSG_GET = 13,
    MSG_DELETE = 14,
    MSG_ADD_DISK = 15,
    MSG_STAT = 16,
    MSG_LIST = 17,
    MSG_LOAD = 18,
    MSG_CACHE_HIT = 19,
    MSG_CACHE_RESOURCE = 20,
    MSG_GET_TRACE_POINTS = 21,
};
```

---

## 五、开发顺序建议

### 5.1 推荐的开发顺序

1. **第一阶段：基础架构（程序员2）**
   - 实现服务端基础框架
   - 实现配置加载
   - 实现日志和追踪
   - 导出必要的函数符号

2. **第二阶段：Agent接口（程序员1）**
   - 实现Agent初始化
   - 实现函数指针加载
   - 实现基本的网络通信
   - 与服务端联调

3. **第三阶段：SDK封装（程序员3）**
   - 定义SDK接口
   - 实现C/C++接口
   - 编写示例代码
   - 完整集成测试

4. **第四阶段：功能完善**
   - 完善缓存模块
   - 完善流管理
   - 完善监控功能
   - 性能优化

### 5.2 关键里程碑

| 里程碑 | 内容 | 验收标准 |
|--------|------|----------|
| M1 | Agent可调用服务端函数 | dlopen成功，函数指针加载成功 |
| M2 | 基础数据操作 | Put/Get/Delete可以工作 |
| M3 | 缓存功能完整 | Cache模块正常工作 |
| M4 | SDK可用 | 应用程序可以调用SDK |
| M5 | Standalone模式 | 单机模式可正常运行 |
| M6 | 完整集成测试 | 所有功能正常工作 |

---

## 六、代码组织结构

```
ubsio-boostio/src/
├── sdk/                      # 客户端SDK（程序员3）
│   ├── bio.h                # 公共API定义
│   ├── bio.cpp              # SDK实现
│   ├── bio_c.h              # C语言接口
│   ├── bio_client.cpp       # Agent调用封装
│   ├── bio_client_agent.cpp # Agent实现（程序员1）
│   ├── bio_client_agent.h
│   ├── bio_client_net.cpp   # 网络通信
│   ├── bio_client_net.h
│   ├── bio_qos.cpp/h       # QoS管理
│   └── mirror_client.cpp/h  # 镜像客户端
├── server/                   # 服务端（程序员2）
│   ├── bio_server.cpp      # 服务端主入口
│   ├── bio_server.h
│   ├── standalone_view.cpp/h # Standalone模式
│   ├── mirror_server.cpp/h  # Mirror服务器
│   └── mirror_server_crb.cpp/h
├── cache/                    # 缓存模块（程序员2）
│   ├── cache.h
│   ├── cache_slice.h
│   ├── read/               # 读缓存
│   └── write/              # 写缓存
├── flow/                    # 流管理（程序员2）
│   ├── flow.h
│   └── flow_manager.cpp
├── net/                     # 网络模块（程序员2）
│   ├── net_engine.cpp
│   └── net_connector.cpp
├── cluster/                 # 集群管理（程序员2）
│   ├── cm.h
│   ├── client/
│   ├── server/
│   └── common/
└── common/                  # 公共组件
    ├── bio_err.h           # 错误码
    ├── bio_types.h         # 类型定义
    └── message.h           # 消息结构
```

---

## 七、注意事项

### 7.1 跨平台考虑
- 处理好dlopen的路径问题（DEBUG_UT vs 生产环境）
- 注意字节序问题
- 注意32位/64位兼容

### 7.2 性能考虑
- Agent调用应该尽量减少内存拷贝
- 考虑使用共享内存传递大数据
- 注意锁的使用，避免死锁

### 7.3 错误处理
- 所有函数都要有错误处理
- 使用统一的错误码
- 做好日志记录

### 7.4 线程安全
- Agent单例要线程安全
- 服务端要考虑并发访问
- 做好同步机制

---

## 八、联系方式

如有接口对接问题，请参考以下文件：
- `docs/API接口说明.txt` - API详细说明
- `docs/通信矩阵.txt` - 消息通信矩阵
- `src/message/message.h` - 消息结构定义
- `src/common/bio_err.h` - 错误码定义
