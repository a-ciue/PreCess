# 导航注册与排序

四层导航按有限 `double order` 升序排列，同值按稳定 ID 排序；标题只用于显示。

## 插件声明

| 类型 | 字段顺序 | 默认 order |
| --- | --- | --- |
| 菜单 | `id, title, order` | `1000` |
| 分组 | `menu_id, id, title, order` | `0` |
| 入口 | `id, title, icon, order, menu_id, group_id` | `0` |

菜单、分组 ID 不含 `/`；`default` 是未分组区域的保留 ID，其布局用空分组 ID/标题声明。入口须引用有效菜单和分组，空 `group_id` 表示未分组。

```cpp
registrar.addNavigationMenu({ "mesh-generation", "网格生成算法", 30 });
registrar.addNavigationGroup({ "mesh-generation", "surface", "曲面", 10 });
auto& navigation = registrar.navigation();
navigation.addEntry({ "quadrilateral", "四边形网格生成", "", 20.5,
    "mesh-generation", "surface" });
navigation.setLabel("四边形方法");
navigation.setOrder(10.5);
```

`setLabel/setIcon/setOrder` 配置子功能；同一功能在各入口内使用相同默认顺序。`setEntryDefault(entry, parameter, value)` 设置入口参数预设，Combo 值为索引字符串。

相同入口 ID 自动聚合子功能，同一功能的多个入口共用参数和状态。单个子功能直接显示参数，多个显示选择器；未声明入口的功能归入“功能”菜单。

父节点首次成功注册的定义保留至会话结束；注册失败不留下声明，卸载后的空菜单和分组隐藏。插件加载器捕获注册异常并记录日志，继续加载其余插件。

## 宿主布局

宿主在加载插件前调用 `tryRegisterNavigationMenu/Group` 预注册布局。

`setNavigationEntries` 保留常驻入口，零子功能时禁用执行。产品布局见 [QModelManager.cpp](../../../app/model/QModelManager.cpp)。注册和布局变更须在所属线程、模型操作空闲时调用。

宿主可用 `setNavigationOrder` 覆盖入口或子功能顺序：

| 目标 | parent | id |
| --- | --- | --- |
| 分组内入口 | `group:<菜单ID>/<分组ID>` | `category:<入口ID>` 或 `feature:<功能名>` |
| 入口内子功能 | `entry:<入口ID>` | 功能内部名称 |

未分组使用 `default`。重复目标保留首项，空 ID 忽略，`0` 有效；未知目标出现时生效，无效列表整批拒绝。菜单、分组顺序来自注册定义。

## 迁移

移除 `menu_path` 和插件 `addOrder`，改用父节点 ID 与默认 `order`。公共头变更后全量重编主程序和插件；独立构建与加载验证见 [SDK 构建说明](../../../sdk/skills/zenithgrid-external-plugin-development/references/build.md)。
