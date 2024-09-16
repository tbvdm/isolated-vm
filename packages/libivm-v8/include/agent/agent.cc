module;
#include <concepts>
#include <memory>
export module ivm.isolated_v8:agent.lock;
import :platform.foreground_runner;
import ivm.utility;
import v8;

namespace ivm {

// The base `agent` class holds a weak reference to a `agent::host`. libivm directly controls the
// lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export class agent : util::non_copyable {
	public:
		class host;
		class lock;
		class storage;

		explicit agent(
			const std::shared_ptr<host>& host,
			const std::shared_ptr<foreground_runner>& task_runner
		);

		auto schedule(std::invocable<lock&> auto task) -> void;

	private:
		std::weak_ptr<host> host_;
		std::shared_ptr<foreground_runner> task_runner_;
};

// A `lock` is a simple holder for an `agent::host` which proves that we are executing in
// the isolate context.
class agent::lock : util::non_moveable {
	public:
		explicit lock(host& host);
		~lock();

		auto operator*() -> host& { return host_; }
		auto operator*() const -> const host& { return host_; }
		auto operator->() -> host* { return &host_; }
		auto operator->() const -> const host* { return &host_; }

		static auto expect() -> lock&;

	private:
		host& host_;
		lock* prev_;
};

// Allow lambda-style callbacks to be called with the same virtual dispatch as `v8::Task`
template <std::invocable<agent::lock&> Invocable>
struct task_of : v8::Task {
		explicit task_of(Invocable&& task) :
				task_{std::forward<Invocable>(task)} {}

		auto Run() -> void final {
			task_(agent::lock::expect());
		}

	private:
		[[no_unique_address]] Invocable task_;
};

auto agent::schedule(std::invocable<lock&> auto task) -> void {
	task_runner_->schedule_non_nestable(std::make_unique<task_of<decltype(task)>>(std::move(task)));
}

} // namespace ivm
