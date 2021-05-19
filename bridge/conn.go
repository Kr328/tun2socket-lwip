package bridge

//#include "tcp.h"
import "C"

import (
	"net"
	"time"
	"unsafe"
)

type conn struct {
	nConn *C.tcp_conn_t
}

func (c *conn) Read(b []byte) (int, error) {
	n := int(C.tcp_conn_read(c.nConn, unsafe.Pointer(&b[0]), C.int(len(b))))
	if n < 0 {
		return 0, ErrNative
	}

	return n, nil
}

func (c *conn) Write(b []byte) (int, error) {
	n := int(C.tcp_conn_write(c.nConn, unsafe.Pointer(&b[0]), C.int(len(b))))
	if n < 0 {
		return 0, ErrNative
	}

	return n, nil
}

func (c *conn) Close() error {
	C.tcp_conn_close(c.nConn)

	return nil
}

func (c *conn) LocalAddr() net.Addr {
	ip := net.IP{0, 0, 0, 0}
	port := C.uint16_t(0)

	if C.tcp_conn_local_addr(c.nConn, (*C.uint8_t)(unsafe.Pointer(&ip[0])), &port) < 0 {
		panic(ErrNative.Error())
	}

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

	if C.tcp_conn_remote_addr(c.nConn, (*C.uint8_t)(unsafe.Pointer(&ip[0])), &port) < 0 {
		panic(ErrNative.Error())
	}

	addr := &net.TCPAddr{
		IP:   ip,
		Port: int(port),
		Zone: "",
	}

	return addr
}

func (c *conn) SetDeadline(t time.Time) error {
	return ErrUnsupported
}

func (c *conn) SetReadDeadline(t time.Time) error {
	return ErrUnsupported
}

func (c *conn) SetWriteDeadline(t time.Time) error {
	return ErrUnsupported
}
