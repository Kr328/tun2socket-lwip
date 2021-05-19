package bridge

//#include "udp.h"
import "C"

import (
	"net"
	"runtime"
	"unsafe"
)

type PacketConn interface {
	ReadFrom(b []byte) (n int, lAddr, rAddr net.Addr, err error)
	WriteTo(b []byte, lAddr, rAddr net.Addr) (int, error)
	Close() error
}

type packetConn struct {
	conn *C.udp_conn_t
}

func (p *packetConn) ReadFrom(b []byte) (int, net.Addr, net.Addr, error) {
	metadata := C.udp_metadata_t{}

	n := C.udp_conn_recv(p.conn, &metadata, unsafe.Pointer(&b[0]), C.int(len(b)))
	if n < 0 {
		return 0, nil, nil, ErrNative
	}

	lAddr := &net.UDPAddr{
		IP: net.IP{
			byte(metadata.src_addr[0]),
			byte(metadata.src_addr[1]),
			byte(metadata.src_addr[2]),
			byte(metadata.src_addr[3]),
		},
		Port: int(metadata.src_port),
		Zone: "",
	}

	rAddr := &net.UDPAddr{
		IP: net.IP{
			byte(metadata.dst_addr[0]),
			byte(metadata.dst_addr[1]),
			byte(metadata.dst_addr[2]),
			byte(metadata.dst_addr[3]),
		},
		Port: int(metadata.dst_port),
		Zone: "",
	}

	return int(n), lAddr, rAddr, nil
}

func (p *packetConn) WriteTo(b []byte, lAddr, rAddr net.Addr) (int, error) {
	metadata := C.udp_metadata_t{}

	udpLAddr, ok := lAddr.(*net.UDPAddr)
	if !ok {
		return 0, ErrUnsupported
	}

	udpRAddr, ok := rAddr.(*net.UDPAddr)
	if !ok {
		return 0, ErrUnsupported
	}

	lIP := udpLAddr.IP.To4()
	rIP := udpRAddr.IP.To4()

	if lIP == nil || rIP == nil {
		return 0, ErrUnsupported
	}

	metadata.src_addr[0] = C.uint8_t(rIP[0])
	metadata.src_addr[1] = C.uint8_t(rIP[1])
	metadata.src_addr[2] = C.uint8_t(rIP[2])
	metadata.src_addr[3] = C.uint8_t(rIP[3])

	metadata.dst_addr[0] = C.uint8_t(lIP[0])
	metadata.dst_addr[1] = C.uint8_t(lIP[1])
	metadata.dst_addr[2] = C.uint8_t(lIP[2])
	metadata.dst_addr[3] = C.uint8_t(lIP[3])

	metadata.dst_port = C.uint16_t(udpLAddr.Port)
	metadata.src_port = C.uint16_t(udpRAddr.Port)

	n := C.udp_conn_sendto(p.conn, &metadata, unsafe.Pointer(&b[0]), C.int(len(b)))
	if n < 0 {
		return 0, ErrNative
	}

	return int(n), nil
}

func (p *packetConn) Close() error {
	C.udp_conn_close(p.conn)

	return nil
}

func ListenPacket() (PacketConn, error) {
	conn := C.udp_conn_listen()
	if conn == nil {
		return nil, ErrNative
	}

	r := &packetConn{conn: conn}

	runtime.SetFinalizer(r, packetConnDestroy)

	return r, nil
}

func packetConnDestroy(conn *packetConn) {
	C.udp_conn_free(conn.conn)
}
