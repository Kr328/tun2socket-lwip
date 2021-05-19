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

type listener struct {
	nListener *C.tcp_listener_t
}

func (l *listener) Accept() (net.Conn, error) {
	nConn := C.tcp_listener_accept(l.nListener)

	if nConn == nil {
		return nil, ErrUnacceptable
	}

	c := &conn{nConn: nConn}

	runtime.SetFinalizer(c, connDestroy)

	return c, nil
}

func (l *listener) Close() error {
	C.tcp_listener_close(l.nListener)

	return nil
}

func (l *listener) Addr() net.Addr {
	return &net.TCPAddr{
		IP:   net.IPv4zero,
		Port: 0,
		Zone: "",
	}
}

func Listen() (net.Listener, error) {
	nListener := C.tcp_listener_listen()
	if nListener == nil {
		return nil, ErrIllegalState
	}

	l := &listener{nListener: nListener}

	runtime.SetFinalizer(l, listenerDestroy)

	return l, nil
}

func listenerDestroy(l *listener) {
	C.tcp_listener_free(l.nListener)
}

func connDestroy(conn *conn) {
	C.tcp_conn_free(conn.nConn)
}
