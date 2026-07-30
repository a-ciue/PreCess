/**
 * @file MeasureHandler.cpp
 * @brief 测量处理器：交互式两点成线测距与共端点夹角标注
 * @author 范成通 email 1941804585@qq.com
 */

#include "MeasureHandler.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureRegistrar.h"
#include "InteractionContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

namespace systems::feature {

namespace {
using Vec3 = std::array<double, 3>;

constexpr const char* kFeatureName = "MeasurePlugin"; //> 插件 json 注册名，过滤 ParameterChangedEvent 用

constexpr double kEps = 1e-9;

Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}

//! @brief 两点中点
Vec3 midpoint(const Vec3& a, const Vec3& b)
{
    return { (a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0, (a[2] + b[2]) / 2.0 };
}

double dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double length(const Vec3& v)
{
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

//! @brief 数值转两位小数字符串（测量值显示精度）
std::string toString(double value)
{
    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss.precision(2);
    oss << value;
    return oss.str();
}

double angleBetween(const Vec3& u, const Vec3& v)
{
    const double lu = length(u);
    const double lv = length(v);
    if (lu < std::numeric_limits<double>::epsilon() || lv < std::numeric_limits<double>::epsilon())
        return 0.0;

    double cos_theta = dot(u, v) / (lu * lv);
    cos_theta = std::max(-1.0, std::min(1.0, cos_theta));
    return std::acos(cos_theta) * 180.0 / 3.14159265358979323846;
}

//! @brief 两点是否同一吸附点：全局顶点 id 优先，同源顶点坐标位级一致兜底
bool samePoint(const systems::interaction::PickInfo& a, const systems::interaction::PickInfo& b)
{
    if (a.mesh_id >= 0 && a.mesh_id == b.mesh_id)
        return true;
    if (a.geom_id >= 0 && a.geom_id == b.geom_id)
        return true;
    return a.world_pos == b.world_pos;
}
}

void MeasureHandler::setup(FeatureRegistrar& reg)
{
    // "清除"按钮参数：无值触发器，点击发布 ParameterChangedEvent，功能内部订阅并清空
    reg.addParameter({ ArgTypeEnum::Button, "清除", "" });
    reg.addMenuItem({ "工具", "测量" });
}

void MeasureHandler::activate(FeatureContext& ctx)
{
    // 标注集绑定到交互状态：渲染层事件后从 InteractionState.annotations 拉取绘制
    annotations_ = &ctx.interaction.annotations();
    ctx.interaction.onActivate([this]() { this->clear(); });
    ctx.interaction.onDeactivate([this]() { this->clear(); });
    ctx.interaction.onPick([this](const PickInfo& p) { return this->onPick(p); });
    ctx.interaction.onHover([this](const PickInfo& p) { return this->onHover(p); });

    // "清除"按钮经 ParameterChangedEvent 触发：清理作为刷新前置操作，延迟到渲染线程安全执行
    param_sub_ = ctx.events.subscribe<ParameterChangedEvent>([this, interaction = &ctx.interaction](const ParameterChangedEvent& e) {
        // 按功能名过滤：其他功能的参数变更不触发本功能清空
        if (e.feature != kFeatureName || e.param_index != 0)
            return;
        interaction->deferRefresh([this] { this->clear(); });
    });
}

// ---------------- 交互回调（经 InteractionContext 注册，渲染线程驱动） ----------------

bool MeasureHandler::onPick(const PickInfo& pick)
{
    if (!pick.valid)
        return false;

    if (!pending_) {
        pending_ = pick;
    } else if (samePoint(*pending_, pick)) {
        pending_.reset(); // 同一点再点一次 = 取消本次起笔
    } else {
        addLine(*pending_, pick);
        pending_.reset();
    }

    refreshAnnotations();
    return true;
}

bool MeasureHandler::onHover(const PickInfo& pick)
{
    // 未吸附或无起笔：清除已有预览；本来无预览则无需刷新
    if (!pending_ || !pick.valid) {
        if (!preview_)
            return false;
        preview_.reset();
        refreshAnnotations();
        return true;
    }

    preview_ = pick;
    refreshAnnotations();
    return true;
}

void MeasureHandler::addLine(const PickInfo& a, const PickInfo& b)
{
    // 与已有线完全重复（含反向）则忽略
    for (const MeasureLine& l : lines_) {
        if ((samePoint(l.a, a) && samePoint(l.b, b)) || (samePoint(l.a, b) && samePoint(l.b, a)))
            return;
    }

    // 与每条已有线做端点匹配，共端点即记录一组夹角
    for (const MeasureLine& l : lines_) {
        auto try_share = [&](const PickInfo& old_shared, const PickInfo& old_other,
                             const PickInfo& new_shared, const PickInfo& new_other) {
            if (!samePoint(old_shared, new_shared))
                return;
            MeasureAngle ang;
            ang.at = old_shared.world_pos;
            ang.p = old_other.world_pos;
            ang.q = new_other.world_pos;
            ang.angle = angleBetween(ang.p - ang.at, ang.q - ang.at);
            angles_.push_back(ang);
        };
        try_share(l.a, l.b, a, b);
        try_share(l.a, l.b, b, a);
        try_share(l.b, l.a, a, b);
        try_share(l.b, l.a, b, a);
    }
    lines_.push_back({ a, b });
}

void MeasureHandler::refreshAnnotations()
{
    annotations_->clear();

    // 已确认线的端点与线段（红色端点、绿色实线）
    for (const MeasureLine& l : lines_) {
        annotations_->points.push_back({ l.a.world_pos });
        annotations_->points.push_back({ l.b.world_pos });
        annotations_->lines.push_back({ l.a.world_pos, l.b.world_pos });
    }
    if (pending_)
        annotations_->points.push_back({ pending_->world_pos });

    // 长度文本：每线一个，放线段中点（白色）
    for (const MeasureLine& l : lines_) {
        const Vec3 mid = midpoint(l.a.world_pos, l.b.world_pos);
        annotations_->texts.push_back({ mid, "L: " + toString(length(l.b.world_pos - l.a.world_pos)), 1.0, 1.0, 1.0 });
    }

    // 夹角文本：放共点沿角平分线偏移（青色）；同一点多个夹角按序号加大偏移防重叠
    for (size_t ai = 0; ai < angles_.size(); ++ai) {
        const MeasureAngle& ang = angles_[ai];
        int stack = 0;
        for (size_t j = 0; j < ai; ++j) {
            if (angles_[j].at == ang.at)
                ++stack;
        }

        const Vec3 u = ang.p - ang.at;
        const Vec3 v = ang.q - ang.at;
        const double lu = length(u);
        const double lv = length(v);
        Vec3 dir { 0.0, 0.0, 0.0 };
        if (lu > kEps && lv > kEps) {
            // 角平分线方向；u、v 近反向（180°）时退化为两端点中点方向
            const Vec3 s { u[0] / lu + v[0] / lv, u[1] / lu + v[1] / lv, u[2] / lu + v[2] / lv };
            const double ls = length(s);
            if (ls > kEps) {
                dir = { s[0] / ls, s[1] / ls, s[2] / ls };
            } else {
                const Vec3 d = midpoint(ang.p, ang.q) - ang.at;
                const double ld = length(d);
                dir = ld > kEps ? Vec3 { d[0] / ld, d[1] / ld, d[2] / ld }
                                : Vec3 { u[0] / lu, u[1] / lu, u[2] / lu };
            }
        }
        const double dist = 0.25 * std::min(lu, lv) * (1.0 + 0.3 * stack);
        annotations_->texts.push_back({ { ang.at[0] + dir[0] * dist, ang.at[1] + dir[1] * dist,
                                           ang.at[2] + dir[2] * dist },
            "Ang: " + toString(ang.angle), 0.3, 0.9, 1.0 });
    }

    // 悬停动态预览：黄色虚线 + 黄色长度文本
    if (pending_ && preview_) {
        AnnotationLine preview;
        preview.p0 = pending_->world_pos;
        preview.p1 = preview_->world_pos;
        preview.r = 1.0;
        preview.g = 0.9;
        preview.b = 0.1;
        preview.dashed = true;
        annotations_->lines.push_back(preview);

        const Vec3 mid = midpoint(pending_->world_pos, preview_->world_pos);
        annotations_->texts.push_back({ mid,
            "L: " + toString(length(preview_->world_pos - pending_->world_pos)), 1.0, 0.9, 0.1 });
    }
}

void MeasureHandler::clear()
{
    pending_.reset();
    preview_.reset();
    lines_.clear();
    angles_.clear();
    // 调用方（on_activate/on_deactivate/deferRefresh 前置操作）均在渲染线程，重建空标注后由框架拉取刷新
    refreshAnnotations();
}

} // namespace systems::feature
