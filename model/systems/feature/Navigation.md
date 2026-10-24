# 导航排序声明

顶部菜单、分组、功能入口和下拉子功能统一按同一父节点内的 `order` 升序排列；相同时按稳定 ID 的字典序排列。排序不依赖显示标题、插件加载顺序或容器遍历顺序。

## 声明位置

插件在 `setup` 中通过 `registrar.navigation().addOrder({ parent, id, order })` 声明默认顺序。应用通过 `FeatureSystem::setNavigationOrder(...)` 集中覆盖产品顺序。宿主覆盖优先；多个插件声明同一父子节点时，取功能内部名称字典序最小者。单个来源重复声明同一父子节点时保留首项。空子项 ID 被忽略。显式 `0` 是有效覆盖值。

| 排序层级 | parent | id |
| --- | --- | --- |
| 顶部菜单 | 空字符串 | 菜单稳定 ID |
| 菜单内分组 | `menu:<菜单ID>` | 分组稳定 ID |
| 分组内图标 | `group:<菜单ID>/<分组ID>` | `category:<入口ID>`，默认未归类功能为 `feature:<功能内部名称>` |
| 入口内下拉子功能 | `entry:<入口ID>` | 功能内部名称 |

新声明的菜单、分组 ID 使用不含 `/` 的稳定标识，不使用翻译后的显示文字。顶部内置 ID `file`、`edit`、`other-algorithms`、`view` 为宿主保留，不用于新插件菜单。

## 示例

```cpp
auto& navigation = registrar.navigation();
navigation.addEntry({ "quadrilateral", "四边形网格生成", "", 20,
    "网格生成算法/曲面", "mesh-generation", "surface" });
navigation.addOrder({ "", "mesh-generation", 30 });
navigation.addOrder({ "menu:mesh-generation", "surface", 10 });
navigation.addOrder({ "group:mesh-generation/surface", "category:quadrilateral", 20 });
navigation.addOrder({ "entry:quadrilateral", "MyQuadFeature", 10 });
```

入口的 `order` 仍是图标的默认顺序；`navigation.setOrder(...)` 仍是该功能在所有入口内的默认顺序。`addOrder` 可覆盖某一个父节点内的位置，同一功能在不同入口里的顺序互不影响。排序仅改变展示，功能对象、参数和执行状态仍共用一份。

宿主产品配置位于 `app/model/QModelManager.cpp`，四类网格入口及顶部菜单的默认顺序均在这里声明；四边形入口内的 Ricci 流方法通过同一配置优先显示。新增或调整顺序无需在通用 QML 中按菜单名称编写判断。

内置功能显式使用 `geometry`、`tools`、`examples`、`features` 菜单 ID，几何分组使用 `topology`、`repair`。工具栏页签与页面的 `objectName` 分别采用 `featureTab_<菜单ID>`、`featureMenuPage_<菜单ID>`，改显示标题不影响自动化定位。

## 兼容与更新

`FeatureNavigationEntry` 新增可选的 `menu_id`、`group_id`；`menu_path` 继续表达显示名称及位置。旧插件不填 ID 时分别回退到菜单段和分组段，空路径仍归入稳定 ID 为 `features` 的“功能”菜单，无分组时分组 ID 为 `default`。旧条目先复用相同显示路径下唯一明确声明的菜单／分组 ID；没有声明或同名声明有歧义时再回退到路径文字，避免误合并。同一宿主入口继承宿主路径时也继承其稳定 ID；插件显式声明另一个路径时不继承。旧声明需迁移为稳定 ID 后，才能保证改标题不改变排序身份。

没有排序覆盖时，菜单默认值为 1000，分组为 0；图标和子功能沿用已有默认值。未知排序目标不会创建空菜单，只在目标出现时生效。插件卸载后重新解析其声明；宿主入口和覆盖不随插件卸载消失。

`setNavigationOrder` 与入口注册一样只允许模型操作空闲时调用，变更后经 `featuresInfoChanged` 统一刷新。刷新按菜单 ID 保持当前页，按入口 ID 和功能名保持当前操作及参数。视图等弹出菜单只参与位置排序，其行为保留。

公共导航头文件发生变化后，必须重建主程序和全部插件；独立 SDK 插件也须使用匹配的头文件重新编译。
