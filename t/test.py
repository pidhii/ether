def foldl(f, acc, l):
    for x in l:
        acc = f(acc, x)
    return acc

def myfilter(f, l):
    # return [x for x in l if f(x)]
    ret = []
    for x in l:
        if f(x): ret.append(x)
    return ret

def myrange(f, t):
    # return range(f, t)
    ret = []
    while f < t:
        ret.append(f)
        f += 1
    return ret

def mymap(f, l):
    # return [f(x) for x in l]
    ret = []
    for x in l:
        ret.append(f(x))
    return ret

def add(x, y): return x + y
def iseven(x): return x % 2 == 0
def isodd(x): return x % 2 != 0


n = 5000000

def job_user():
    return foldl(add, 0, myfilter(iseven, mymap(lambda x: x + 1, myrange(0, n))))

def job_native():
    return foldl(add, 0, [x + 1 for x in range(0, n) if isodd(x)])

job = job_user
# job = job_native

print(job())
for _ in range(1):
    job()

