#include "GeometryTopologyDiagnosticTaskQueue.h"

#include <NCollection_IndexedMap.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <set>
#include <string>
#include <utility>

namespace {
/**
 * @brief 单个时间片的上限（毫秒）。
 *
 * 这个取值决定"新提交的请求最多等多久"与"让位本身的开销"之间的折中：每片结束时工作
 * 线程会检查一次是否有新请求在等，有就先把工作线程让出去。片太短会让长任务频繁进出、
 * 增加开销；太长则失去意义（一次 60 秒的诊断仍会把后面的短任务挡住很久）。
 */
constexpr double kSliceMilliseconds = 200.0;

/**
 * @brief 判断排队中的请求是否应被新请求替换。
 *
 * 同一请求方同一时刻只关心最新一次请求：形状被替换或阈值被改动后，旧请求已经失去
 * 意义，直接替换可以避免重复排队，也避免白算一遍旧形状。跨请求方绝不能替换，否则
 * 另一方的结果会永远等不到。
 */
bool canReplace(const GeometryTopologyDiagnosticRequest& queued,
    const GeometryTopologyDiagnosticRequest& incoming)
{
    return queued.owner == incoming.owner;
}

/** @brief 把耗时格式化为一位小数的毫秒文本。 */
std::string formatMilliseconds(double milliseconds)
{
    char buffer[32] {};
    std::snprintf(buffer, sizeof(buffer), "%.1f", milliseconds);
    return buffer;
}

/** @brief 描述本次请求覆盖的诊断类别，便于在日志里对照耗时。 */
std::string describeOptions(const GeometryTopologyDiagnosticOptions& options)
{
    std::string text;
    auto append = [&text](const char* name, bool enabled) {
        if (!enabled)
            return;
        if (!text.empty())
            text += ' ';
        text += name;
    };
    append("边拓扑", options.edge_topology);
    append("细小边", options.small_edges);
    append("细小面", options.small_faces);
    append("重复面", options.duplicate_faces);
    append("相交面", options.intersecting_faces);
    append("无效拓扑", options.invalid_topology);
    return text.empty() ? std::string("无") : text;
}

/** @brief 描述诊断结果各项的规模。 */
std::string describeResult(const GeometryTopologyDiagnosticResult& result)
{
    char buffer[320] {};
    std::snprintf(buffer, sizeof(buffer),
        "边界边=%zu 孤立边=%zu 非流形边=%zu 细小边=%zu 细小面=%zu 重复面组=%zu 相交面对=%zu"
        " 无效拓扑=%zu",
        result.boundary_edges.size(), result.isolated_edges.size(),
        result.non_manifold_edges.size(), result.small_edges.size(), result.small_faces.size(),
        result.duplicate_face_groups.size(), result.intersecting_face_pairs.size(),
        result.invalid_shapes.size());
    return buffer;
}

/**
 * @brief 把一次后台诊断的规模、排队时间与耗时写入日志。
 *
 * 诊断耗时同时取决于构建配置与 OpenCASCADE 动态库版本（见启动时的环境自检）：
 * 两套同名动态库被 PATH 选错就会慢约一个数量级。把"面/边规模 + 选项 + 排队时间 +
 * 真实耗时"打出来，才能在界面日志里直接区分"算法慢"、"排在前一个任务后面"与
 * "加载了错误的依赖库"。
 */
void logOutcome(const GeometryTopologyDiagnosticRequest& request,
    const GeometryTopologyDiagnosticOutcome& outcome, double queued_ms, double elapsed_ms,
    int yields)
{
    const std::string elapsed = formatMilliseconds(elapsed_ms);
    const std::string queued = formatMilliseconds(queued_ms);
    // 让位次数只在真的被新请求抢占过时才打印，日常日志不长。
    const std::string yields_text
        = yields > 0 ? "，让位 " + std::to_string(yields) + " 次" : "";
    if (outcome.cancelled) {
        spdlog::info("几何拓扑诊断：已取消，排队 {} ms，已计算 {} ms{}", queued, elapsed,
            yields_text);
        return;
    }
    if (outcome.failed) {
        spdlog::warn("几何拓扑诊断：计算失败，排队 {} ms，已计算 {} ms{}", queued, elapsed,
            yields_text);
        return;
    }

    // 规模统计只在信息级日志确实会输出时才做，避免给超大模型增加无谓开销。
    std::string scale = "规模未统计";
    if (request.shape && spdlog::default_logger()->should_log(spdlog::level::info)) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
        TopExp::MapShapes(*request.shape, TopAbs_FACE, faces);
        TopExp::MapShapes(*request.shape, TopAbs_EDGE, edges);
        scale = "面 " + std::to_string(faces.Extent()) + " / 边 " + std::to_string(edges.Extent());
    }

    spdlog::info("几何拓扑诊断：{}，选项 [{}]，排队 {} ms，计算 {} ms{}，结果 [{}]", scale,
        describeOptions(request.options), queued, elapsed, yields_text,
        describeResult(outcome.result));
}

}

GeometryTopologyDiagnosticTaskQueue& GeometryTopologyDiagnosticTaskQueue::shared()
{
    // 故意使用"永不析构"的单例：工作线程可能正阻塞在长达数十秒的 OCC 计算中，
    // 若在退出时析构队列（join 会等它跑完，detach 又会踩到已销毁的成员）都不安全。
    // 对象本身很小，进程退出后由操作系统回收。
    static GeometryTopologyDiagnosticTaskQueue* queue = new GeometryTopologyDiagnosticTaskQueue();
    return *queue;
}

GeometryTopologyDiagnosticTaskQueue::~GeometryTopologyDiagnosticTaskQueue()
{
    shutdown();
}

std::uint64_t GeometryTopologyDiagnosticTaskQueue::submit(GeometryTopologyDiagnosticRequest request)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_)
        return 0;

    request.generation = next_generation_++;
    const std::uint64_t generation = request.generation;
    request.submitted_at = std::chrono::steady_clock::now();
    // 地址可能被新对象复用：既然它又提交了请求，就不再是"已析构请求方"。
    discarded_owners_.erase(request.owner);

    // 该请求方此前挂起（被让位）的任务已被新请求取代，直接丢弃。
    const void* owner = request.owner;
    suspended_.erase(std::remove_if(suspended_.begin(), suspended_.end(),
                         [owner](const std::unique_ptr<ActiveTask>& task) {
                             return task && task->request.owner == owner;
                         }),
        suspended_.end());

    auto replaced = std::find_if(pending_.begin(), pending_.end(),
        [&request](const GeometryTopologyDiagnosticRequest& queued) {
            return canReplace(queued, request);
        });
    if (replaced != pending_.end()) {
        *replaced = std::move(request);
        condition_.notify_one();
        return generation;
    }

    pending_.push_back(std::move(request));
    if (!worker_.joinable())
        worker_ = std::thread(&GeometryTopologyDiagnosticTaskQueue::run, this);
    condition_.notify_one();
    return generation;
}

std::vector<GeometryTopologyDiagnosticOutcome> GeometryTopologyDiagnosticTaskQueue::takeOutcomes(
    const void* owner)
{
    std::vector<GeometryTopologyDiagnosticOutcome> outcomes;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto entry = ready_.begin(); entry != ready_.end();) {
        if (entry->owner == owner) {
            outcomes.push_back(std::move(*entry));
            entry = ready_.erase(entry);
        } else {
            ++entry;
        }
    }
    return outcomes;
}

void GeometryTopologyDiagnosticTaskQueue::discardOwner(const void* owner)
{
    std::lock_guard<std::mutex> lock(mutex_);
    discarded_owners_.insert(owner);
    pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                       [owner](const GeometryTopologyDiagnosticRequest& request) {
                           return request.owner == owner;
                       }),
        pending_.end());
    suspended_.erase(std::remove_if(suspended_.begin(), suspended_.end(),
                         [owner](const std::unique_ptr<ActiveTask>& task) {
                             return task && task->request.owner == owner;
                         }),
        suspended_.end());
    ready_.erase(std::remove_if(ready_.begin(), ready_.end(),
                     [owner](const GeometryTopologyDiagnosticOutcome& outcome) {
                         return outcome.owner == owner;
                     }),
        ready_.end());
}

bool GeometryTopologyDiagnosticTaskQueue::isBusy()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return busy_ || !pending_.empty();
}

bool GeometryTopologyDiagnosticTaskQueue::hasWork()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return busy_ || !pending_.empty() || !suspended_.empty() || !ready_.empty();
}

bool GeometryTopologyDiagnosticTaskQueue::hasReadyResults()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !ready_.empty();
}

void GeometryTopologyDiagnosticTaskQueue::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        pending_.clear();
        suspended_.clear();
    }
    condition_.notify_all();
    if (worker_.joinable())
        worker_.detach();
}

void GeometryTopologyDiagnosticTaskQueue::publish(ActiveTask& task, bool cancelled, bool failed)
{
    GeometryTopologyDiagnosticOutcome outcome;
    outcome.owner = task.request.owner;
    outcome.generation = task.request.generation;
    outcome.options = task.request.options;
    outcome.cancelled = cancelled;
    outcome.failed = failed;
    if (!cancelled && !failed && task.session)
        outcome.result = task.session->takeResult();

    const double queued_ms = std::chrono::duration<double, std::milli>(
        task.started_at - task.request.submitted_at)
                                 .count();
    logOutcome(task.request, outcome, queued_ms, task.spent_ms, task.preemptions);

    std::lock_guard<std::mutex> lock(mutex_);
    busy_ = false;
    if (stopping_ || discarded_owners_.count(outcome.owner) > 0) {
        // 请求方已经消失，结果不再需要保留。
        return;
    }
    ready_.push_back(std::move(outcome));
}

void GeometryTopologyDiagnosticTaskQueue::run()
{
    for (;;) {
        std::unique_ptr<ActiveTask> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return stopping_ || !pending_.empty() || !suspended_.empty();
            });
            if (stopping_ && pending_.empty() && suspended_.empty()) {
                suspended_.clear();
                return;
            }
            if (!pending_.empty()) {
                // 新提交的请求优先于被让位的长任务，这是"点一下不用等一分钟"的关键。
                task = std::make_unique<ActiveTask>();
                task->request = std::move(pending_.front());
                pending_.pop_front();
                task->started_at = std::chrono::steady_clock::now();
                busy_ = true;
            } else {
                task = std::move(suspended_.front());
                suspended_.pop_front();
            }
        }

        if (!task->session) {
            if (!task->request.shape) {
                publish(*task, false, true);
                continue;
            }
            try {
                // 会话构造会做参数校验与取消检查，因此在工作线程里建立。
                task->session = GeometryTopologyDiagnosticSession::start(*task->request.shape,
                    task->request.small_edge_length_threshold,
                    task->request.small_face_area_threshold, task->request.options,
                    task->request.cancel ? task->request.cancel.get() : nullptr);
            } catch (const GeometryTopologyDiagnosticCancelled&) {
                publish(*task, true, false);
                continue;
            } catch (...) {
                publish(*task, false, true);
                continue;
            }
        }

        const auto slice_started = std::chrono::steady_clock::now();
        bool done = false;
        bool cancelled = false;
        bool failed = false;
        try {
            // 后台只做纯几何计算，不触碰任何 VTK / 渲染状态；一个时间片最多推进
            // kSliceMilliseconds，片与片之间可以放弃工作线程、让新请求先算。
            done = task->session->advance(kSliceMilliseconds);
        } catch (const GeometryTopologyDiagnosticCancelled&) {
            cancelled = true;
        } catch (const Standard_Failure&) {
            failed = true;
        } catch (const std::exception&) {
            failed = true;
        } catch (...) {
            failed = true;
        }
        task->spent_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - slice_started)
                              .count();

        if (done || cancelled || failed) {
            publish(*task, cancelled, failed);
            continue;
        }

        // 还没算完：放回挂起队列首位。取任务时 pending_ 优先，因此有新的请求在等就
        // 自然让位，没有就立刻继续推进同一个任务。
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            suspended_.clear();
            busy_ = false;
            continue;
        }
        // 只有确实有新请求在等时才算"让位"，普通的时间片切换不计。
        if (!pending_.empty())
            ++task->preemptions;
        suspended_.push_front(std::move(task));
    }
}
