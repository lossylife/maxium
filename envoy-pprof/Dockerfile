FROM envoy

RUN apt update && apt install -y \
    git \
    graphviz \
    wget
RUN wget https://golang.org/dl/go1.15.6.linux-amd64.tar.gz
RUN tar -C /usr/local -xzf go1.15.6.linux-amd64.tar.gz
RUN export PATH=$PATH:/usr/local/go/bin
RUN go get -u github.com/google/pprof
RUN export PATH=$PATH:/root/go/bin

RUN echo "PATH=$PATH:/usr/local/go/bin" > ~/.bashrc
RUN echo "PATH=$PATH:/root/go/bin" > ~/.bashrc


