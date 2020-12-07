package main


import (
	"bufio"
	"flag"
	"fmt"
	"log"
	"net"
	"strings"
)

func handleConnection(conn net.Conn) {
	defer conn.Close()
	scanner := bufio.NewScanner(conn)
	for scanner.Scan() {
		message := scanner.Text()
		newMessage := strings.ToUpper(message)
		conn.Write([]byte(newMessage + "\n"))
	}

	if err := scanner.Err(); err != nil {
		fmt.Println("error:", err)
	}
}

func main() {
	i := flag.String("i", "127.0.0.1", "server ip")
	p := flag.String("p", "80", "server post")
	flag.Parse()

	ip := *i
	port := *p
	ln, err := net.Listen("tcp", ip + ":" + port)
	if err != nil {
		log.Fatal(err)
	}

	for {
		conn, err := ln.Accept()
		if err != nil {
			log.Fatal(err)
		}
		go handleConnection(conn)
	}
}