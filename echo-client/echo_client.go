package main

import (
	"flag"
	"net"
	"time"
	"fmt"
    "math/rand"
    "syscall"
)

func connection(local, ip, port string) {
    rand.Seed(time.Now().UnixNano())
	strAddr := ip + ":" + port

    for {
        time.Sleep(time.Duration(rand.Intn(20)) * time.Second)

        laddr,err := net.ResolveTCPAddr("tcp", local)
        if err != nil {
            fmt.Println("resolve local address failed, ", err.Error())
            continue
        }

        dialer := &net.Dialer {
            LocalAddr: laddr,
            Control: func(network, address string, c syscall.RawConn) error {
                return c.Control(func(fd uintptr) {
                    err = syscall.SetsockoptInt(int(fd), syscall.SOL_IP, syscall.IP_TRANSPARENT, 1)
                    if err != nil {
                        fmt.Println("failed to set IP_TRANSPARENT, ", err.Error())
                        return
                    }
                })
            },
        }


        conn,err := dialer.Dial("tcp", strAddr)
        if err != nil {
            fmt.Println("connect to server failed, ", err.Error())
            continue
        }
        defer conn.Close()


        for {
            _,err := conn.Write([]byte("hello\n"))
            if err != nil {
                fmt.Println("write to server failed, ", err.Error())
                break
            }

            buf := make([]byte, 2000)
            n,err := conn.Read(buf)
            if err != nil {
                fmt.Println("read from server failed, ", err.Error())
                break
            }

            fmt.Println(string(buf[:n]))

            time.Sleep(time.Duration(rand.Intn(20)) * time.Second)
        }
    }
}

func main(){
	c := flag.Int("c", 1, "connections")
	i := flag.String("i", "127.0.0.1", "server ip")
	p := flag.String("p", "80", "server post")
	b := flag.String("b", "0.0.0.0", "bind local ip")
	flag.Parse()

	conns := *c
	ip := *i
	port := *p
    local := *b
	for i:=0; i<conns; i++ {
		go connection(local, ip, port)
		time.Sleep(5 * time.Millisecond)
	}

	for {
		time.Sleep(time.Second)
	}
}
