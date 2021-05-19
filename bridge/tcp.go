package bridge

//#include "tcp.h"
import "C"

import (
	"errors"
	"net"
	"runtime"
)

var ErrUnacceptable = errors.New("unacceptable")
var ErrIllegalState = errors.New("illegal state")

type TCP interface {
	Accept() (net.Conn, error)
	Close() error
}

type tcp struct {
	context *C.tcp_listener_t
}

func (l *tcp) Accept() (net.Conn, error) {
	context := C.tcp_listener_accept(l.context)

	if context == nil {
		return nil, ErrUnacceptable
	}

	return newConn(context), nil
}

func (l *tcp) Close() error {
	C.tcp_listener_close(l.context)

	return nil
}

func (l *tcp) Addr() net.Addr {
	return &net.TCPAddr{
		IP:   net.IPv4zero,
		Port: 0,
		Zone: "",
	}
}

func ListenTCP() (TCP, error) {
	context := C.tcp_listener_listen()
	if context == nil {
		return nil, ErrIllegalState
	}

	l := &tcp{context: context}

	runtime.SetFinalizer(l, tcpDestroy)

	return l, nil
}

func tcpDestroy(l *tcp) {
	C.tcp_listener_free(l.context)
}
