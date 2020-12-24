package main

import (
	"flag"
	"net"
	"time"
	"fmt"
    "math/rand"
    "math/big"
    "syscall"
)

func connection(local, ip, port string) {
    rand.Seed(time.Now().UnixNano())
	strRAddr := ip + ":" + port
    strLAddr := local + ":" + "0"

    var conn net.Conn
    for {
        time.Sleep(time.Duration(rand.Intn(20)) * time.Second)

        if local == "0.0.0.0" {
            raddr,err := net.ResolveTCPAddr("tcp", strRAddr)
            if err != nil {
                fmt.Println("resolve remote address failed, ", err.Error())
                continue
            }
            conn,err = net.DialTCP("tcp", nil, raddr)
            if err != nil {
                fmt.Println("connect to server failed, ", err.Error())
                continue
            }
        }else{
            laddr,err := net.ResolveTCPAddr("tcp", strLAddr)
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
            conn,err = dialer.Dial("tcp", strRAddr)
            if err != nil {
                fmt.Println("connect to server failed, ", err.Error())
                continue
            }
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
    m := flag.Int("m", 40000, "how many connections per ip")
	i := flag.String("i", "127.0.0.1", "server ip")
	p := flag.String("p", "80", "server post")
	b := flag.String("b", "0.0.0.0", "local ip base")
    w := flag.Int("w", 1000, "wait ${w} ms after create ${g} connections")
    g := flag.Int("g", 100, "wait ${w} ms after create ${g} connections")
	flag.Parse()

	conns := *c
    many := *m
	ip := *i
	port := *p
    base := *b
    wait := *w
    group := *g
    baseIP := net.ParseIP(base)

    current := 0
    for i:=0; i<=conns/many; i++ {
        ipb := big.NewInt(0).SetBytes([]byte(baseIP))
        ipb.Add(ipb, big.NewInt(int64(i)))
        b := ipb.Bytes()
        b = append(make([]byte, len(baseIP)-len(b)), b...)
        localIp := net.IP(b).String()

        bucket := many
        if bucket > conns - i * many {
            bucket = conns - i * many
        }
        for j:=0; j<bucket; j++ {
            if true {
                go connection(localIp, ip, port)
                current += 1
                if current % group == 0 {
                    time.Sleep(time.Duration(wait) * time.Millisecond)
                }
            }
        }
        fmt.Printf("localIp: %s, bucket: %d, current: %d\n", localIp, bucket, current)
    }

	for {
		time.Sleep(time.Second)
	}
}

