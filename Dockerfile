FROM ubuntu:latest
RUN apt update && apt install -y sudo apt-transport-https curl gnupg git build-essential gcc-10 g++-10&& curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel.gpg && mv bazel.gpg /etc/apt/trusted.gpg.d/ && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100 --slave /usr/bin/g++ g++ /usr/bin/g++-10 --slave /usr/bin/gcov gcov /usr/bin/gcov-10 && apt-get install -y dos2unix
ADD . .
RUN dos2unix ./rocksdb_install.sh
RUN yes | ./rocksdb_install.sh
RUN ./bazelisk build embeddingstore:main
ENV EMBEDDINGHUB_PORT=7462
EXPOSE $EMBEDDINGHUB_PORT
ENTRYPOINT /bin/bash