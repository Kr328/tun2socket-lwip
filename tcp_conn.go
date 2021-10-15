package tun2socket

/*
#cgo CFLAGS: -Inative

#include "tcp.h"
*/
import "C"

import (
	"net"
	"runtime"
	"time"
	"unsafe"
)

type conn struct {
	context *C.tcp_conn_t
}

func (c *conn) Read(b []byte) (int, error) {
	n := int(C.tcp_conn_read(c.context, unsafe.Pointer(&b[:cap(b)][0]), C.int(len(b))))
	if n < 0 {
		return 0, ErrNative
	}

	return n, nil
}

func (c *conn) Write(b []byte) (int, error) {
	n := int(C.tcp_conn_write(c.context, unsafe.Pointer(&b[:cap(b)][0]), C.int(len(b))))
	if n < 0 {
		return 0, ErrNative
	}

	return n, nil
}

func (c *conn) Close() error {
	C.tcp_conn_close(c.context)

	return nil
}

func (c *conn) LocalAddr() net.Addr {
	ip := net.IP{0, 0, 0, 0}
	port := C.uint16_t(0)

	C.tcp_conn_local_addr(c.context, (*C.uint8_t)(unsafe.Pointer(&ip[0])), &port)

	addr := &net.TCPAddr{
		IP:   ip,
		Port: int(port),
		Zone: "",
	}

	return addr
}

func (c *conn) RemoteAddr() net.Addr {
	ip := net.IP{0, 0, 0, 0}
	port := C.uint16_t(0)

	C.tcp_conn_remote_addr(c.context, (*C.uint8_t)(unsafe.Pointer(&ip[0])), &port)

	addr := &net.TCPAddr{
		IP:   ip,
		Port: int(port),
		Zone: "",
	}

	return addr
}

func (c *conn) SetDeadline(t time.Time) error {
	return c.Close()
}

func (c *conn) SetReadDeadline(t time.Time) error {
	return c.SetDeadline(t)
}

func (c *conn) SetWriteDeadline(t time.Time) error {
	return c.SetDeadline(t)
}

func newConn(context *C.tcp_conn_t) *conn {
	c := &conn{context: context}

	runtime.SetFinalizer(c, connDestroy)

	return c
}

func connDestroy(conn *conn) {
	C.tcp_conn_free(conn.context)
}
