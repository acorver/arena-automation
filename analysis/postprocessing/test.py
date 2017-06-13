from concurrent import futures
import random
import time


def task(n):
    time.sleep(random.random())
    return (n, n / 10)


ex = futures.ThreadPoolExecutor(max_workers=5)
print('main: starting')

def wait_for():
    for i in range(50, 0, -1):
        print(i)
        yield ex.submit(task, i)

for f in futures.as_completed(wait_for()):
    print('main: result: {}'.format(f.result()))
