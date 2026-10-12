# Stanford Pintos

## Introduction

This repository records my attempt at a complete implementation of Stanford university's [Pintos](https://web.stanford.edu/class/cs140/projects/) Operating System. The table below showcases my progress.

| Project | Status | Implementation | Design Document |
| --- | --- | --- | --- |
| Project 1: Threads | Complete — all 27 tests passed | [threads branch](../../tree/threads) | [DESIGNDOC_threads.md](../../blob/threads/src/threads/DESIGNDOC_threads.md) |


## Installation Instructions

If you wish to work on this project on your own, follow the instructions below to set up a docker image with a fully installed starter code downloaded from the author's [git](https://pintos-os.org/cgi-bin/gitweb.cgi?p=pintos-anon) repository.

- Create a folder named `pintos` in a directory of your choice (for example `$HOME`).

```bash
$ cd $HOME
$ mkdir pintos
$ cd pintos
```

- Download or copy the contents of [`Dockerfile`](./Dockerfile) available in this repository.

- Build an image from `Dockerfile`. This creates an image named `pintos` with the tag `latest` and installs Pintos base code into Ubuntu 22.04 LTS which is the latest version as of 2025.

```bash
$ docker build -t pintos:latest .
```

### Develop locally, test and run virtually

To make the development process easier, it would be nice to use your favorite code editor and LSP server but installing them inside the image makes it unnecessarily large. To write the code on your local machine and have it immediately sync to the image, follow the instructions below:

- Copy the starter repository already installed in the image to our local file system. To do this, create a temporary container.

```bash
# Creating a stopped container from the image
$ docker create --name pintos_container pintos:latest

# Copy the starter repository to the local file system. Assuming you are currently in $HOME/pintos/ directory
$ cd ../
$ docker cp pintos_container:/home/pintos .

# Delete the tempororay container
$ docker rm pintos_container
```

- Having copied the starter code to the host machine, copy the DESIGNDOCs in [`doc/`](./doc/) folder of this repository to a folder named `doc/` in your local repository. The local repository can now be tracked using `git` and set up for storing in a remote repository.

- To sync the files dynamically into the container when developing in the repository on the host machine, use the following command

```bash
$ cd $HOME/pintos
$ docker run --rm -it -u $(id -u):$(id -g) -v $PWD:/home/pintos -w /home/pintos pintos:latest bash
```

The above command does the following:

- `--rm` creates a temporary container which gets deleted every time we exit it.
- `-it` allows us to interact with the container in real time through `bash` shell.
- `-u $(id -u):$(id -g)` ensures that any process runs inside the container using host user and group. Otherwise, any files created or updated inside the container will be owned by the root user locally and not by **you**.
- `-v $PWD:/home/pintos` mounts your current working directory on the host system to the container at `/home/pintos`, thus overwriting the initially installed `pintos` directory.
- `-w /home/pintos` takes you directly to `/home/pintos` (by defualt you are logged in at `/`).

## References

The `Dockerfile` was created with the help of [Chatgpt](chatgpt.com) (GPT-5 model) after carefully reviewing and understanding the installation instructions from the official Pintos [documenation](https://web.stanford.edu/class/cs140/projects/pintos/pintos_12.html#SEC166). Everything else was developed by me without referring to any external resources other than the official documentaion itself.
