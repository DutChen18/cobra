#ifndef COBRA_TASK_HH
#define COBRA_TASK_HH

#include <atomic>
#include <exception>
#include <memory>
#include <functional>

namespace cobra {
	enum class task_state {
		wait,
		done,
		error,
	};

	enum class fd_type {
		in,
		out,
		task,
		none,
	};

	class task;

	template<class T>
	class future;

	class poll_target {
	private:
		fd_type type;
		int fd;
		const task* t;
	public:
		poll_target();
		poll_target(int fd, fd_type type);
		poll_target(const task& t);

		fd_type get_type() const;
		int get_fd() const;
		const task* get_task() const;
	};

	class task {
	private:
		task_state state = task_state::wait;
		std::unique_ptr<std::exception> exception;
	public:
		virtual ~task();

		virtual void poll();
		virtual poll_target target() const;

		task_state get_state() const;
		void set_state(task_state state);
		const std::exception* get_exception() const;
		void set_exception(std::exception exception);

		std::shared_ptr<task> and_then(std::function<void()> func) const;
		std::shared_ptr<task> and_then(std::function<std::shared_ptr<task>()> func) const;
		template<class T> std::shared_ptr<future<T>> and_then(std::function<T()> func) const;
		template<class T> std::shared_ptr<future<T>> and_then(std::function<std::shared_ptr<future<T>>()> func) const;
	};

	template<class T>
	class future : public task {
	private:
		std::unique_ptr<T> result;
	public:
		future();
		future(T result);
	
		const T* get_result() const;
		void set_result(T result);
		
		std::shared_ptr<task> and_then(std::function<void(const T& arg)> func) const;
		std::shared_ptr<task> and_then(std::function<std::shared_ptr<task>(const T& arg)> func) const;
		template<class U> std::shared_ptr<future<U>> and_then(std::function<U(const T& arg)> func) const;
		template<class U> std::shared_ptr<future<U>> and_then(std::function<std::shared_ptr<future<U>>(const T& arg)> func) const;
	};

	class task_and_then_task : public task {
	private:
		std::shared_ptr<task> before;
		std::function<std::shared_ptr<task>()> func;
		std::shared_ptr<task> after;
	public:
		task_and_then_task(std::shared_ptr<task> before, std::function<std::shared_ptr<task>()> func);

		void poll() override;
		poll_target target() const override;
	};

	template<class After>
	class task_and_then_future : public future<After> {
	private:
		std::shared_ptr<task> before;
		std::function<std::shared_ptr<future<After>>()> func;
		std::shared_ptr<future<After>> after;
	public:
		task_and_then_future(std::shared_ptr<task> before, std::function<std::shared_ptr<future<After>>()> func);

		void poll() override;
		poll_target target() const override;
	};

	template<class Before>
	class future_and_then_task : public task {
	private:
		std::shared_ptr<future<Before>> before;
		std::function<std::shared_ptr<task>(const Before& arg)> func;
		std::shared_ptr<task> after;
	public:
		future_and_then_task(std::shared_ptr<future<Before>> before, std::function<std::shared_ptr<task>()> func);

		void poll() override;
		poll_target target() const override;
	};

	template<class Before, class After>
	class future_and_then_future : public future<After> {
	private:
		std::shared_ptr<future<Before>> before;
		std::function<std::shared_ptr<future<After>>(const Before& arg)> func;
		std::shared_ptr<future<After>> after;
	public:
		future_and_then_future(std::shared_ptr<future<Before>> before, std::function<std::shared_ptr<future<After>>()> func);

		void poll() override;
		poll_target target() const override;
	};

	template<class T>
	future<T>::future() {
	}

	template<class T>
	future<T>::future(T result) {
		set_result(result);
	}

	template<class T>
	const T* future<T>::get_result() const {
		return result.get();
	}

	template<class T>
	void future<T>::set_result(T result) {
		set_state(task_state::done);
		this->result.reset(new T(result));
	}

	task_and_then_future::task_and_then_future(std::shared_ptr<task> before, std::function<std::shared_ptr<task>()> func) {
		this->before = before;
		this->func = func;
	}

	void task_and_then_task::poll() {
		if (!after) {
			switch (before->get_state()) {
			case task_state::done:
				after = func();
				break;
			case task_state::error:
				set_exception(*after->get_exception());
				break;
			case task_state::wait:
				break;
			}
		}

		if (after) {
			switch (after->get_state()) {
			case task_state::done:
				set_state(task_state::done);
				break;
			case task_state::error:
				set_exception(*after->get_exception());
				break;
			case task_state::wait:
				break;
			}
		}
	}

	poll_target task_and_then_task::target() const {
		if (after) {
			return poll_target(*after);
		} else {
			return poll_target(*before);
		}
	}
}

#endif
