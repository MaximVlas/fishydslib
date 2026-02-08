PY ?= python3
NODE ?= node
BUN ?= bun

.PHONY: all python node bun

all: python node bun

python:
	@command -v $(PY) >/dev/null 2>&1 && $(PY) bench_json.py || echo "python3 not found"

node:
	@command -v $(NODE) >/dev/null 2>&1 && $(NODE) bench_json.js || echo "node not found"

bun:
	@command -v $(BUN) >/dev/null 2>&1 && $(BUN) bench_json.js || echo "bun not found"
