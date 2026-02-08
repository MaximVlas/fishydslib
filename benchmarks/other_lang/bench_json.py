import sys
import time
import json

try:
    import discord  # noqa: F401
except Exception as exc:
    print("discord.py not available:", exc)
    sys.exit(1)


SAMPLE_JSON = '{"id":"123456789012345678","name":"test","value":42}'
SAMPLE_BYTES = SAMPLE_JSON.encode("utf-8")
MIN_WALL_SECONDS = 0.5


def _cpu_time():
    return time.process_time()


def bench(name, fn):
    iters = 1
    # warmup
    for _ in range(1000):
        fn()
    # calibrate iterations to reach MIN_WALL_SECONDS
    while True:
        start = time.perf_counter()
        for _ in range(iters):
            fn()
        wall = time.perf_counter() - start
        if wall >= MIN_WALL_SECONDS:
            break
        iters *= 2

    wall_start = time.perf_counter()
    cpu_start = _cpu_time()
    for _ in range(iters):
        fn()
    wall = time.perf_counter() - wall_start
    cpu = _cpu_time() - cpu_start
    ns_per_wall = (wall / iters) * 1e9
    ns_per_cpu = (cpu / iters) * 1e9
    print(f"{name:30s} {ns_per_wall:10.1f} ns {ns_per_cpu:10.1f} ns {iters:11d}")


def main():
    print("----------------------------------------------------------------")
    print("Benchmark                      Time             CPU   Iterations")
    print("----------------------------------------------------------------")
    bench("BM_JSON_Parse", lambda: json.loads(SAMPLE_JSON))
    bench("BM_JSON_Parse_Buffer", lambda: json.loads(SAMPLE_BYTES))

    obj = json.loads(SAMPLE_JSON)
    snowflake_str = obj["id"]

    def snowflake_op():
        int(snowflake_str)

    bench("BM_JSON_Get_Snowflake", snowflake_op)

    def serialize_op():
        payload = {"id": snowflake_str, "name": "test", "value": 42}
        json.dumps(payload, separators=(",", ":"))

    bench("BM_JSON_Mut_Serialize", serialize_op)


if __name__ == "__main__":
    main()
