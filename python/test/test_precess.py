# precess 绑定冒烟测试：会话生命周期、只读查询、undo 与功能调用 e2e
# 经 ctest 运行（precess_python_smoke），--plugins 传入构建树插件目录。
import argparse
import os
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--plugins", default="")
args = parser.parse_args()

# Python 3.8 起扩展模块的依赖 DLL 不再经 PATH 解析：先把依赖目录注册进进程 DLL 搜索集
for dll_dir in os.environ.get("PRECESS_DLL_DIRS", "").split(os.pathsep):
    if dll_dir and Path(dll_dir).is_dir():
        os.add_dll_directory(dll_dir)

import precess

# —— 空会话的查询与 undo 状态 ——
session = precess.Session()
query = session.query
assert query.list_models() == []
assert not query.has_model(0)
assert not query.has_component(0)
assert query.model_name(0) is None
assert query.find_model_id_by_component(-1) == -1
assert query.first_mesh_component_id(0) is None

undo_stack = session.undo_stack
assert not undo_stack.can_undo()
assert not undo_stack.can_redo()

# 结构操作对无效 id 抛异常（ModelLayer 既有语义），不产生 undo 记录
try:
    session.remove_model(-1)
    raise AssertionError("expected RuntimeError for invalid model id")
except RuntimeError:
    pass
session.remove_mesh(-1)
assert not undo_stack.can_undo()

# —— 插件装载 + 功能调用 e2e（插件目录缺失时跳过）——
plugins_dir = Path(args.plugins)
if plugins_dir.is_dir():
    session.load_static_plugins()
    session.load_plugins_from_directory(str(plugins_dir))

    names = session.feature_names()
    assert "CreateBox" in names, names

    # 写入目标 = 新建 Model（Combo 参数下标 6 的选项下标 2）
    assert session.set_parameter("CreateBox", 6, 2)
    component_id = session.invoke("CreateBox")
    assert isinstance(component_id, int) and component_id >= 0, component_id

    assert query.has_component(component_id)
    assert query.find_model_id_by_component(component_id) >= 0
    summary = query.geometry_summary(component_id)
    assert summary.has_geometry
    assert summary.face_count == 6 and summary.vertex_count == 8
    assert query.component_name(component_id) == "Box_1"

    # invoke 是操作边界：undo 自动记录，撤销后组件随临时模型一并消失
    assert undo_stack.can_undo()
    undo_stack.undo()
    assert not query.has_component(component_id)
    assert undo_stack.can_redo()
    undo_stack.redo()
    assert query.has_component(component_id)
    print("plugin e2e ok")
else:
    print("plugins dir not found, skip feature e2e")

session.teardown()
print("precess smoke ok")
