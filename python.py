class Poller:
    def add(self, fd, value):
        pass

    def poll(self):
        return [(fd, value)]

class Task:
    def target(self):
        pass

def get_fd(task):
    tgt = task.target()

    if tgt is None:
        return None
    elif isinstance(tgt, int):
        return (tgt, task)
    elif isinstance(tgt, Task):
        return get_fd(tgt)

class EventLoop:
    def __init__(self):
        self.tasks = []
        self.poller = Poller()

    def schedule(self, task):
        self.tasks.append(task)
        tgt = get_fd(task)

        if tgt is not None:
            self.poller.add(tgt[0], task)

    def all_tasks(self):
        for task in self.tasks:
            while True:
                yield task
                tgt = task.target()

                if isinstance(tgt, Task):
                    task = tgt
                else:
                    break

    def poll(self):
        fds = self.poller.poll()

        for fd, task in fds:
            old_fd, tail = get_fd(task)
            tail.poll()
            new_fd, _ = get_fd(task)

            if old_fd != new_fd:
                self.poller.update(old_fd, new_fd)

        for task in self.tasks:
            fd, tail = get_fd(task)

            if fd is None:
                tail.poll()
