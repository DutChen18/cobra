std::shared_ptr<task> get_task() {
	cobra::future<int> fut;

	return fut.then([](int a) {
		std::cout << a;
	}).then([]() {
		
	});
}

int main() {
	std::shared_ptr<task> t = get_task();

	t->wait_task() == fut;
	t2->wait_task() == fut;
	t->poll_task() == fut;

	t->prev_task()->get_state() == done -> t->poll();
	t->prev_task()->prev_task()->get_state() == done -> t->prev_task()->poll();

	cobra::event_loop loop;

	loop.schedule(t);
	loop.run();
}
