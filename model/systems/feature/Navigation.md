# 导航注册与排序声明

顶部菜单、分组、功能入口和下拉子功能按同一父节点内的 `order` 升序排列；相同时按稳定 ID 的字典序排列。`order` 使用 `double`，可用 `10.5` 等小数在已有顺序码之间插入。标题仅用于显示，不参与身份或排序判断。

## 菜单、分组与入口

菜单和分组独立注册，不再从入口路径推导。菜单保存 `id`、`title`、`order`（默认 `1000`）；分组保存 `menu_id`、`id`、`title`、`order`（默认 `0`）。菜单 ID、分组 ID 使用不含 `/` 的稳定标识。分组 ID `default` 保留给适配器的未分组区域，不可作为具名分组注册。

`FeatureNavigationEntry` 依次保存 `id`、`title`、`icon`、`order`、`menu_id`、`group_id`。入口必须明确引用一个已注册或本次声明的菜单；`group_id` 可留空，非空时必须引用所属菜单下的分组。入口及子功能的默认 `order` 为 `0`。不同功能声明相同入口 ID 后仍自动聚合，同一功能关联多个入口时仍共用一份参数与执行状态。共享入口的提供者选择规则保持不变。

插件在 `setup` 中调用 `registrar.addNavigationMenu(...)` 和 `registrar.addNavigationGroup(...)` 收集父节点声明，再通过 `registrar.navigation().addEntry(...)` 添加入口：

```cpp
registrar.addNavigationMenu({ "mesh-generation", "网格生成算法", 30 });
registrar.addNavigationGroup({ "mesh-generation", "surface", "曲面", 10 });
auto& navigation = registrar.navigation();
navigation.addEntry({ "quadrilateral", "四边形网格生成", "", 20.5,
    "mesh-generation", "surface" });
navigation.setLabel("四边形方法");
navigation.setOrder(10.5);
```

`FeatureRegistrar` 是临时收集器；`setup`、参数准备和父节点引用验证成功后，系统才接纳本次菜单、分组及功能，并统一通知界面刷新。失败注册不保留本次父节点定义。

菜单按 ID、分组按 `(menu_id, id)` 去重，首次成功接纳的定义决定标题和顺序，后续重复声明保留原值。应用在加载静态、动态插件前，通过 `FeatureSystem::tryRegisterNavigationMenu(...)` / `tryRegisterNavigationGroup(...)` 预注册产品布局；插件声明其所需节点以支持独立加载。首次成功注册优先的规则仅作用于菜单、分组，不改变共享入口的提供者选择。

定义由 `FeatureSystem` 持有，生命周期随会话。插件卸载时移除其入口关联，菜单与分组定义保留；没有内容的纯功能菜单和分组由界面隐藏。文件、编辑、视图等自带内容的菜单继续显示。同一会话内重复注册不更新已有父节点标题和顺序，本接口不提供运行时父节点更新。

## 入口与子功能排序

菜单、分组的顺序只来自其注册定义。`navigation.addOrder(...)` 和宿主的 `FeatureSystem::setNavigationOrder(...)` 继续覆盖入口、子功能的顺序，不再接收根菜单或 `menu:` 分组排序规则：

| 排序层级 | parent | id |
| --- | --- | --- |
| 分组内图标 | `group:<菜单ID>/<分组ID>` | `category:<入口ID>`，默认未归类功能为 `feature:<功能内部名称>` |
| 入口内下拉子功能 | `entry:<入口ID>` | 功能内部名称 |

无分组入口在展示数据中使用 `default` 分组，因此对应覆盖规则为 `group:<菜单ID>/default`。普通用法直接在菜单、分组、入口的声明中填写顺序，并用 `navigation.setOrder(...)` 设置该功能在所有入口内的默认顺序，不需要再对同一顺序添加覆盖。

需要特殊排序时才声明覆盖。例如，注册名为 `MyQuadFeature` 的功能同时关联 `quadrilateral` 和 `triangle` 入口，若只需调整它在 `triangle` 内的位置，可在该功能的 `setup` 中调用：

```cpp
navigation.addOrder({ "entry:triangle", "MyQuadFeature", -10 });
```

覆盖仅改变展示，功能在其他入口继续使用默认顺序。宿主需要固定产品布局时，可通过 `FeatureSystem::setNavigationOrder(...)` 提供入口或子功能的完整覆盖列表。

入口、子功能的排序覆盖仍为宿主优先；多个插件声明相同父子节点时，取功能内部名称字典序最小者。单个来源重复声明保留首项，空子项 ID 被忽略，显式 `0` 是有效覆盖。未知排序目标不会创建空菜单，只在目标出现时生效；插件卸载后移除其覆盖，宿主覆盖继续保留。

## 产品布局与 QML 适配

产品默认布局位于 `app/model/QModelManager.cpp`：文件 `10`、编辑 `20`、网格生成 `30`、其他算法 `40`、几何 `50`、工具 `60`、示例 `70`、功能 `80`、视图 `10000`。几何菜单内仍为未分组区域 `10`、拓扑 `20`、修复 `30`。应用可注册 `{ "geometry", "", "", 10 }` 的无标题分组布局，插件入口继续省略 `group_id`，无需声明名为 `default` 的分组。

宿主继续通过 `setNavigationEntries(...)` 声明四类常驻网格入口；没有功能实现时显示空态并禁用执行。未声明入口的功能仍归入默认“功能”菜单。

C++ 入口和 QML 展示数据均不使用 `menu_path`。`QFeatureSystemAdaptor` 根据注册定义输出 `menu_id`、`group_id` 和独立的 `menu_title`、`group_title`，标题中的 `/` 按普通字符处理。导航脚本将空 `group_id` 映射为未分组的 `default` 区域；未声明入口的功能生成入口时使用默认菜单 ID `features` 和标题“功能”；与正式 `features` 入口混合时，菜单显示标题沿用先聚合的正式入口。父节点顺序转换成现有 `navigationOrder` 格式；展示数据由注册定义派生，不额外保存可修改的父节点配置。

页签与页面的 `objectName` 分别使用 `featureTab_<菜单ID>`、`featureMenuPage_<菜单ID>`。导航刷新按菜单 ID 保持当前页，按入口 ID、功能名称保持当前操作、参数与选择监听。显示顺序与页面索引分离，视图弹出菜单及文件、编辑页面继续执行原有行为。

## 管理边界与迁移

父节点注册、`setNavigationEntries(...)` 和 `setNavigationOrder(...)` 仅允许在所属线程、模型操作空闲时调用。成功的布局变更经 `featuresInfoChanged` 刷新；重复父节点注册不覆盖定义，也不触发无意义刷新。

旧插件必须移除入口的路径字段，补充菜单、分组声明并改用稳定 ID 引用。注册时验证父节点引用，避免旧聚合初始化虽可编译却把路径当成 ID。公共导航头文件改变了源码及 ABI，主程序、全部项目内插件、示例与独立 SDK / Addons 插件必须使用匹配头文件重新全量编译。
