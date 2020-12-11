FROM envoy

ENV PATH="${PATH}:/usr/local/go/bin:/root/go/bin"

RUN apt update && apt install -y \
    git \
    graphviz \
    wget
RUN wget https://golang.org/dl/go1.15.6.linux-amd64.tar.gz
RUN tar -C /usr/local -xzf go1.15.6.linux-amd64.tar.gz
RUN go get -u github.com/google/pprof

RUN echo "PATH=$PATH:/usr/local/go/bin" > ~/.bashrc
RUN echo "PATH=$PATH:/root/go/bin" > ~/.bashrc


