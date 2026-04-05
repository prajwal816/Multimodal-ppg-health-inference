# Lightweight dev image: C++ demo + Python ONNX pipeline (simulated hardware).
FROM python:3.11-slim-bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY CMakeLists.txt .
COPY src/cpp ./src/cpp
COPY tests/cpp ./tests/cpp
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DPPG_BUILD_TESTS=ON \
    && cmake --build build -j \
    && ctest --test-dir build --output-on-failure

COPY configs ./configs
COPY scripts ./scripts
COPY benchmarks ./benchmarks
COPY src/python ./src/python
COPY pytest.ini .
COPY tests ./tests

ENV PYTHONPATH=/app/src/python

RUN pytest

CMD ["python", "scripts/run_pipeline.py", "--mode", "sim", "--windows", "15"]
