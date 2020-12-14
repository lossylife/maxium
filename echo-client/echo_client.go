package main

import (
	"flag"
	"net"
	"time"
	"fmt"
    "math/rand"
)

func connection(ip, port string) {
    rand.Seed(time.Now().UnixNano())
	strAddr := ip + ":" + port

    for {
        time.Sleep(time.Duration(rand.Intn(20)) * time.Second)

        s,err := net.ResolveTCPAddr("tcp", strAddr)
        if err != nil {
            fmt.Println("resolve server address failed, ", err.Error())
            continue
        }
        conn,err := net.DialTCP("tcp", nil, s)
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
	flag.Parse()

	conns := *c
	ip := *i
	port := *p
	for i:=0; i<conns; i++ {
		go connection(ip, port)
		time.Sleep(5 * time.Millisecond)
	}

	for {
		time.Sleep(time.Second)
	}
}
