# Docker Container

---

The project includes a dockerfile allowing you to setup the game inside an isolated linux environment.

---

## Running the docker container

1. Build the docker container:
  ```bash
  docker build -t tanks-dev-x86_64 -f ../Dockerfile . .
  ```

2. Run the container:
  ```bash
  docker run -it \
    -v $(PWD):/workspace:cached \
    -e LD_LIBRARY_PATH=/workspace/1_gm_so:/workspace/1_algo_so \
    --workdir /workspace \
    --user root \
    tanks-dev-x86_64
  ```
  (Note that in some shells you might need to replcae `$(PWD)` with `${PWD}`)


After setting up the container, follow the instructions in [Building and Running the Game](BuildGuide.md).

---

[Back to Table of Contents](../README.md)